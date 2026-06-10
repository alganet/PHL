# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6541/8388 lines (77.98%)

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
|        - |   136 |  |
|        - |   137 | `/*` |
|        - |   138 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   139 | ` */` |
|   914358 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   914360 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   914326 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   914316 |   148 | `	return FALSE;` |
|   457203 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335376 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335378 |   162 | `	sxu8 bReal = FALSE;` |
|   335378 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335378 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335308 |   166 | `		return FALSE;` |
|        - |   167 | `	}` |
|       71 |   168 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       71 |   169 | `	if( sStr.nByte == 0 ){` |
|        5 |   170 | `		return TRUE;` |
|        - |   171 | `	}` |
|       67 |   172 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       47 |   173 | `		return TRUE;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   176 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   177 | `	 * still takes the Perl path. */` |
|       21 |   178 | `	zEnd = sStr.zString + sStr.nByte;` |
|       25 |   179 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   180 | `		zTail++;` |
|        1 |   181 | `	}` |
|       21 |   182 | `	return zTail < zEnd;` |
|   167712 |   183 |  |
|        - |   184 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   185 | `/*` |
|        - |   186 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   187 | ` * it can be expanded from the target PHP program.` |
|        - |   188 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   189 | ` * simple and work as follows:` |
|        - |   190 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   191 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   192 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   193 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   194 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   195 | ` * (Windows,Linux,...) and so on.` |
|        - |   196 | ` * Please refer to the official documentation for additional information.` |
|        - |   197 | ` */` |
|   631690 |   198 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   199 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   200 | `	const SyString *pName,  /* Constant name */` |
|        - |   201 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   202 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   203 | `	)` |
|        2 |   204 |  |
|        - |   205 | `	ph7_constant *pCons;` |
|        - |   206 | `	SyHashEntry *pEntry;` |
|        - |   207 | `	char *zDupName;` |
|        - |   208 | `	sxi32 rc;` |
|   631692 |   209 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   631692 |   210 | `	if( pEntry ){` |
|        - |   211 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   212 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   213 | `		pCons->xExpand = xExpand;` |
|        6 |   214 | `		pCons->pUserData = pUserData;` |
|        6 |   215 | `		return SXRET_OK;` |
|        - |   216 | `	}` |
|        - |   217 | `	/* Allocate a new constant instance */` |
|   631688 |   218 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   631688 |   219 | `	if( pCons == 0 ){` |
|      ! 0 |   220 | `		return 0;` |
|        - |   221 | `	}` |
|        - |   222 | `	/* Duplicate constant name */` |
|   631688 |   223 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   631688 |   224 | `	if( zDupName == 0 ){` |
|      ! 0 |   225 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   226 | `		return 0;` |
|        - |   227 | `	}` |
|        - |   228 | `	/* Install the constant */` |
|   631688 |   229 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   631688 |   230 | `	pCons->xExpand = xExpand;` |
|   631688 |   231 | `	pCons->pUserData = pUserData;` |
|   631688 |   232 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   631688 |   233 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   234 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   235 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   236 | `		return rc;` |
|        - |   237 | `	}` |
|        - |   238 | `	/* All done,constant can be invoked from PHP code */` |
|   631688 |   239 | `	return SXRET_OK;` |
|   315847 |   240 |  |
|        - |   241 | `/*` |
|        - |   242 | ` * Allocate a new foreign function instance.` |
|        - |   243 | ` * This function return SXRET_OK on success. Any other` |
|        - |   244 | ` * return value indicates failure.` |
|        - |   245 | ` * Please refer to the official documentation for an introduction to` |
|        - |   246 | ` * the foreign function mechanism.` |
|        - |   247 | ` */` |
|  1382126 |   248 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   249 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   250 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   251 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   252 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   253 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   254 | `	)` |
|        2 |   255 |  |
|        - |   256 | `	ph7_user_func *pFunc;` |
|        - |   257 | `	char *zDup;` |
|        - |   258 | `	/* Allocate a new user function */` |
|  1382128 |   259 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1382128 |   260 | `	if( pFunc == 0 ){` |
|      ! 0 |   261 | `		return SXERR_MEM;` |
|        - |   262 | `	}` |
|        - |   263 | `	/* Duplicate function name */` |
|  1382128 |   264 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1382128 |   265 | `	if( zDup == 0 ){` |
|      ! 0 |   266 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   267 | `		return SXERR_MEM;` |
|        - |   268 | `	}` |
|        - |   269 | `	/* Zero the structure */` |
|  1382128 |   270 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   271 | `	/* Initialize structure fields */` |
|  1382128 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1382128 |   273 | `	pFunc->pVm   = pVm;` |
|  1382128 |   274 | `	pFunc->xFunc = xFunc;` |
|  1382128 |   275 | `	pFunc->pUserData = pUserData;` |
|  1382128 |   276 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   277 | `	/* Write a pointer to the new function */` |
|  1382128 |   278 | `	*ppOut = pFunc;` |
|  1382128 |   279 | `	return SXRET_OK;` |
|   691065 |   280 |  |
|        - |   281 | `/*` |
|        - |   282 | ` * Install a foreign function and it's associated callback so that` |
|        - |   283 | ` * it can be invoked from the target PHP code.` |
|        - |   284 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   285 | ` * return value indicates failure.` |
|        - |   286 | ` * Please refer to the official documentation for an introduction to` |
|        - |   287 | ` * the foreign function mechanism.` |
|        - |   288 | ` */` |
|  1384946 |   289 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   290 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   291 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   292 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   293 | `	void *pUserData           /* Foreign function private data */` |
|        - |   294 | `	)` |
|        2 |   295 |  |
|        - |   296 | `	ph7_user_func *pFunc;` |
|        - |   297 | `	SyHashEntry *pEntry;` |
|        - |   298 | `	sxi32 rc;` |
|        - |   299 | `	/* Overwrite any previously registered function with the same name */` |
|  1384948 |   300 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1384948 |   301 | `	if( pEntry ){` |
|     2822 |   302 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2822 |   303 | `		pFunc->pUserData = pUserData;` |
|     2822 |   304 | `		pFunc->xFunc = xFunc;` |
|     2822 |   305 | `		SySetReset(&pFunc->aAux);` |
|     2822 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* Create a new user function */` |
|  1382128 |   309 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1382128 |   310 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   311 | `		return rc;` |
|        - |   312 | `	}` |
|        - |   313 | `	/* Install the function in the corresponding hashtable */` |
|  1382128 |   314 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1382128 |   315 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   316 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   317 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   318 | `		return rc;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* User function successfully installed */` |
|  1382128 |   321 | `	return SXRET_OK;` |
|   692475 |   322 |  |
|        - |   323 | `/*` |
|        - |   324 | ` * Initialize a VM function.` |
|        - |   325 | ` */` |
|   274556 |   326 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   327 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   328 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   329 | `	const char *zName,  /* Function name */` |
|        - |   330 | `	sxu32 nByte,        /* zName length */` |
|        - |   331 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   332 | `	void *pUserData     /* Function private data */` |
|        - |   333 | `	)` |
|        2 |   334 |  |
|        - |   335 | `	/* Zero the structure */` |
|   274558 |   336 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   337 | `	/* Initialize structure fields */` |
|        - |   338 | `	/* Arguments container */` |
|   274558 |   339 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   340 | `	/* Static variable container */` |
|   274558 |   341 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   342 | `	/* Bytecode container */` |
|   274558 |   343 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   344 | `    /* Preallocate some instruction slots */` |
|   274558 |   345 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   346 | `	/* Closure environment */` |
|   274558 |   347 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   348 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   274558 |   349 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   274558 |   350 | `	pFunc->iFlags = iFlags;` |
|   274558 |   351 | `	pFunc->pUserData = pUserData;` |
|        - |   352 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   353 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   274558 |   354 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   274558 |   355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   274558 |   356 | `	return SXRET_OK;` |
|        2 |   357 |  |
|        - |   358 | `/*` |
|        - |   359 | ` * Namespace-aware function lookup.` |
|        - |   360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   362 | ` */` |
|        - |   363 | `/*` |
|        - |   364 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   365 | ` */` |
|   757578 |   366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   367 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   368 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   369 | `	SyString *pName     /* Function name */` |
|        - |   370 | `	)` |
|        2 |   371 |  |
|        - |   372 | `	SyHashEntry *pEntry;` |
|        - |   373 | `	sxi32 rc;` |
|   757580 |   374 | `	if( pName == 0 ){` |
|        - |   375 | `		/* Use the built-in name */` |
|    41802 |   376 | `		pName = &pFunc->sName;` |
|    20900 |   377 | `	}` |
|        - |   378 | `	/* Check for duplicates (functions with the same name) first */` |
|   757580 |   379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   757580 |   380 | `	if( pEntry ){` |
|   561664 |   381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   561664 |   382 | `		if( pLink != pFunc ){` |
|        - |   383 | `			/* Link */` |
|      188 |   384 | `			pFunc->pNextName = pLink;` |
|      188 |   385 | `			pEntry->pUserData = pFunc;` |
|       93 |   386 | `		}` |
|   561664 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|        - |   389 | `	/* First time seen */` |
|   195918 |   390 | `	pFunc->pNextName = 0;` |
|   195918 |   391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   195918 |   392 | `	return rc;` |
|   378791 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   396 | ` */` |
|    76212 |   397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   398 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   399 | `	ph7_class *pClass /* Target Class */` |
|        - |   400 | `	)` |
|        2 |   401 |  |
|    76214 |   402 | `	SyString *pName = &pClass->sName;` |
|        - |   403 | `	SyHashEntry *pEntry;` |
|        - |   404 | `	sxi32 rc;` |
|        - |   405 | `	/* Check for duplicates */` |
|    76214 |   406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    76214 |   407 | `	if( pEntry ){` |
|       31 |   408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   409 | `		/* Link entry with the same name */` |
|       31 |   410 | `		pClass->pNextName = pLink;` |
|       31 |   411 | `		pEntry->pUserData = pClass;` |
|       31 |   412 | `		return SXRET_OK;` |
|        - |   413 | `	}` |
|    76184 |   414 | `	pClass->pNextName = 0;` |
|        - |   415 | `	/* Perform a simple hashtable insertion */` |
|    76184 |   416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    76184 |   417 | `	return rc;` |
|    38108 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Instruction builder interface.` |
|        - |   421 | ` */` |
|  4247320 |   422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   423 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   424 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   425 | `	sxi32 iP1,    /* First operand */` |
|        - |   426 | `	sxu32 iP2,    /* Second operand */` |
|        - |   427 | `	void *p3,     /* Third operand */` |
|        - |   428 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   429 | `	)` |
|        2 |   430 |  |
|        - |   431 | `	VmInstr sInstr;` |
|        - |   432 | `	sxi32 rc;` |
|        - |   433 | `	/* Fill the VM instruction */` |
|  4247322 |   434 | `	sInstr.iOp = (sxu8)iOp;` |
|  4247322 |   435 | `	sInstr.iP1 = iP1;` |
|  4247322 |   436 | `	sInstr.iP2 = iP2;` |
|  4247322 |   437 | `	sInstr.p3  = p3;` |
|  4247322 |   438 | `	if( pIndex ){` |
|        - |   439 | `		/* Instruction index in the bytecode array */` |
|   230666 |   440 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115332 |   441 | `	}` |
|        - |   442 | `	/* Finally,record the instruction */` |
|  4247322 |   443 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4247322 |   444 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   445 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   446 | `		/* Fall throw */` |
|      ! 0 |   447 | `	}` |
|  4247322 |   448 | `	return rc;` |
|        2 |   449 |  |
|        - |   450 | `/*` |
|        - |   451 | ` * Swap the current bytecode container with the given one.` |
|        - |   452 | ` */` |
|   551460 |   453 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   454 |  |
|   551462 |   455 | `	if( pContainer == 0 ){` |
|        - |   456 | `		/* Point to the default container */` |
|      ! 0 |   457 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   458 | `	}else{` |
|        - |   459 | `		/* Change container */` |
|   551462 |   460 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   461 | `	}` |
|   551462 |   462 | `	return SXRET_OK;` |
|        2 |   463 |  |
|        - |   464 | `/*` |
|        - |   465 | ` * Return the current bytecode container.` |
|        - |   466 | ` */` |
|   275730 |   467 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   468 |  |
|   275732 |   469 | `	return pVm->pByteContainer;` |
|        2 |   470 |  |
|        - |   471 | `/*` |
|        - |   472 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   473 | ` */` |
|   227454 |   474 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   475 |  |
|        - |   476 | `	VmInstr *pInstr;` |
|   227456 |   477 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   227456 |   478 | `	return pInstr;` |
|        2 |   479 |  |
|        - |   480 | `/*` |
|        - |   481 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   482 | ` */` |
|  1276224 |   483 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   484 |  |
|  1276226 |   485 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Pop the last VM instruction.` |
|        - |   489 | ` */` |
|   210452 |   490 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   491 |  |
|   210454 |   492 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   493 |  |
|        - |   494 | `/*` |
|        - |   495 | ` * Peek the last VM instruction.` |
|        - |   496 | ` */` |
|   836564 |   497 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   498 |  |
|   836566 |   499 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   500 |  |
|    33320 |   501 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   502 |  |
|        - |   503 | `	VmInstr *aInstr;` |
|        - |   504 | `	sxu32 n;` |
|    33322 |   505 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33322 |   506 | `	if( n < 2 ){` |
|      ! 0 |   507 | `		return 0;` |
|        - |   508 | `	}` |
|    33322 |   509 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33322 |   510 | `	return &aInstr[n - 2];` |
|    16662 |   511 |  |
|        - |   512 | `/*` |
|        - |   513 | ` * Allocate a new virtual machine frame.` |
|        - |   514 | ` */` |
|    22216 |   515 | `static VmFrame * VmNewFrame(` |
|        - |   516 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   517 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   518 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   519 | `	)` |
|        2 |   520 |  |
|        - |   521 | `	VmFrame *pFrame;` |
|        - |   522 | `	/* Allocate a new vm frame */` |
|    22218 |   523 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22218 |   524 | `	if( pFrame == 0 ){` |
|      ! 0 |   525 | `		return 0;` |
|        - |   526 | `	}` |
|        - |   527 | `	/* Zero the structure */` |
|    22218 |   528 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   529 | `	/* Initialize frame fields */` |
|    22218 |   530 | `	pFrame->pUserData = pUserData;` |
|    22218 |   531 | `	pFrame->pThis = pThis;` |
|    22218 |   532 | `	pFrame->pVm = pVm;` |
|    22218 |   533 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22218 |   534 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22218 |   535 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22218 |   536 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22218 |   537 | `	return pFrame;` |
|    11110 |   538 |  |
|        - |   539 | `/* Forward declaration */` |
|        - |   540 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   541 | `/*` |
|        - |   542 | ` * Enter a VM frame.` |
|        - |   543 | ` */` |
|    22170 |   544 | `static sxi32 VmEnterFrame(` |
|        - |   545 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   546 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   547 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   548 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   549 | `	)` |
|        2 |   550 |  |
|        - |   551 | `	VmFrame *pFrame;` |
|        - |   552 | `	/* Allocate a new frame */` |
|    22172 |   553 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22172 |   554 | `	if( pFrame == 0 ){` |
|      ! 0 |   555 | `		return SXERR_MEM;` |
|        - |   556 | `	}` |
|        - |   557 | `	/* Link to the list of active VM frame */` |
|    22172 |   558 | `	pFrame->pParent = pVm->pFrame;` |
|    22172 |   559 | `	pVm->pFrame = pFrame;` |
|    22172 |   560 | `	if( ppFrame ){` |
|        - |   561 | `		/* Write a pointer to the new VM frame */` |
|    19038 |   562 | `		*ppFrame = pFrame;` |
|     9518 |   563 | `	}` |
|    22172 |   564 | `	return SXRET_OK;` |
|    11087 |   565 |  |
|        - |   566 | `/*` |
|        - |   567 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   568 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   569 | ` * information.` |
|        - |   570 | ` */` |
|       70 |   571 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   572 |  |
|        - |   573 | `	VmFrame *pTarget,*pFrame;` |
|       72 |   574 | `	SyHashEntry *pEntry = 0;` |
|        - |   575 | `	sxi32 rc;` |
|        - |   576 | `	/* Point to the upper frame */` |
|       72 |   577 | `	pFrame = pVm->pFrame;` |
|       72 |   578 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       72 |   579 | `	pTarget = pFrame;` |
|       72 |   580 | `	pFrame = pTarget->pParent;` |
|       72 |   581 | `	while( pFrame ){` |
|       72 |   582 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   583 | `			/* Query the current frame */` |
|       72 |   584 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       72 |   585 | `			if( pEntry ){` |
|        - |   586 | `				/* Variable found */` |
|       72 |   587 | `				break;` |
|        - |   588 | `			}` |
|      ! 0 |   589 | `		}` |
|        - |   590 | `		/* Point to the upper frame */` |
|      ! 0 |   591 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   592 | `	}` |
|       72 |   593 | `	if( pEntry == 0 ){` |
|        - |   594 | `		/* Inexistant variable */` |
|      ! 0 |   595 | `		return SXERR_NOTFOUND;` |
|        - |   596 | `	}` |
|        - |   597 | `	/* Link to the current frame */` |
|       72 |   598 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       72 |   599 | `	if( rc == SXRET_OK ){` |
|        - |   600 | `		sxu32 nIdx;` |
|       72 |   601 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       72 |   602 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       35 |   603 | `	}` |
|       72 |   604 | `	return rc;` |
|       37 |   605 |  |
|        - |   606 | `/*` |
|        - |   607 | ` * Leave the top-most active frame.` |
|        - |   608 | ` */` |
|    19026 |   609 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   610 |  |
|    19028 |   611 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19028 |   612 | `	if( pCurFrame ){` |
|        - |   613 | `		/* Unlink from the list of active VM frame */` |
|    19028 |   614 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19028 |   615 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   616 | `			VmSlot  *aSlot;` |
|        - |   617 | `			sxu32 n;` |
|        - |   618 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18672 |   619 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   123358 |   620 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   621 | `				/* Unset the local variable */` |
|   104688 |   622 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52345 |   623 | `			}` |
|        - |   624 | `			/* Remove local reference */` |
|    18672 |   625 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   123432 |   626 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   104762 |   627 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52382 |   628 | `			}` |
|     9335 |   629 | `		}` |
|        - |   630 | `		/* Release internal containers */` |
|    19028 |   631 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19028 |   632 | `		SySetRelease(&pCurFrame->sArg);` |
|    19028 |   633 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19028 |   634 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   635 | `		/* Release the whole structure */` |
|    19028 |   636 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9513 |   637 | `	}` |
|    19028 |   638 |  |
|        - |   639 | `/*` |
|        - |   640 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   641 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   642 | ` * should be skipped when looking for the real execution context.` |
|        - |   643 | ` */` |
|  7089706 |   644 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   645 |  |
|  7091856 |   646 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2150 |   647 | `		pFrame = pFrame->pParent;` |
|        2 |   648 | `	}` |
|  7089708 |   649 | `	return pFrame;` |
|        2 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * After a callee invoked from an OP_CALL site (object __invoke, an array` |
|        - |   653 | ` * callable, or a NEW constructor) returns PH7_EXCEPTION, decide how the current` |
|        - |   654 | ` * frame unwinds. The catch body, if any, already ran in-place inside` |
|        - |   655 | ` * VmThrowException. Returns TRUE and sets *pResumePc to the post-try resume` |
|        - |   656 | ` * point when THIS frame's own try caught the exception; returns FALSE to signal` |
|        - |   657 | ` * the caller to propagate (goto Exception) so the exception unwinds through` |
|        - |   658 | ` * intermediate frames that have no local handler.` |
|        - |   659 | ` */` |
|       12 |   660 | `static int VmCalleeExceptionResume(ph7_vm *pVm,sxi32 *pResumePc)` |
|        1 |   661 |  |
|       13 |   662 | `	VmFrame *pFrame = pVm->pFrame;` |
|       12 |   663 | `	if( (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0` |
|       11 |   664 | `	 && !(pFrame->iFlags & VM_FRAME_THROW) ){` |
|       11 |   665 | `		*pResumePc = (sxi32)pFrame->iExceptionJump - 1;` |
|       11 |   666 | `		return TRUE;` |
|        - |   667 | `	}` |
|        3 |   668 | `	return FALSE;` |
|        7 |   669 |  |
|        - |   670 | `/*` |
|        - |   671 | ` * Compare two functions signature and return the comparison result.` |
|        - |   672 | ` */` |
|      836 |   673 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   674 |  |
|      837 |   675 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   676 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   677 | `	const char *zSin = pSecond->zString;` |
|      837 |   678 | `	const char *zFin = pFirst->zString;` |
|      837 |   679 | `	const char *zPtr = zFin;` |
|      421 |   680 | `	for(;;){` |
|      843 |   681 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   682 | `			break;` |
|        - |   683 | `		}` |
|       19 |   684 | `		if( zFin[0] != zSin[0] ){` |
|        - |   685 | `			/* mismatch */` |
|       13 |   686 | `			break;` |
|        - |   687 | `		}` |
|        7 |   688 | `		zFin++;` |
|        7 |   689 | `		zSin++;` |
|        1 |   690 | `	}` |
|      837 |   691 | `	return (int)(zFin-zPtr);` |
|        1 |   692 |  |
|        - |   693 | `/*` |
|        - |   694 | ` * Select the appropriate VM function for the current call context.` |
|        - |   695 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   696 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   697 | ` * Refer to the official documentation for more information.` |
|        - |   698 | ` */` |
|      138 |   699 | `static ph7_vm_func * VmOverload(` |
|        - |   700 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   701 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   702 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   703 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   704 | `	)` |
|        2 |   705 |  |
|        - |   706 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   707 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   708 | `	ph7_vm_func *pLink;` |
|        - |   709 | `	SyString sArgSig;` |
|        - |   710 | `	SyBlob sSig;` |
|        - |   711 |  |
|      140 |   712 | `	pLink = pList;` |
|      140 |   713 | `	i = 0;` |
|        - |   714 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   715 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   716 | `		if( pLink == 0 ){` |
|       78 |   717 | `			break;` |
|        - |   718 | `		}` |
|      948 |   719 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   720 | `			/* Candidate for overloading */` |
|      902 |   721 | `			apSet[i++] = pLink;` |
|      450 |   722 | `		}` |
|        - |   723 | `		/* Point to the next entry */` |
|      948 |   724 | `		pLink = pLink->pNextName;` |
|        2 |   725 | `	}` |
|      140 |   726 | `	if( i < 1 ){` |
|        - |   727 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   728 | `		return pList;` |
|        - |   729 | `	}` |
|      140 |   730 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   731 | `		/* Return the only candidate */` |
|       32 |   732 | `		return apSet[0];` |
|        - |   733 | `	}` |
|        - |   734 | `	/* Calculate function signature */` |
|      109 |   735 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   736 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   737 | `		int c = 'n'; /* null */` |
|      259 |   738 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   739 | `			/* Hashmap */` |
|       45 |   740 | `			c = 'h';` |
|      237 |   741 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   742 | `			/* bool */` |
|      ! 0 |   743 | `			c = 'b';` |
|      215 |   744 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   745 | `			/* int */` |
|        7 |   746 | `			c = 'i';` |
|      212 |   747 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   748 | `			/* String */` |
|      107 |   749 | `			c = 's';` |
|      156 |   750 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   751 | `			/* Float */` |
|      ! 0 |   752 | `			c = 'f';` |
|      103 |   753 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   754 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   755 | `			int marker = 'o';` |
|        3 |   756 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   757 | `			SyString *pName = &pClass->sName;` |
|        3 |   758 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   759 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   760 | `			c = -1;` |
|        1 |   761 | `		}` |
|      259 |   762 | `		if( c > 0 ){` |
|      257 |   763 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   764 | `		}` |
|      130 |   765 | `	}` |
|      109 |   766 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   767 | `	iTarget = 0;` |
|      109 |   768 | `	iMax = -1;` |
|        - |   769 | `	/* Select the appropriate function */` |
|      945 |   770 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   771 | `		/* Compare the two signatures */` |
|      837 |   772 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   773 | `		if( iCur > iMax ){` |
|      113 |   774 | `			iMax = iCur;` |
|      113 |   775 | `			iTarget = j;` |
|       56 |   776 | `		}` |
|      419 |   777 | `	}` |
|      109 |   778 | `	SyBlobRelease(&sSig);` |
|        - |   779 | `	/* Appropriate function for the current call context */` |
|      109 |   780 | `	return apSet[iTarget];` |
|       71 |   781 |  |
|        - |   782 | `/* Forward declaration */` |
|        - |   783 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   784 | `/*` |
|        - |   785 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   786 | ` * it can be instanciated from the executed PHP script.` |
|        - |   787 | ` */` |
|   259218 |   788 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   789 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   790 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   791 | `	)` |
|        2 |   792 |  |
|        - |   793 | `	ph7_class_method *pMeth;` |
|        - |   794 | `	ph7_class_attr *pAttr;` |
|        - |   795 | `	SyHashEntry *pEntry;` |
|        - |   796 | `	sxi32 rc;` |
|        - |   797 | `	/* Reset the loop cursor */` |
|   259220 |   798 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   799 | `	/* Process only static and constant attribute */` |
|   791399 |   800 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   801 | `		/* Extract the current attribute */` |
|   402572 |   802 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   402572 |   803 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   804 | `			ph7_value *pMemObj;` |
|        - |   805 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1822 |   806 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1822 |   807 | `			if( pMemObj == 0 ){` |
|      ! 0 |   808 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   809 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   810 | `					&pClass->sName,&pAttr->sName` |
|        - |   811 | `					);` |
|      ! 0 |   812 | `				return SXERR_MEM;` |
|        - |   813 | `			}` |
|     1822 |   814 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   815 | `				/* Initialize attribute default value (any complex expression) */` |
|     1818 |   816 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      908 |   817 | `			}` |
|        - |   818 | `			/* Record attribute index */` |
|     1822 |   819 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   820 | `			/* Install static attribute in the reference table */` |
|     1822 |   821 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   822 | `			/* If this is a typed static property, register the slot so the` |
|        - |   823 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   824 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   825 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1822 |   826 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   827 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   828 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   829 | `					return SXERR_MEM;` |
|        - |   830 | `				}` |
|       10 |   831 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   832 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   833 | `				pVmAttrS->iState = 0;` |
|       10 |   834 | `				pVmAttrS->pOwner = pClass;` |
|        - |   835 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   836 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   837 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   838 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   839 | `				}` |
|       10 |   840 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   841 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   842 | `					return SXERR_MEM;` |
|        - |   843 | `				}` |
|        4 |   844 | `			}` |
|      910 |   845 | `		}` |
|        2 |   846 | `	}` |
|        - |   847 | `	/* Install class methods */` |
|   259220 |   848 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   849 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   850 | `		 */` |
|   175028 |   851 | `		return SXRET_OK;` |
|        - |   852 | `	}` |
|        - |   853 | `	/* Create constructor alias if not yet done */` |
|    84194 |   854 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   855 | `		/* User constructor with the same base class name */` |
|     6646 |   856 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6646 |   857 | `		if( pEntry ){` |
|      ! 0 |   858 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   859 | `			/* Create the alias */` |
|      ! 0 |   860 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   861 | `		}` |
|     3322 |   862 | `	}` |
|        - |   863 | `	/* Install the methods now */` |
|    84194 |   864 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   842076 |   865 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   715788 |   866 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   715788 |   867 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   715780 |   868 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   715780 |   869 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   870 | `				return rc;` |
|        - |   871 | `			}` |
|   357889 |   872 | `		}` |
|        2 |   873 | `	}` |
|        - |   874 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    84194 |   875 | `	pClass->bMounted = TRUE;` |
|    84194 |   876 | `	return SXRET_OK;` |
|   129611 |   877 |  |
|        - |   878 | `/*` |
|        - |   879 | ` * Allocate a private frame for attributes of the given` |
|        - |   880 | ` * class instance (Object in the PHP jargon).` |
|        - |   881 | ` */` |
|     2048 |   882 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   883 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   884 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   885 | `	)` |
|        2 |   886 |  |
|     2050 |   887 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   888 | `	ph7_class_attr *pAttr;` |
|        - |   889 | `	SyHashEntry *pEntry;` |
|        - |   890 | `	sxi32 rc;` |
|        - |   891 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2050 |   892 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8458 |   893 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   894 | `		VmClassAttr *pVmAttr;` |
|        - |   895 | `		/* Extract the current attribute */` |
|     6410 |   896 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6410 |   897 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6410 |   898 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   899 | `			return SXERR_MEM;` |
|        - |   900 | `		}` |
|     6410 |   901 | `		pVmAttr->pAttr = pAttr;` |
|     6410 |   902 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   903 | `			ph7_value *pMemObj;` |
|        - |   904 | `			/* Reserve a memory object for this attribute */` |
|     6384 |   905 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6384 |   906 | `			if( pMemObj == 0 ){` |
|      ! 0 |   907 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   908 | `				return SXERR_MEM;` |
|        - |   909 | `			}` |
|     6384 |   910 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6384 |   911 | `			pVmAttr->iState = 0;` |
|     6384 |   912 | `			pVmAttr->pOwner = pClass;` |
|     6384 |   913 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   914 | `				/* Initialize attribute default value (any complex expression) */` |
|     2198 |   915 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5286 |   916 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   917 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   918 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   919 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   920 | `			}` |
|     6384 |   921 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6384 |   922 | `			if( rc != SXRET_OK ){` |
|        - |   923 | `				VmSlot sSlot;` |
|        - |   924 | `				/* Restore memory object */` |
|      ! 0 |   925 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   926 | `				sSlot.pUserData = 0;` |
|      ! 0 |   927 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   928 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   929 | `				return SXERR_MEM;` |
|        - |   930 | `			}` |
|        - |   931 | `			/* Install attribute in the reference table */` |
|     6384 |   932 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   933 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   934 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   935 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6384 |   936 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      170 |   937 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      170 |   938 | `				if( rc != SXRET_OK ){` |
|        - |   939 | `					VmSlot sSlot;` |
|      ! 0 |   940 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   941 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   942 | `					sSlot.pUserData = 0;` |
|      ! 0 |   943 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   944 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   945 | `					return SXERR_MEM;` |
|        - |   946 | `				}` |
|       84 |   947 | `			}` |
|     3193 |   948 | `		}else{` |
|        - |   949 | `			/* Install static/constant attribute */` |
|       28 |   950 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       28 |   951 | `			pVmAttr->iState = 0;` |
|       28 |   952 | `			pVmAttr->pOwner = pClass;` |
|       28 |   953 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       28 |   954 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   955 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   956 | `				return SXERR_MEM;` |
|        - |   957 | `			}` |
|        - |   958 | `		}` |
|        2 |   959 | `	}` |
|     2050 |   960 | `	return SXRET_OK;` |
|     1026 |   961 |  |
|        - |   962 | `/* Forward declaration */` |
|        - |   963 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   964 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   965 | `/*` |
|        - |   966 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   967 | ` */` |
|        - |   968 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   969 | `/*` |
|        - |   970 | ` * Reserve a constant memory object.` |
|        - |   971 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   972 | ` */` |
|   454458 |   973 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   974 |  |
|        - |   975 | `	ph7_value *pObj;` |
|        - |   976 | `	sxi32 rc;` |
|   454460 |   977 | `	if( pIndex ){` |
|        - |   978 | `		/* Object index in the object table */` |
|   445058 |   979 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   222528 |   980 | `	}` |
|        - |   981 | `	/* Reserve a slot for the new object */` |
|   454460 |   982 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   454460 |   983 | `	if( rc != SXRET_OK ){` |
|        - |   984 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   985 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   986 | `		 */` |
|      ! 0 |   987 | `		return 0;` |
|        - |   988 | `	}` |
|   454460 |   989 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   454460 |   990 | `	return pObj;` |
|   227231 |   991 |  |
|        - |   992 | `/*` |
|        - |   993 | ` * Reserve a memory object.` |
|        - |   994 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   995 | ` */` |
|  2151596 |   996 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   997 |  |
|        - |   998 | `	ph7_value *pObj;` |
|        - |   999 | `	sxi32 rc;` |
|  2151598 |  1000 | `	if( pIndex ){` |
|        - |  1001 | `		/* Object index in the object table */` |
|  2151598 |  1002 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075798 |  1003 | `	}` |
|        - |  1004 | `	/* Reserve a slot for the new object */` |
|  2151598 |  1005 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151598 |  1006 | `	if( rc != SXRET_OK ){` |
|        - |  1007 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1008 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1009 | `		 */` |
|      ! 0 |  1010 | `		return 0;` |
|        - |  1011 | `	}` |
|  2151598 |  1012 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151598 |  1013 | `	return pObj;` |
|  1075800 |  1014 |  |
|        - |  1015 | `/* Forward declaration */` |
|        - |  1016 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1017 | `/* Forward declarations for Fiber C functions */` |
|        - |  1018 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1019 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1020 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1021 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1022 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1023 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1024 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1025 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1026 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1027 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1028 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1029 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1030 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1031 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1032 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1033 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1034 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1035 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1036 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1037 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1038 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1039 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1040 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1041 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1042 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1043 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1044 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1045 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1046 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1047 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1048 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1049 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1050 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1051 | `/*` |
|        - |  1052 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1053 | ` * directly as foreign functions.` |
|        - |  1054 | ` */` |
|        - |  1055 | `#define PH7_BUILTIN_LIB \` |
|        - |  1056 | `	"interface Throwable {"\` |
|        - |  1057 | `	"public function getMessage();"\` |
|        - |  1058 | `	"public function getCode();"\` |
|        - |  1059 | `	"public function getFile();"\` |
|        - |  1060 | `	"public function getLine();"\` |
|        - |  1061 | `	"public function getTrace();"\` |
|        - |  1062 | `	"public function getTraceAsString();"\` |
|        - |  1063 | `	"public function getPrevious();"\` |
|        - |  1064 | `	"public function __toString();"\` |
|        - |  1065 | `	"}"\` |
|        - |  1066 | `	"interface Traversable {}"\` |
|        - |  1067 | `	"interface ArrayAccess {"\` |
|        - |  1068 | `	"public function offsetExists($offset);"\` |
|        - |  1069 | `	"public function offsetGet($offset);"\` |
|        - |  1070 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1071 | `	"public function offsetUnset($offset);"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"interface Countable {"\` |
|        - |  1074 | `	"public function count();"\` |
|        - |  1075 | `	"}"\` |
|        - |  1076 | `	"interface Stringable {"\` |
|        - |  1077 | `	"public function __toString();"\` |
|        - |  1078 | `	"}"\` |
|        - |  1079 | `	"interface UnitEnum {"\` |
|        - |  1080 | `	"public static function cases();"\` |
|        - |  1081 | `	"}"\` |
|        - |  1082 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1083 | `	"public static function from($value);"\` |
|        - |  1084 | `	"public static function tryFrom($value);"\` |
|        - |  1085 | `	"}"\` |
|        - |  1086 | `	"class Exception implements Throwable { "\` |
|        - |  1087 | `    "protected $message = '';"\` |
|        - |  1088 | `    "protected $code = 0;"\` |
|        - |  1089 | `    "protected $file;"\` |
|        - |  1090 | `    "protected $line;"\` |
|        - |  1091 | `    "protected $trace;"\` |
|        - |  1092 | `    "protected $previous;"\` |
|        - |  1093 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1094 | `	"   if( isset($message) ){"\` |
|        - |  1095 | `	"	  $this->message = $message;"\` |
|        - |  1096 | `	"   }"\` |
|        - |  1097 | `	"   $this->code = $code;"\` |
|        - |  1098 | `	"   $this->file = __FILE__;"\` |
|        - |  1099 | `	"   $this->line = __LINE__;"\` |
|        - |  1100 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1101 | `	"   if( isset($previous) ){"\` |
|        - |  1102 | `	"     $this->previous = $previous;"\` |
|        - |  1103 | `	"   }"\` |
|        - |  1104 | `	"}"\` |
|        - |  1105 | `	"public function getMessage(){"\` |
|        - |  1106 | `	"   return $this->message;"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	" public function getCode(){"\` |
|        - |  1109 | `	"  return $this->code;"\` |
|        - |  1110 | `	"}"\` |
|        - |  1111 | `	"public function getFile(){"\` |
|        - |  1112 | `	"  return $this->file;"\` |
|        - |  1113 | `	"}"\` |
|        - |  1114 | `	"public function getLine(){"\` |
|        - |  1115 | `	"  return $this->line;"\` |
|        - |  1116 | `	"}"\` |
|        - |  1117 | `	"public function getTrace(){"\` |
|        - |  1118 | `	"   return $this->trace;"\` |
|        - |  1119 | `	"}"\` |
|        - |  1120 | `	"public function getTraceAsString(){"\` |
|        - |  1121 | `	"  return debug_string_backtrace();"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"public function getPrevious(){"\` |
|        - |  1124 | `	"    return $this->previous;"\` |
|        - |  1125 | `	"}"\` |
|        - |  1126 | `	"public function __toString(){"\` |
|        - |  1127 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1128 | `    "}"\` |
|        - |  1129 | `	"}"\` |
|        - |  1130 | `	"class Error implements Throwable { "\` |
|        - |  1131 | `    "protected $message = '';"\` |
|        - |  1132 | `    "protected $code = 0;"\` |
|        - |  1133 | `    "protected $file;"\` |
|        - |  1134 | `    "protected $line;"\` |
|        - |  1135 | `    "protected $trace;"\` |
|        - |  1136 | `    "protected $previous;"\` |
|        - |  1137 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1138 | `	"   if( isset($message) ){"\` |
|        - |  1139 | `	"	  $this->message = $message;"\` |
|        - |  1140 | `	"   }"\` |
|        - |  1141 | `	"   $this->code = $code;"\` |
|        - |  1142 | `	"   $this->file = __FILE__;"\` |
|        - |  1143 | `	"   $this->line = __LINE__;"\` |
|        - |  1144 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1145 | `	"   if( isset($previous) ){"\` |
|        - |  1146 | `	"     $this->previous = $previous;"\` |
|        - |  1147 | `	"   }"\` |
|        - |  1148 | `	"}"\` |
|        - |  1149 | `	"public function getMessage(){"\` |
|        - |  1150 | `	"   return $this->message;"\` |
|        - |  1151 | `	"}"\` |
|        - |  1152 | `	"public function getCode(){"\` |
|        - |  1153 | `	"  return $this->code;"\` |
|        - |  1154 | `	"}"\` |
|        - |  1155 | `	"public function getFile(){"\` |
|        - |  1156 | `	"  return $this->file;"\` |
|        - |  1157 | `	"}"\` |
|        - |  1158 | `	"public function getLine(){"\` |
|        - |  1159 | `	"  return $this->line;"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"public function getTrace(){"\` |
|        - |  1162 | `	"   return $this->trace;"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"public function getTraceAsString(){"\` |
|        - |  1165 | `	"  return debug_string_backtrace();"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"public function getPrevious(){"\` |
|        - |  1168 | `	"    return $this->previous;"\` |
|        - |  1169 | `	"}"\` |
|        - |  1170 | `	"public function __toString(){"\` |
|        - |  1171 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1172 | `	"}"\` |
|        - |  1173 | `	"}"\` |
|        - |  1174 | `	"class TypeError extends Error { }"\` |
|        - |  1175 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1176 | `	"class ValueError extends Error { }"\` |
|        - |  1177 | `	"class FiberError extends Error { }"\` |
|        - |  1178 | `	"class AssertionError extends Error { }"\` |
|        - |  1179 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1180 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1181 | `	"class ErrorException extends Exception { "\` |
|        - |  1182 | `	"protected $severity;"\` |
|        - |  1183 | `	"public function __construct(string $message = null,"\` |
|        - |  1184 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1185 | `	"   if( isset($message) ){"\` |
|        - |  1186 | `	"	  $this->message = $message;"\` |
|        - |  1187 | `	"   }"\` |
|        - |  1188 | `	"   $this->severity = $severity;"\` |
|        - |  1189 | `	"   $this->code = $code;"\` |
|        - |  1190 | `	"   $this->file = $filename;"\` |
|        - |  1191 | `	"   $this->line = $lineno;"\` |
|        - |  1192 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1193 | `	"   if( isset($previous) ){"\` |
|        - |  1194 | `	"     $this->previous = $previous;"\` |
|        - |  1195 | `	"   }"\` |
|        - |  1196 | `	"}"\` |
|        - |  1197 | `	"public function getSeverity(){"\` |
|        - |  1198 | `	"   return $this->severity;"\` |
|        - |  1199 | `    "}"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `	"interface Iterator extends Traversable {"\` |
|        - |  1202 | `	"public function current();"\` |
|        - |  1203 | `	"public function key();"\` |
|        - |  1204 | `	"public function next();"\` |
|        - |  1205 | `	"public function rewind();"\` |
|        - |  1206 | `	"public function valid();"\` |
|        - |  1207 | `	"}"\` |
|        - |  1208 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1209 | `	"public function getIterator();"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `	"interface Serializable {"\` |
|        - |  1212 | `	"public function serialize();"\` |
|        - |  1213 | `	"public function unserialize(string $serialized);"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `	"/* Directory releated IO */"\` |
|        - |  1216 | `	"class Directory {"\` |
|        - |  1217 | `	"public $handle = null;"\` |
|        - |  1218 | `	"public $path  = null;"\` |
|        - |  1219 | `	"public function __construct(string $path)"\` |
|        - |  1220 | `	"{"\` |
|        - |  1221 | `	"   $this->handle = opendir($path);"\` |
|        - |  1222 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1223 | `	"      $this->path = $path;"\` |
|        - |  1224 | `	"   }"\` |
|        - |  1225 | `	"}"\` |
|        - |  1226 | `	"public function __destruct()"\` |
|        - |  1227 | `	"{"\` |
|        - |  1228 | `	"  if( $this->handle != null ){"\` |
|        - |  1229 | `	"       closedir($this->handle);"\` |
|        - |  1230 | `	"  }"\` |
|        - |  1231 | `	"}"\` |
|        - |  1232 | `	"public function read()"\` |
|        - |  1233 | `	"{"\` |
|        - |  1234 | `	"    return readdir($this->handle);"\` |
|        - |  1235 | `	"}"\` |
|        - |  1236 | `	"public function rewind()"\` |
|        - |  1237 | `	"{"\` |
|        - |  1238 | `	"    rewinddir($this->handle);"\` |
|        - |  1239 | `	"}"\` |
|        - |  1240 | `	"public function close()"\` |
|        - |  1241 | `	"{"\` |
|        - |  1242 | `	"    closedir($this->handle);"\` |
|        - |  1243 | `	"    $this->handle = null;"\` |
|        - |  1244 | `	"}"\` |
|        - |  1245 | `	"}"\` |
|        - |  1246 | `	"class Fiber {"\` |
|        - |  1247 | `	"  private $__ctx;"\` |
|        - |  1248 | `	"  private $__callable;"\` |
|        - |  1249 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1250 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1251 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1252 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1253 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1254 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1255 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1256 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1257 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1258 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1259 | `	"}"\` |
|        - |  1260 | `	"class Generator implements Iterator {"\` |
|        - |  1261 | `	"  private $__ctx;"\` |
|        - |  1262 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1263 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1264 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1265 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1266 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1267 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1268 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1269 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1270 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1271 | `	"}"\` |
|        - |  1272 | `	"class stdClass{"\` |
|        - |  1273 | `	"  public $value;"\` |
|        - |  1274 | `	" /* Magic methods */"\` |
|        - |  1275 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1276 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1277 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1278 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1279 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1280 | `	"}"\` |
|        - |  1281 | `	"function dir(string $path){"\` |
|        - |  1282 | `	"   return new Directory($path);"\` |
|        - |  1283 | `	"}"\` |
|        - |  1284 | `	"function Dir(string $path){"\` |
|        - |  1285 | `	"   return new Directory($path);"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1288 | `    "{"\` |
|        - |  1289 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1290 | `	"  $aDir = array();"\` |
|        - |  1291 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1292 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1293 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1294 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1295 | `	"   }"\` |
|        - |  1296 | `	"  closedir($pHandle);"\` |
|        - |  1297 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1298 | `	"      rsort($aDir);"\` |
|        - |  1299 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1300 | `	"      sort($aDir);"\` |
|        - |  1301 | `	"  }"\` |
|        - |  1302 | `	"  return $aDir;"\` |
|        - |  1303 | `	"}"\` |
|        - |  1304 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1305 | `	"/* Open the target directory */"\` |
|        - |  1306 | `	"$zDir = dirname($pattern);"\` |
|        - |  1307 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1308 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1309 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1310 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1311 | `	"	return FALSE;"\` |
|        - |  1312 | `	"}"\` |
|        - |  1313 | `	"$pattern = basename($pattern);"\` |
|        - |  1314 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1315 | `	"/* Loop throw available entries */"\` |
|        - |  1316 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1317 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1318 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1319 | `	"	if( $rc ){"\` |
|        - |  1320 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1321 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1322 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1323 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1324 | `	"		  }"\` |
|        - |  1325 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1326 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1327 | `	"		 continue;"\` |
|        - |  1328 | `	"	   }"\` |
|        - |  1329 | `	"	   /* Add the entry */"\` |
|        - |  1330 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1331 | `	"	}"\` |
|        - |  1332 | `	" }"\` |
|        - |  1333 | `	"/* Close the handle */"\` |
|        - |  1334 | `	"closedir($pHandle);"\` |
|        - |  1335 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1336 | `	"  /* Sort the array */"\` |
|        - |  1337 | `	"  sort($pArray);"\` |
|        - |  1338 | `	"}"\` |
|        - |  1339 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1340 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1341 | `	"  $pArray[] = $pattern;"\` |
|        - |  1342 | `	"}"\` |
|        - |  1343 | `	"/* Return the created array */"\` |
|        - |  1344 | `	"return $pArray;"\` |
|        - |  1345 | `   "}"\` |
|        - |  1346 | `   "/* Creates a temporary file */"\` |
|        - |  1347 | `   "function tmpfile(){"\` |
|        - |  1348 | `   "  /* Extract the temp directory */"\` |
|        - |  1349 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1350 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1351 | `   "    /* Use the current dir */"\` |
|        - |  1352 | `   "    $zTempDir = '.';"\` |
|        - |  1353 | `   "  }"\` |
|        - |  1354 | `   "  /* Create the file */"\` |
|        - |  1355 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1356 | `   "  return $pHandle;"\` |
|        - |  1357 | `   "}"\` |
|        - |  1358 | `   "/* Creates a temporary filename */"\` |
|        - |  1359 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1360 | `   "{"\` |
|        - |  1361 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1362 | `   "}"\` |
|        - |  1363 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1364 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1365 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1366 | `   "/* Copy arguments */"\` |
|        - |  1367 | `   "$nArgs = func_num_args();"\` |
|        - |  1368 | `   "$pNew = array();"\` |
|        - |  1369 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1370 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1371 | `    "}"\` |
|        - |  1372 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1373 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1374 | `	"/* Erase */"\` |
|        - |  1375 | `	"array_erase($pArray);"\` |
|        - |  1376 | `	"/* Unshift */"\` |
|        - |  1377 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1378 | `	"return sizeof($pArray);"\` |
|        - |  1379 | `    "}"\` |
|        - |  1380 | `	"function array_merge_recursive(){"\` |
|        - |  1381 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1382 | `    "$arrays = func_get_args();"\` |
|        - |  1383 | `    "$narrays = count($arrays);"\` |
|        - |  1384 | `    "$ret = array();"\` |
|        - |  1385 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1386 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1387 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1388 | `	 " }"\` |
|        - |  1389 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1390 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1391 | `     "  if( $keyIsInt ) {"\` |
|        - |  1392 | `     "   $ret[] = $value;"\` |
|        - |  1393 | `     "  } else {"\` |
|        - |  1394 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1395 | `     "    $cur = $ret[$key];"\` |
|        - |  1396 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1397 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1398 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1399 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1400 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1401 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1402 | `     "    } else {"\` |
|        - |  1403 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1404 | `     "    }"\` |
|        - |  1405 | `     "   } else {"\` |
|        - |  1406 | `     "    $ret[$key] = $value;"\` |
|        - |  1407 | `     "   }"\` |
|        - |  1408 | `     "  }"\` |
|        - |  1409 | `     " }"\` |
|        - |  1410 | `	 " }"\` |
|        - |  1411 | `	 " return $ret;"\` |
|        - |  1412 | `    "}"\` |
|        - |  1413 | `	"function max(){"\` |
|        - |  1414 | `    "  $pArgs = func_get_args();"\` |
|        - |  1415 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1416 | `	"  return null;"\` |
|        - |  1417 | `    " }"\` |
|        - |  1418 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1419 | `    " $pArg = $pArgs[0];"\` |
|        - |  1420 | `	" if( !is_array($pArg) ){"\` |
|        - |  1421 | `	"   return $pArg; "\` |
|        - |  1422 | `	" }"\` |
|        - |  1423 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1424 | `	"   return null;"\` |
|        - |  1425 | `	" }"\` |
|        - |  1426 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1427 | `	" reset($pArg);"\` |
|        - |  1428 | `	" $max = current($pArg);"\` |
|        - |  1429 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1430 | `	"   if( $val > $max ){"\` |
|        - |  1431 | `	"     $max = $val;"\` |
|        - |  1432 | `    " }"\` |
|        - |  1433 | `	" }"\` |
|        - |  1434 | `	" return $max;"\` |
|        - |  1435 | `    " }"\` |
|        - |  1436 | `    " $max = $pArgs[0];"\` |
|        - |  1437 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1438 | `    " $val = $pArgs[$i];"\` |
|        - |  1439 | `	"if( $val > $max ){"\` |
|        - |  1440 | `	" $max = $val;"\` |
|        - |  1441 | `	"}"\` |
|        - |  1442 | `    " }"\` |
|        - |  1443 | `	" return $max;"\` |
|        - |  1444 | `    "}"\` |
|        - |  1445 | `	"function min(){"\` |
|        - |  1446 | `    "  $pArgs = func_get_args();"\` |
|        - |  1447 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1448 | `	"  return null;"\` |
|        - |  1449 | `    " }"\` |
|        - |  1450 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1451 | `    " $pArg = $pArgs[0];"\` |
|        - |  1452 | `	" if( !is_array($pArg) ){"\` |
|        - |  1453 | `	"   return $pArg; "\` |
|        - |  1454 | `	" }"\` |
|        - |  1455 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1456 | `	"   return null;"\` |
|        - |  1457 | `	" }"\` |
|        - |  1458 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1459 | `	" reset($pArg);"\` |
|        - |  1460 | `	" $min = current($pArg);"\` |
|        - |  1461 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1462 | `	"   if( $val < $min ){"\` |
|        - |  1463 | `	"     $min = $val;"\` |
|        - |  1464 | `    " }"\` |
|        - |  1465 | `	" }"\` |
|        - |  1466 | `	" return $min;"\` |
|        - |  1467 | `    " }"\` |
|        - |  1468 | `    " $min = $pArgs[0];"\` |
|        - |  1469 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1470 | `    " $val = $pArgs[$i];"\` |
|        - |  1471 | `	"if( $val < $min ){"\` |
|        - |  1472 | `	" $min = $val;"\` |
|        - |  1473 | `	" }"\` |
|        - |  1474 | `    " }"\` |
|        - |  1475 | `	" return $min;"\` |
|        - |  1476 | `	"}"\` |
|        - |  1477 | `	"function fileowner(string $file){"\` |
|        - |  1478 | `    " $a = stat($file);"\` |
|        - |  1479 | `	" if( !is_array($a) ){"\` |
|        - |  1480 | `	"	return false;"\` |
|        - |  1481 | `	" }"\` |
|        - |  1482 | `	" return $a['uid'];"\` |
|        - |  1483 | `    "}"\` |
|        - |  1484 | `    "function filegroup(string $file){"\` |
|        - |  1485 | `	" $a = stat($file);"\` |
|        - |  1486 | `	" if( !is_array($a) ){"\` |
|        - |  1487 | `	"	return false;"\` |
|        - |  1488 | `	" }"\` |
|        - |  1489 | `	" return $a['gid'];"\` |
|        - |  1490 | `    "}"\` |
|        - |  1491 | `	 "function fileinode(string $file){"\` |
|        - |  1492 | `	" $a = stat($file);"\` |
|        - |  1493 | `	" if( !is_array($a) ){"\` |
|        - |  1494 | `	"	return false;"\` |
|        - |  1495 | `	" }"\` |
|        - |  1496 | `	" return $a['ino'];"\` |
|        - |  1497 | `    "}"` |
|        - |  1498 |  |
|        - |  1499 | `/*` |
|        - |  1500 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1501 | ` * start compiling the target PHP program.` |
|        - |  1502 | ` */` |
|     3134 |  1503 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1504 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1505 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1506 | `	 )` |
|        2 |  1507 |  |
|        - |  1508 | `	SyString sBuiltin;` |
|        - |  1509 | `	ph7_value *pObj;` |
|        - |  1510 | `	sxi32 rc;` |
|        - |  1511 | `	/* Zero the structure */` |
|     3136 |  1512 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1513 | `	/* Initialize VM fields */` |
|     3136 |  1514 | `	pVm->pEngine = &(*pEngine);` |
|     3136 |  1515 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1516 | `	/* Instructions containers */` |
|     3136 |  1517 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3136 |  1518 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3136 |  1519 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1520 | `	/* Object containers */` |
|     3136 |  1521 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3136 |  1522 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1523 | `	/* Virtual machine internal containers */` |
|     3136 |  1524 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3136 |  1525 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3136 |  1526 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3136 |  1527 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3136 |  1528 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3136 |  1529 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3136 |  1530 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3136 |  1531 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3136 |  1532 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3136 |  1533 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3136 |  1534 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3136 |  1535 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3136 |  1536 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3136 |  1537 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3136 |  1538 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3136 |  1539 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3136 |  1540 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3136 |  1541 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3136 |  1542 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3136 |  1543 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3136 |  1544 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3136 |  1545 | `	pVm->pPendingException = 0;` |
|        - |  1546 | `	/* Configuration containers */` |
|     3136 |  1547 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1548 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1549 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1550 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3136 |  1551 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3136 |  1552 | `	pVm->iResponseStatus = 200;` |
|     3136 |  1553 | `	pVm->bHeadersSent = 0;` |
|     3136 |  1554 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1555 | `	/* Error callbacks containers */` |
|     3136 |  1556 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3136 |  1557 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3136 |  1558 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3136 |  1559 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3136 |  1560 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1561 | `	/* Set a default recursion limit */` |
|        - |  1562 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3136 |  1563 | `	pVm->nMaxDepth = 32;` |
|        - |  1564 | `#else` |
|        - |  1565 | `	pVm->nMaxDepth = 16;` |
|        - |  1566 | `#endif` |
|        - |  1567 | `	/* Default assertion flags */` |
|     3136 |  1568 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1569 | `	/* JSON return status */` |
|     3136 |  1570 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1571 | `	/* PRNG context */` |
|     3136 |  1572 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1573 | `	/* Install the null constant */` |
|     3136 |  1574 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1575 | `	if( pObj == 0 ){` |
|      ! 0 |  1576 | `		rc = SXERR_MEM;` |
|      ! 0 |  1577 | `		goto Err;` |
|        - |  1578 | `	}` |
|     3136 |  1579 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1580 | `	/* Install the boolean TRUE constant */` |
|     3136 |  1581 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1582 | `	if( pObj == 0 ){` |
|      ! 0 |  1583 | `		rc = SXERR_MEM;` |
|      ! 0 |  1584 | `		goto Err;` |
|        - |  1585 | `	}` |
|     3136 |  1586 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1587 | `	/* Install the boolean FALSE constant */` |
|     3136 |  1588 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1589 | `	if( pObj == 0 ){` |
|      ! 0 |  1590 | `		rc = SXERR_MEM;` |
|      ! 0 |  1591 | `		goto Err;` |
|        - |  1592 | `	}` |
|     3136 |  1593 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1594 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1595 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1596 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3136 |  1597 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3136 |  1598 | `	if( pObj == 0 ){` |
|      ! 0 |  1599 | `		rc = SXERR_MEM;` |
|      ! 0 |  1600 | `		goto Err;` |
|        - |  1601 | `	}` |
|     3136 |  1602 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1603 | `	/* Create the global frame */` |
|     3136 |  1604 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3136 |  1605 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1606 | `		goto Err;` |
|        - |  1607 | `	}` |
|        - |  1608 | `	/* Initialize the code generator */` |
|     3136 |  1609 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3136 |  1610 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1611 | `		goto Err;` |
|        - |  1612 | `	}` |
|        - |  1613 | `	/* VM correctly initialized,set the magic number */` |
|     3136 |  1614 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3136 |  1615 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1616 | `	/* Compile the built-in library */` |
|     3136 |  1617 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1618 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3136 |  1619 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1620 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3136 |  1621 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3136 |  1622 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3136 |  1623 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|        - |  1624 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3136 |  1625 | `	pVm->pCoalesceObj = 0;` |
|     3136 |  1626 | `	pVm->bCoalesceArmed = 0;` |
|     3136 |  1627 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1628 | `	/* Register Fiber internal C functions */` |
|     3136 |  1629 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3136 |  1630 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3136 |  1631 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3136 |  1632 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3136 |  1633 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3136 |  1634 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3136 |  1635 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3136 |  1636 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3136 |  1637 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3136 |  1638 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1639 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3136 |  1640 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3136 |  1641 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3136 |  1642 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3136 |  1643 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3136 |  1644 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3136 |  1645 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3136 |  1646 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3136 |  1647 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3136 |  1648 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3136 |  1649 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1650 | `	/* Reset the code generator */` |
|     3136 |  1651 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3136 |  1652 | `	return SXRET_OK;` |
|      ! 0 |  1653 | `Err:` |
|      ! 0 |  1654 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1655 | `	return rc;` |
|     1569 |  1656 |  |
|        - |  1657 | `/*` |
|        - |  1658 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1659 | ` * routine which store the output in an internal blob.` |
|        - |  1660 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1661 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1662 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1663 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1664 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1665 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1666 | ` * to finish executing and extracting the output.` |
|        - |  1667 | ` */` |
|       38 |  1668 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1669 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1670 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1671 | `	void *pUserData     /* User private data */` |
|        - |  1672 | `	)` |
|      ! 0 |  1673 |  |
|        - |  1674 | `	 sxi32 rc;` |
|        - |  1675 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1676 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1677 | `	 return rc;` |
|      ! 0 |  1678 |  |
|        - |  1679 | `/*` |
|        - |  1680 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1681 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1682 | ` */` |
|    20036 |  1683 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1684 |  |
|    20038 |  1685 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20038 |  1686 | `	if( xCons != VmObConsumer ){` |
|     8170 |  1687 | `		pVm->nOutputLen += nLen;` |
|     8170 |  1688 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1022 |  1689 | `			pVm->bHeadersSent = 1;` |
|      510 |  1690 | `		}` |
|     4084 |  1691 | `	}` |
|    20038 |  1692 |  |
|        - |  1693 | `#define VM_STACK_GUARD 16` |
|        - |  1694 | `/*` |
|        - |  1695 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1696 | ` * our compiled PHP program.` |
|        - |  1697 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1698 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1699 | ` */` |
|    44542 |  1700 | `static ph7_value * VmNewOperandStack(` |
|        - |  1701 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1702 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1703 | `	)` |
|        2 |  1704 |  |
|        - |  1705 | `	ph7_value *pStack;` |
|        - |  1706 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1707 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1708 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1709 | `  ** on the maximum stack depth required.` |
|        - |  1710 | `  **` |
|        - |  1711 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1712 | `  */` |
|    44544 |  1713 | `	nInstr += VM_STACK_GUARD;` |
|    44544 |  1714 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    44544 |  1715 | `	if( pStack == 0 ){` |
|      ! 0 |  1716 | `		return 0;` |
|        - |  1717 | `	}` |
|        - |  1718 | `	/* Initialize the operand stack */` |
|  3015394 |  1719 | `	while( nInstr > 0 ){` |
|  2970852 |  1720 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2970852 |  1721 | `		--nInstr;` |
|        2 |  1722 | `	}` |
|        - |  1723 | `	/* Ready for bytecode execution */` |
|    44544 |  1724 | `	return pStack;` |
|    22273 |  1725 |  |
|        - |  1726 | `/* Forward declaration */` |
|        - |  1727 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1728 | `/*` |
|        - |  1729 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1730 | ` * This routine gets called by the PH7 engine after` |
|        - |  1731 | ` * successful compilation of the target PHP program.` |
|        - |  1732 | ` */` |
|     2820 |  1733 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1734 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1735 | `	)` |
|        2 |  1736 |  |
|        - |  1737 | `	SyHashEntry *pEntry;` |
|        - |  1738 | `	sxi32 rc;` |
|     2822 |  1739 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1740 | `		/* Initialize your VM first */` |
|      ! 0 |  1741 | `		return SXERR_CORRUPT;` |
|        - |  1742 | `	}` |
|        - |  1743 | `	/* Mark the VM ready for byte-code execution */` |
|     2822 |  1744 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1745 | `	/* Release the code generator now we have compiled our program */` |
|     2822 |  1746 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1747 | `	/* Emit the DONE instruction */` |
|     2822 |  1748 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2822 |  1749 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1750 | `		return SXERR_MEM;` |
|        - |  1751 | `	}` |
|        - |  1752 | `	/* Script return value */` |
|     2822 |  1753 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1754 | `	/* Allocate a new operand stack */` |
|     2822 |  1755 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2822 |  1756 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1757 | `		return SXERR_MEM;` |
|        - |  1758 | `	}` |
|        - |  1759 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1760 | `	 * private data. */` |
|     2822 |  1761 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2822 |  1762 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1763 | `	/* Allocate the reference table */` |
|     2822 |  1764 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2822 |  1765 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2822 |  1766 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1767 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1768 | `		return SXERR_MEM;` |
|        - |  1769 | `	}` |
|        - |  1770 | `	/* Zero the reference table */` |
|     2822 |  1771 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1772 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2822 |  1773 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2822 |  1774 | `	if( rc != SXRET_OK ){` |
|        - |  1775 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1776 | `		return rc;` |
|        - |  1777 | `	}` |
|        - |  1778 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2822 |  1779 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2822 |  1780 | `	if( rc != SXRET_OK ){` |
|        - |  1781 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1782 | `		return rc;` |
|        - |  1783 | `	}` |
|        - |  1784 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2822 |  1785 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1786 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2822 |  1787 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1788 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2822 |  1789 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1790 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1791 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2822 |  1792 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2822 |  1793 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1794 | `#endif` |
|        - |  1795 | `	/* Initialize and install static and constants class attributes */` |
|     2822 |  1796 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    70846 |  1797 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    68026 |  1798 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    68026 |  1799 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1800 | `			return rc;` |
|        - |  1801 | `		}` |
|        2 |  1802 | `	}` |
|        - |  1803 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2822 |  1804 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1805 | `	/* VM is ready for bytecode execution */` |
|     2822 |  1806 | `	return SXRET_OK;` |
|     1412 |  1807 |  |
|        - |  1808 | `/*` |
|        - |  1809 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1810 | ` */` |
|      ! 0 |  1811 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1812 |  |
|      ! 0 |  1813 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1814 | `		return SXERR_CORRUPT;` |
|        - |  1815 | `	}` |
|        - |  1816 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1817 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1818 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1819 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1820 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1821 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1822 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1823 | `	pVm->bHttpContext = 0;` |
|        - |  1824 | `	/* Set the ready flag */` |
|      ! 0 |  1825 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1826 | `	return SXRET_OK;` |
|      ! 0 |  1827 |  |
|        - |  1828 | `/*` |
|        - |  1829 | ` * Release a Virtual Machine.` |
|        - |  1830 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1831 | ` */` |
|     2820 |  1832 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1833 |  |
|        - |  1834 | `	/* Set the stale magic number */` |
|     2822 |  1835 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1836 | `	/* Release the private memory subsystem */` |
|     2822 |  1837 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2822 |  1838 | `	return SXRET_OK;` |
|        2 |  1839 |  |
|        - |  1840 | `/*` |
|        - |  1841 | ` * Initialize a foreign function call context.` |
|        - |  1842 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1843 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1844 | ` * functions.` |
|        - |  1845 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1846 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1847 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1848 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1849 | ` */` |
|   692142 |  1850 | `static sxi32 VmInitCallContext(` |
|        - |  1851 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1852 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1853 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1854 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1855 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1856 | `	)` |
|        2 |  1857 |  |
|   692144 |  1858 | `	pOut->pFunc = pFunc;` |
|   692144 |  1859 | `	pOut->pVm   = pVm;` |
|   692144 |  1860 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   692144 |  1861 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1862 | `	/* Assume a null return value */` |
|   692144 |  1863 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   692144 |  1864 | `	pOut->pRet = pRet;` |
|   692144 |  1865 | `	pOut->iFlags = iFlags;` |
|   692144 |  1866 | `	return SXRET_OK;` |
|        2 |  1867 |  |
|        - |  1868 | `/*` |
|        - |  1869 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1870 | ` * left behind.` |
|        - |  1871 | ` */` |
|   692142 |  1872 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1873 |  |
|        - |  1874 | `	sxu32 n;` |
|   692144 |  1875 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8538 |  1876 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    24882 |  1877 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16346 |  1878 | `			if( apObj[n] == 0 ){` |
|        - |  1879 | `				/* Already released */` |
|      318 |  1880 | `				continue;` |
|        - |  1881 | `			}` |
|    16030 |  1882 | `			PH7_MemObjRelease(apObj[n]);` |
|    16030 |  1883 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8016 |  1884 | `		}` |
|     8538 |  1885 | `		SySetRelease(&pCtx->sVar);` |
|     4268 |  1886 | `	}` |
|   692144 |  1887 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1888 | `		ph7_aux_data *aAux;` |
|        - |  1889 | `		void *pChunk;` |
|        - |  1890 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1891 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1892 | `		 */` |
|        9 |  1893 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1894 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1895 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1896 | `			/* Release the chunk */` |
|       25 |  1897 | `			if( pChunk ){` |
|       25 |  1898 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1899 | `			}` |
|       13 |  1900 | `		}` |
|        9 |  1901 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1902 | `	}` |
|   692144 |  1903 |  |
|        - |  1904 | `/*` |
|        - |  1905 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1906 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1907 | ` */` |
|      316 |  1908 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1909 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1910 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|      318 |  1913 | `	if( pValue == 0 ){` |
|        - |  1914 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1915 | `		return;` |
|        - |  1916 | `	}` |
|      318 |  1917 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      318 |  1918 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1919 | `		sxu32 n;` |
|     1116 |  1920 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1116 |  1921 | `			if( apObj[n] == pValue ){` |
|      318 |  1922 | `				PH7_MemObjRelease(pValue);` |
|      318 |  1923 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1924 | `				/* Mark as released */` |
|      318 |  1925 | `				apObj[n] = 0;` |
|      318 |  1926 | `				break;` |
|        - |  1927 | `			}` |
|      401 |  1928 | `		}` |
|      158 |  1929 | `	}` |
|      160 |  1930 |  |
|        - |  1931 | `/*` |
|        - |  1932 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1933 | ` */` |
|  3931288 |  1934 | `static void VmPopOperand(` |
|        - |  1935 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1936 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1937 | `	)` |
|        2 |  1938 |  |
|  3931290 |  1939 | `	ph7_value *pTos = *ppTos;` |
|  8374154 |  1940 | `	while( nPop > 0 ){` |
|  4442866 |  1941 | `		PH7_MemObjRelease(pTos);` |
|  4442866 |  1942 | `		pTos--;` |
|  4442866 |  1943 | `		nPop--;` |
|        2 |  1944 | `	}` |
|        - |  1945 | `	/* Top of the stack */` |
|  3931290 |  1946 | `	*ppTos = pTos;` |
|  3931290 |  1947 |  |
|        - |  1948 | `/*` |
|        - |  1949 | ` * Reserve a memory object.` |
|        - |  1950 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1951 | ` */` |
|  3195034 |  1952 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1953 |  |
|  3195036 |  1954 | `	ph7_value *pObj = 0;` |
|        - |  1955 | `	VmSlot *pSlot;` |
|        - |  1956 | `	sxu32 nIdx;` |
|        - |  1957 | `	/* Check for a free slot */` |
|  3195036 |  1958 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3195036 |  1959 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3195036 |  1960 | `	if( pSlot ){` |
|  1043440 |  1961 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1043440 |  1962 | `		nIdx = pSlot->nIdx;` |
|   521719 |  1963 | `	}` |
|  3195036 |  1964 | `	if( pObj == 0 ){` |
|        - |  1965 | `		/* Reserve a new memory object */` |
|  2151598 |  1966 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151598 |  1967 | `		if( pObj == 0 ){` |
|      ! 0 |  1968 | `			return 0;` |
|        - |  1969 | `		}` |
|  1075798 |  1970 | `	}` |
|        - |  1971 | `	/* Set a null default value */` |
|  3195036 |  1972 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3195036 |  1973 | `	pObj->nIdx = nIdx;` |
|  3195036 |  1974 | `	return pObj;` |
|  1597519 |  1975 |  |
|        - |  1976 | `/*` |
|        - |  1977 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1978 | ` */` |
|    35188 |  1979 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1980 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1981 | `	const char *zKey,  /* Entry key */` |
|        - |  1982 | `	sxu32 nByte,       /* Key length */` |
|        - |  1983 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1984 | `	)` |
|        2 |  1985 |  |
|        - |  1986 | `	ph7_value sKey;` |
|        - |  1987 | `	sxi32 rc;` |
|    35190 |  1988 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35190 |  1989 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1990 | `	/* Perform the insertion */` |
|    35190 |  1991 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35190 |  1992 | `	PH7_MemObjRelease(&sKey);` |
|    35190 |  1993 | `	return rc;` |
|        2 |  1994 |  |
|        - |  1995 | `/*` |
|        - |  1996 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1997 | ` * Return a pointer to the variable value on success.` |
|        - |  1998 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1999 | ` */` |
|  3655872 |  2000 | `static ph7_value * VmExtractMemObj(` |
|        - |  2001 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2002 | `	const SyString *pName, /* Variable name */` |
|        - |  2003 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2004 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2005 | `	)` |
|        2 |  2006 |  |
|  3655874 |  2007 | `	int bNullify = FALSE;` |
|        - |  2008 | `	SyHashEntry *pEntry;` |
|        - |  2009 | `	VmFrame *pFrame;` |
|        - |  2010 | `	ph7_value *pObj;` |
|        - |  2011 | `	sxu32 nIdx;` |
|        - |  2012 | `	sxi32 rc;` |
|        - |  2013 | `	/* Point to the top active frame */` |
|  3655874 |  2014 | `	pFrame = pVm->pFrame;` |
|  3655874 |  2015 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2016 | `	/* Perform the lookup */` |
|  3655874 |  2017 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2018 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2019 | `		pName = &sAnnon;` |
|        - |  2020 | `		/* Always nullify the object */` |
|      ! 0 |  2021 | `		bNullify = TRUE;` |
|      ! 0 |  2022 | `		bDup = FALSE;` |
|      ! 0 |  2023 | `	}` |
|        - |  2024 | `	/* Check the superglobals table first */` |
|  3655874 |  2025 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3655874 |  2026 | `	if( pEntry == 0 ){` |
|        - |  2027 | `		/* Query the top active frame */` |
|  3655834 |  2028 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3655834 |  2029 | `		if( pEntry == 0 ){` |
|   112652 |  2030 | `			char *zName = (char *)pName->zString;` |
|        - |  2031 | `			VmSlot sLocal;` |
|   112652 |  2032 | `			if( !bCreate ){` |
|        - |  2033 | `				/* Do not create the variable,return NULL instead */` |
|      958 |  2034 | `				return 0;` |
|        - |  2035 | `			}` |
|        - |  2036 | `			/* No such variable,automatically create a new one and install` |
|        - |  2037 | `			 * it in the current frame.` |
|        - |  2038 | `			 */` |
|   111696 |  2039 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   111696 |  2040 | `			if( pObj == 0 ){` |
|      ! 0 |  2041 | `				return 0;` |
|        - |  2042 | `			}` |
|   111696 |  2043 | `			nIdx = pObj->nIdx;` |
|   111696 |  2044 | `			if( bDup ){` |
|        - |  2045 | `				/* Duplicate name */` |
|      230 |  2046 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2047 | `				if( zName == 0 ){` |
|      ! 0 |  2048 | `					return 0;` |
|        - |  2049 | `				}` |
|      114 |  2050 | `			}` |
|        - |  2051 | `			/* Link to the top active VM frame */` |
|   111696 |  2052 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   111696 |  2053 | `			if( rc != SXRET_OK ){` |
|        - |  2054 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2055 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2056 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2057 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2058 | `				return 0;` |
|        - |  2059 | `			}` |
|   111696 |  2060 | `			if( pFrame->pParent != 0 ){` |
|        - |  2061 | `				/* Local variable */` |
|   104736 |  2062 | `				sLocal.nIdx = nIdx;` |
|   104736 |  2063 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52369 |  2064 | `			}else{` |
|        - |  2065 | `				/* Register in the $GLOBALS array */` |
|     6962 |  2066 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2067 | `			}` |
|        - |  2068 | `			/* Install in the reference table */` |
|   111696 |  2069 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2070 | `			/* Save object index */` |
|   111696 |  2071 | `			pObj->nIdx = nIdx;` |
|    55849 |  2072 | `		}else{` |
|        - |  2073 | `			/* Extract variable contents */` |
|  3543184 |  2074 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3543184 |  2075 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3543184 |  2076 | `			if( bNullify && pObj ){` |
|      ! 0 |  2077 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2078 | `			}` |
|        - |  2079 | `		}` |
|  1827550 |  2080 | `	}else{` |
|        - |  2081 | `		/* Superglobal */` |
|       42 |  2082 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2083 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2084 | `	}` |
|  3654918 |  2085 | `	return pObj;` |
|  1828048 |  2086 |  |
|        - |  2087 | `/*` |
|        - |  2088 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2089 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2090 | ` */` |
|     3124 |  2091 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2092 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2093 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2094 | `	sxu32 nByte        /* zName length */` |
|        - |  2095 | `	)` |
|        2 |  2096 |  |
|        - |  2097 | `	SyHashEntry *pEntry;` |
|        - |  2098 | `	ph7_value *pValue;` |
|        - |  2099 | `	sxu32 nIdx;` |
|        - |  2100 | `	/* Query the superglobal table */` |
|     3126 |  2101 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3126 |  2102 | `	if( pEntry == 0 ){` |
|        - |  2103 | `		/* No such entry */` |
|      ! 0 |  2104 | `		return 0;` |
|        - |  2105 | `	}` |
|        - |  2106 | `	/* Extract the superglobal index in the global object pool */` |
|     3126 |  2107 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2108 | `	/* Extract the variable value  */` |
|     3126 |  2109 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3126 |  2110 | `	return pValue;` |
|     1564 |  2111 |  |
|        - |  2112 | `/*` |
|        - |  2113 | ` * Perform a raw hashmap insertion.` |
|        - |  2114 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2115 | ` */` |
|     3154 |  2116 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2117 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2118 | `	const char *zKey,   /* Entry key */` |
|        - |  2119 | `	int nKeylen,        /* zKey length*/` |
|        - |  2120 | `	const char *zData,  /* Entry data */` |
|        - |  2121 | `	int nLen            /* zData length */` |
|        - |  2122 | `	)` |
|        2 |  2123 |  |
|        - |  2124 | `	ph7_value sKey,sValue;` |
|        - |  2125 | `	sxi32 rc;` |
|     3156 |  2126 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3156 |  2127 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3156 |  2128 | `	if( zKey ){` |
|     3134 |  2129 | `		if( nKeylen < 0 ){` |
|     3082 |  2130 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1540 |  2131 | `		}` |
|     3134 |  2132 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1566 |  2133 | `	}` |
|     3156 |  2134 | `	if( zData ){` |
|     3156 |  2135 | `		if( nLen < 0 ){` |
|        - |  2136 | `			/* Compute length automatically */` |
|      144 |  2137 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2138 | `		}` |
|     3156 |  2139 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1577 |  2140 | `	}` |
|        - |  2141 | `	/* Perform the insertion */` |
|     3156 |  2142 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3156 |  2143 | `	PH7_MemObjRelease(&sKey);` |
|     3156 |  2144 | `	PH7_MemObjRelease(&sValue);` |
|     3156 |  2145 | `	return rc;` |
|        2 |  2146 |  |
|        - |  2147 | `/*` |
|        - |  2148 | ` * Configure a working virtual machine instance.` |
|        - |  2149 | ` *` |
|        - |  2150 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2151 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2152 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2153 | ` * The second argument to this function is an integer configuration option` |
|        - |  2154 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2155 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2156 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2157 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2158 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2159 | ` */` |
|    45450 |  2160 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2161 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2162 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2163 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2164 | `	)` |
|        2 |  2165 |  |
|    45452 |  2166 | `	sxi32 rc = SXRET_OK;` |
|    45452 |  2167 | `	switch(nOp){` |
|     1402 |  2168 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2806 |  2169 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2806 |  2170 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2171 | `		/* VM output consumer callback */` |
|        - |  2172 | `#ifdef UNTRUST` |
|        - |  2173 | `		if( xConsumer == 0 ){` |
|        - |  2174 | `			rc = SXERR_CORRUPT;` |
|        - |  2175 | `			break;` |
|        - |  2176 | `		}` |
|        - |  2177 | `#endif` |
|        - |  2178 | `		/* Install the output consumer */` |
|     2806 |  2179 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2806 |  2180 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2806 |  2181 | `		break;` |
|        - |  2182 | `							   }` |
|     1410 |  2183 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2184 | `		/* Import path */` |
|        - |  2185 | `		  const char *zPath;` |
|        - |  2186 | `		  SyString sPath;` |
|     2822 |  2187 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2188 | `#if defined(UNTRUST)` |
|        - |  2189 | `		  if( zPath == 0 ){` |
|        - |  2190 | `			  rc = SXERR_EMPTY;` |
|        - |  2191 | `			  break;` |
|        - |  2192 | `		  }` |
|        - |  2193 | `#endif` |
|     2822 |  2194 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2195 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2196 | `#ifdef __WINNT__` |
|        2 |  2197 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2198 | `#endif` |
|     5642 |  2199 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2200 | `		  /* Remove leading and trailing white spaces */` |
|     2822 |  2201 | `		  SyStringFullTrim(&sPath);` |
|     2822 |  2202 | `		  if( sPath.nByte > 0 ){` |
|        - |  2203 | `			  /* Store the path in the corresponding conatiner */` |
|     2822 |  2204 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1410 |  2205 | `		  }` |
|     2822 |  2206 | `		  break;` |
|        - |  2207 | `									 }` |
|     1410 |  2208 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2209 | `		/* Run-Time Error report */` |
|     2822 |  2210 | `		pVm->bErrReport = 1;` |
|     2822 |  2211 | `		break;` |
|      ! 0 |  2212 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2213 | `		/* Recursion depth */` |
|      ! 0 |  2214 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2215 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2216 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2217 | `		}` |
|      ! 0 |  2218 | `		break;` |
|        - |  2219 | `									   }` |
|      ! 0 |  2220 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2221 | `		/* VM output length in bytes */` |
|      ! 0 |  2222 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2223 | `#ifdef UNTRUST` |
|        - |  2224 | `		if( pOut == 0 ){` |
|        - |  2225 | `			rc = SXERR_CORRUPT;` |
|        - |  2226 | `			break;` |
|        - |  2227 | `		}` |
|        - |  2228 | `#endif` |
|      ! 0 |  2229 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2230 | `		break;` |
|        - |  2231 | `							   }` |
|        - |  2232 |  |
|    14100 |  2233 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2234 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2235 | `		/* Create a new superglobal/global variable */` |
|    28202 |  2236 | `		const char *zName = va_arg(ap,const char *);` |
|    28202 |  2237 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2238 | `		SyHashEntry *pEntry;` |
|        - |  2239 | `		ph7_value *pObj;` |
|        - |  2240 | `		sxu32 nByte;` |
|        - |  2241 | `		sxu32 nIdx;` |
|        - |  2242 | `#ifdef UNTRUST` |
|        - |  2243 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2244 | `			rc = SXERR_CORRUPT;` |
|        - |  2245 | `			break;` |
|        - |  2246 | `		}` |
|        - |  2247 | `#endif` |
|    28202 |  2248 | `		nByte = SyStrlen(zName);` |
|    28202 |  2249 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2250 | `			/* Check if the superglobal is already installed */` |
|    28202 |  2251 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14102 |  2252 | `		}else{` |
|        - |  2253 | `			/* Query the top active VM frame */` |
|      ! 0 |  2254 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2255 | `		}` |
|    28202 |  2256 | `		if( pEntry ){` |
|        - |  2257 | `			/* Variable already installed */` |
|      ! 0 |  2258 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2259 | `			/* Extract contents */` |
|      ! 0 |  2260 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2261 | `			if( pObj ){` |
|        - |  2262 | `				/* Overwrite old contents */` |
|      ! 0 |  2263 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2264 | `			}` |
|      ! 0 |  2265 | `		}else{` |
|        - |  2266 | `			/* Install a new variable */` |
|    28202 |  2267 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28202 |  2268 | `			if( pObj == 0 ){` |
|      ! 0 |  2269 | `				rc = SXERR_MEM;` |
|      ! 0 |  2270 | `				break;` |
|        - |  2271 | `			}` |
|    28202 |  2272 | `			nIdx = pObj->nIdx;` |
|        - |  2273 | `			/* Copy value */` |
|    28202 |  2274 | `			PH7_MemObjStore(pValue,pObj);` |
|    28202 |  2275 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2276 | `				/* Install the superglobal */` |
|    28202 |  2277 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14102 |  2278 | `			}else{` |
|        - |  2279 | `				/* Install in the current frame */` |
|      ! 0 |  2280 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2281 | `			}` |
|    28202 |  2282 | `			if( rc == SXRET_OK ){` |
|        - |  2283 | `				SyHashEntry *pRef;` |
|    28202 |  2284 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28202 |  2285 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14102 |  2286 | `				}else{` |
|      ! 0 |  2287 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2288 | `				}` |
|        - |  2289 | `				/* Install in the reference table */` |
|    28202 |  2290 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28202 |  2291 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2292 | `					/* Register in the $GLOBALS array */` |
|    28202 |  2293 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14100 |  2294 | `				}` |
|    14100 |  2295 | `			}` |
|        - |  2296 | `		}` |
|    28202 |  2297 | `		break;` |
|        - |  2298 | `									}` |
|     1540 |  2299 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2300 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2301 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2302 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2303 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2304 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2305 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3082 |  2306 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3082 |  2307 | `		const char *zValue = va_arg(ap,const char *);` |
|     3082 |  2308 | `		int nLen = va_arg(ap,int);` |
|        - |  2309 | `		ph7_hashmap *pMap;` |
|        - |  2310 | `		ph7_value *pValue;` |
|     3082 |  2311 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2312 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2313 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3081 |  2314 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2315 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2316 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3080 |  2317 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2318 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2319 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3080 |  2320 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2321 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2322 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3080 |  2323 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2324 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2325 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3080 |  2326 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2327 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2328 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2329 | `		}else{` |
|        - |  2330 | `			/* Extract the $_SERVER superglobal */` |
|     3080 |  2331 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2332 | `		}` |
|     3082 |  2333 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2334 | `			/* No such entry */` |
|      ! 0 |  2335 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2336 | `			break;` |
|        - |  2337 | `		}` |
|        - |  2338 | `		/* Point to the hashmap */` |
|     3082 |  2339 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2340 | `		/* Perform the insertion */` |
|     3082 |  2341 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3082 |  2342 | `		break;` |
|        - |  2343 | `								   }` |
|       11 |  2344 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2345 | `		/* Script arguments */` |
|       24 |  2346 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2347 | `		ph7_hashmap *pMap;` |
|        - |  2348 | `		ph7_value *pValue;` |
|        - |  2349 | `		sxu32 n;` |
|       24 |  2350 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2351 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2352 | `			break;` |
|        - |  2353 | `		}` |
|        - |  2354 | `		/* Extract the $argv array */` |
|       24 |  2355 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2356 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2357 | `			/* No such entry */` |
|      ! 0 |  2358 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2359 | `			break;` |
|        - |  2360 | `		}` |
|        - |  2361 | `		/* Point to the hashmap */` |
|       24 |  2362 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2363 | `		/* Perform the insertion */` |
|       24 |  2364 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2365 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2366 | `		if( rc == SXRET_OK ){` |
|       24 |  2367 | `			if( pMap->nEntry > 1 ){` |
|        - |  2368 | `				/* Append space separator first */` |
|       18 |  2369 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2370 | `			}` |
|       24 |  2371 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2372 | `		}` |
|       24 |  2373 | `		break;` |
|        - |  2374 | `								  }` |
|      ! 0 |  2375 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2376 | `		/* error_log() consumer */` |
|      ! 0 |  2377 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2378 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2379 | `		break;` |
|        - |  2380 | `										}` |
|      ! 0 |  2381 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2382 | `		/* Script return value */` |
|      ! 0 |  2383 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2384 | `#ifdef UNTRUST` |
|        - |  2385 | `		if( ppValue == 0 ){` |
|        - |  2386 | `			rc = SXERR_CORRUPT;` |
|        - |  2387 | `			break;` |
|        - |  2388 | `		}` |
|        - |  2389 | `#endif` |
|      ! 0 |  2390 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2391 | `		break;` |
|        - |  2392 | `								   }` |
|     2820 |  2393 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2394 | `		/* Register an IO stream device */` |
|     5642 |  2395 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2396 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8460 |  2397 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5642 |  2398 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2399 | `				/* Invalid stream */` |
|      ! 0 |  2400 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2401 | `				break;` |
|        - |  2402 | `		}` |
|     5642 |  2403 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2404 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2822 |  2405 | `			pVm->pDefStream = pStream;` |
|     1410 |  2406 | `		}` |
|        - |  2407 | `		/* Insert in the appropriate container */` |
|     5642 |  2408 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5642 |  2409 | `		break;` |
|        - |  2410 | `								  }` |
|        8 |  2411 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2412 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2413 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2414 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2415 | `#ifdef UNTRUST` |
|        - |  2416 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2417 | `			rc = SXERR_CORRUPT;` |
|        - |  2418 | `			break;` |
|        - |  2419 | `		}` |
|        - |  2420 | `#endif` |
|       16 |  2421 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2422 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2423 | `		break;` |
|        - |  2424 | `									   }` |
|        8 |  2425 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2426 | `		/* Raw HTTP request*/` |
|       16 |  2427 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2428 | `		int nByte = va_arg(ap,int);` |
|       16 |  2429 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2430 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2431 | `			break;` |
|        - |  2432 | `		}` |
|       16 |  2433 | `		if( nByte < 0 ){` |
|        - |  2434 | `			/* Compute length automatically */` |
|      ! 0 |  2435 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2436 | `		}` |
|        - |  2437 | `		/* Process the request */` |
|       16 |  2438 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2439 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2440 | `		if( rc == SXRET_OK ){` |
|       16 |  2441 | `			pVm->bHttpContext = 1;` |
|        8 |  2442 | `		}` |
|       16 |  2443 | `		break;` |
|        - |  2444 | `									}` |
|        8 |  2445 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2446 | `		/* Extract HTTP response status code */` |
|       16 |  2447 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2448 | `		if( pStatus ){` |
|       16 |  2449 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2450 | `		}` |
|       16 |  2451 | `		break;` |
|        - |  2452 | `										}` |
|        8 |  2453 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2454 | `		/* Iterate response headers via callback */` |
|        - |  2455 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2456 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2457 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2458 | `		if( xCallback ){` |
|       16 |  2459 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2460 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2461 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2462 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2463 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2464 | `							   pUserData);` |
|       12 |  2465 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2466 | `					break;` |
|        - |  2467 | `				}` |
|        6 |  2468 | `			}` |
|        8 |  2469 | `		}` |
|       16 |  2470 | `		break;` |
|        - |  2471 | `										 }` |
|      ! 0 |  2472 | `	default:` |
|        - |  2473 | `		/* Unknown configuration option */` |
|      ! 0 |  2474 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2475 | `		break;` |
|        - |  2476 | `	}` |
|    45452 |  2477 | `	return rc;` |
|        2 |  2478 |  |
|        - |  2479 | `/* Forward declaration */` |
|        - |  2480 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2481 | `/*` |
|        - |  2482 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2483 | ` * format.` |
|        - |  2484 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2485 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2486 | ` * (STDOUT).` |
|        - |  2487 | ` */` |
|        2 |  2488 | `static sxi32 VmByteCodeDump(` |
|        - |  2489 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2490 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2491 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2492 | `	)` |
|        1 |  2493 |  |
|        - |  2494 | `	static const char zDump[] = {` |
|        - |  2495 | `		"====================================================\n"` |
|        - |  2496 | `		"PH7 VM Dump\n"` |
|        - |  2497 | `		"====================================================\n"` |
|        - |  2498 | `	};` |
|        - |  2499 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2500 | `	sxi32 rc = SXRET_OK;` |
|        - |  2501 | `	sxu32 n;` |
|        - |  2502 | `	/* Point to the PH7 instructions */` |
|        3 |  2503 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2504 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2505 | `	n = 0;` |
|        3 |  2506 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2507 | `	/* Dump instructions */` |
|        7 |  2508 | `	for(;;){` |
|       15 |  2509 | `		if( pInstr >= pEnd ){` |
|        - |  2510 | `			/* No more instructions */` |
|        3 |  2511 | `			break;` |
|        - |  2512 | `		}` |
|        - |  2513 | `		/* Format and call the consumer callback */` |
|       19 |  2514 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2515 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2516 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2517 | `		if( rc != SXRET_OK ){` |
|        - |  2518 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2519 | `			return rc;` |
|        - |  2520 | `		}` |
|       13 |  2521 | `		++n;` |
|       13 |  2522 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2523 | `	}` |
|        3 |  2524 | `	return rc;` |
|        2 |  2525 |  |
|        - |  2526 | `/* Forward declaration */` |
|        - |  2527 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2528 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2529 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2530 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2531 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2532 | `/*` |
|        - |  2533 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2534 | ` * consumer callback.` |
|        - |  2535 | ` */` |
|      600 |  2536 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2537 |  |
|      601 |  2538 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      601 |  2539 | `	sxi32 rc = SXRET_OK;` |
|        - |  2540 | `	/* Append a new line */` |
|        - |  2541 | `#ifdef __WINNT__` |
|        1 |  2542 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2543 | `#else` |
|      600 |  2544 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2545 | `#endif` |
|        - |  2546 | `	/* Invoke the output consumer callback */` |
|      601 |  2547 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      601 |  2548 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      601 |  2549 | `	return rc;` |
|        1 |  2550 |  |
|        - |  2551 | `/*` |
|        - |  2552 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2553 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2554 | ` * information.` |
|        - |  2555 | ` */` |
|      148 |  2556 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2557 |  |
|      150 |  2558 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2559 | `		ph7_value apArg[4];` |
|        - |  2560 | `		ph7_value *apArgPtr[4];` |
|        - |  2561 | `		ph7_value sResult;` |
|        - |  2562 | `		SyString sErr;` |
|        - |  2563 | `		/* Prepare arguments */` |
|       76 |  2564 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2565 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2566 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2567 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2568 | `		if( pFile ){` |
|       76 |  2569 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2570 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2571 | `		}else{` |
|      ! 0 |  2572 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2573 | `		}` |
|       76 |  2574 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2575 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2576 | `		/* Set up pointer array */` |
|       76 |  2577 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2578 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2579 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2580 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2581 | `		/* Call the handler */` |
|       76 |  2582 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2583 | `		/* Check return value */` |
|       76 |  2584 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2585 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2586 | `		}` |
|        - |  2587 | `		/* Release */` |
|       76 |  2588 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2589 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2590 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2591 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2592 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2593 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2594 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2595 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2596 | `	}` |
|        - |  2597 | `	/* No handler, always call error handler */` |
|       75 |  2598 | `	return TRUE;` |
|       76 |  2599 |  |
|      110 |  2600 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2601 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2602 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2603 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2604 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2605 | `	)` |
|        2 |  2606 |  |
|      112 |  2607 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2608 | `	SyString *pFile;` |
|        - |  2609 | `	char *zErr;` |
|      112 |  2610 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2611 | `	if( !pVm->bErrReport ){` |
|        - |  2612 | `		/* Don't bother reporting errors */` |
|        3 |  2613 | `		return SXRET_OK;` |
|        - |  2614 | `	}` |
|        - |  2615 | `	/* Reset the working buffer */` |
|      110 |  2616 | `	SyBlobReset(pWorker);` |
|        - |  2617 | `	/* Peek the processed file if available */` |
|      110 |  2618 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2619 | `	if( pFile ){` |
|        - |  2620 | `		/* Append file name */` |
|      110 |  2621 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2622 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2623 | `	}` |
|        - |  2624 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2625 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2626 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2627 | `	 * E_DEPRECATED). */` |
|      110 |  2628 | `	zErr = "Error:  ";` |
|      110 |  2629 | `	switch(iErr){` |
|       19 |  2630 | `	case PH7_CTX_WARNING:` |
|       40 |  2631 | `		zErr = "Warning:  ";` |
|       40 |  2632 | `		break;` |
|        6 |  2633 | `	case PH7_CTX_NOTICE:` |
|       14 |  2634 | `		zErr = "Notice:  ";` |
|       12 |  2635 | `		break;` |
|       29 |  2636 | `	default:` |
|        - |  2637 | `		/* keep iErr unchanged */` |
|       58 |  2638 | `		break;` |
|        - |  2639 | `	}` |
|      110 |  2640 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2641 | `	if( pFuncName ){` |
|        - |  2642 | `		/* Append function name first */` |
|       23 |  2643 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2644 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2645 | `	}` |
|      110 |  2646 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2647 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2648 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2649 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2650 | `	}` |
|      110 |  2651 | `	return rc;` |
|       57 |  2652 |  |
|        - |  2653 | `/*` |
|        - |  2654 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2655 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2656 | ` * information.` |
|        - |  2657 | ` */` |
|       40 |  2658 | `static sxi32 VmThrowErrorAp(` |
|        - |  2659 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2660 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2661 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2662 | `	const char *zFormat, /* Format message */` |
|        - |  2663 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2664 | `	)` |
|        2 |  2665 |  |
|       42 |  2666 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2667 | `	SyBlob sMsg;` |
|        - |  2668 | `	SyString *pFile;` |
|        - |  2669 | `	char *zErr;` |
|       42 |  2670 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2671 | `	if( !pVm->bErrReport ){` |
|        - |  2672 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2673 | `		return SXRET_OK;` |
|        - |  2674 | `	}` |
|        - |  2675 | `	/* Reset the working buffer */` |
|       42 |  2676 | `	SyBlobReset(pWorker);` |
|        - |  2677 | `	/* Peek the processed file if available */` |
|       42 |  2678 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2679 | `	if( pFile ){` |
|        - |  2680 | `		/* Append file name */` |
|       42 |  2681 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2682 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2683 | `	}` |
|        - |  2684 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2685 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2686 | `	 * the correct errno value. */` |
|       42 |  2687 | `	zErr = "Error:  ";` |
|       42 |  2688 | `	switch(iErr){` |
|        4 |  2689 | `	case PH7_CTX_WARNING:` |
|        9 |  2690 | `		zErr = "Warning:  ";` |
|        9 |  2691 | `		break;` |
|        3 |  2692 | `	case PH7_CTX_NOTICE:` |
|        7 |  2693 | `		zErr = "Notice:  ";` |
|        6 |  2694 | `		break;` |
|       13 |  2695 | `	default:` |
|        - |  2696 | `		/* do not change iErr */` |
|       26 |  2697 | `		break;` |
|        - |  2698 | `	}` |
|       42 |  2699 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2700 | `	if( pFuncName ){` |
|        - |  2701 | `		/* Append function name first */` |
|       26 |  2702 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2703 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2704 | `	}` |
|        - |  2705 | `	/* Format the raw message */` |
|       42 |  2706 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2707 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2708 | `	/* Check if a user error handler is installed */` |
|       42 |  2709 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2710 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2711 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2712 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2713 | `	}` |
|       42 |  2714 | `	SyBlobRelease(&sMsg);` |
|       42 |  2715 | `	return rc;` |
|       22 |  2716 |  |
|        - |  2717 | `/*` |
|        - |  2718 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2719 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2720 | ` * possible.` |
|        - |  2721 | ` */` |
|       38 |  2722 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2723 |  |
|        - |  2724 | `	ph7_class *pClass;` |
|       39 |  2725 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2726 | `	ph7_class_instance *pThis;` |
|        - |  2727 | `	ph7_class_method *pCons;` |
|        - |  2728 | `	ph7_value sArg;` |
|        - |  2729 | `	ph7_value *apArg[1];` |
|        - |  2730 | `	SyBlob sMsg;` |
|        - |  2731 | `	SyString sMsgStr;` |
|        - |  2732 | `	VmFrame *pFrame;` |
|        - |  2733 | `	sxi32 rc;` |
|       39 |  2734 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2735 | `	if( pClass == 0 ){` |
|      ! 0 |  2736 | `		return PH7_ABORT;` |
|        - |  2737 | `	}` |
|       39 |  2738 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2739 | `	if( pThis == 0 ){` |
|      ! 0 |  2740 | `		return PH7_ABORT;` |
|        - |  2741 | `	}` |
|       39 |  2742 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2743 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2744 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2745 | `	{` |
|       39 |  2746 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2747 | `		if( pOwner ){` |
|       39 |  2748 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2749 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2750 | `		}else{` |
|      ! 0 |  2751 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2752 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2753 | `		}` |
|        - |  2754 | `	}` |
|       39 |  2755 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2756 | `	if( pCons ){` |
|       39 |  2757 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2758 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2759 | `		apArg[0] = &sArg;` |
|       39 |  2760 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2761 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2762 | `	}` |
|       39 |  2763 | `	SyBlobRelease(&sMsg);` |
|       39 |  2764 | `	pFrame = pVm->pFrame;` |
|       39 |  2765 | `	if( pFrame ){` |
|       39 |  2766 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2767 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2768 | `	}` |
|       39 |  2769 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2770 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2771 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2772 | `		return PH7_ABORT;` |
|        - |  2773 | `	}` |
|       39 |  2774 | `	return PH7_EXCEPTION;` |
|       20 |  2775 |  |
|        - |  2776 |  |
|        - |  2777 | `/*` |
|        - |  2778 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2779 | ` */` |
|        4 |  2780 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2781 |  |
|        - |  2782 | `	ph7_class *pErrClass;` |
|        - |  2783 | `	ph7_class_instance *pThis;` |
|        - |  2784 | `	ph7_class_method *pCons;` |
|        - |  2785 | `	ph7_value sArg;` |
|        - |  2786 | `	ph7_value *apArg[1];` |
|        - |  2787 | `	SyBlob sMsg;` |
|        - |  2788 | `	SyString sMsgStr;` |
|        - |  2789 | `	VmFrame *pFrame;` |
|        - |  2790 | `	sxi32 rc;` |
|        5 |  2791 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2792 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2793 | `		return PH7_ABORT;` |
|        - |  2794 | `	}` |
|        5 |  2795 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2796 | `	if( pThis == 0 ){` |
|      ! 0 |  2797 | `		return PH7_ABORT;` |
|        - |  2798 | `	}` |
|        5 |  2799 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2800 | `	{` |
|        5 |  2801 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2802 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2803 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2804 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2805 | `	}` |
|        5 |  2806 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2807 | `	if( pCons ){` |
|        5 |  2808 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2809 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2810 | `		apArg[0] = &sArg;` |
|        5 |  2811 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2812 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2813 | `	}` |
|        5 |  2814 | `	SyBlobRelease(&sMsg);` |
|        5 |  2815 | `	pFrame = pVm->pFrame;` |
|        5 |  2816 | `	if( pFrame ){` |
|        5 |  2817 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2818 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2819 | `	}` |
|        5 |  2820 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2821 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2822 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2823 | `		return PH7_ABORT;` |
|        - |  2824 | `	}` |
|        5 |  2825 | `	return PH7_EXCEPTION;` |
|        3 |  2826 |  |
|        - |  2827 |  |
|        - |  2828 | `/*` |
|        - |  2829 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2830 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2831 | ` * For class types, instanceof is verified.` |
|        - |  2832 | ` *` |
|        - |  2833 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2834 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2835 | ` */` |
|        - |  2836 | `/*` |
|        - |  2837 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2838 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2839 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2840 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2841 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2842 | ` */` |
|       20 |  2843 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2844 |  |
|        - |  2845 | `	const char *z, *zEnd, *zTail;` |
|        - |  2846 | `	sxu32 n;` |
|        - |  2847 | `	sxu8 bReal;` |
|        - |  2848 | `	sxi32 rc;` |
|       22 |  2849 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2850 | `		return 0;` |
|        - |  2851 | `	}` |
|       22 |  2852 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2853 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2854 | `	zEnd = z + n;` |
|       22 |  2855 | `	if( n == 0 ){` |
|      ! 0 |  2856 | `		return 0;` |
|        - |  2857 | `	}` |
|       22 |  2858 | `	zTail = 0;` |
|       22 |  2859 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2860 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2861 | `		return 0;` |
|        - |  2862 | `	}` |
|        - |  2863 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2864 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2865 | `		zTail++;` |
|      ! 0 |  2866 | `	}` |
|       16 |  2867 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2868 |  |
|        - |  2869 |  |
|        - |  2870 | `/*` |
|        - |  2871 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2872 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2873 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2874 | ` *   0 if it's not strictly numeric.` |
|        - |  2875 | ` */` |
|       16 |  2876 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2877 |  |
|        - |  2878 | `	const char *z, *zEnd, *zTail;` |
|        - |  2879 | `	sxu32 n;` |
|       18 |  2880 | `	sxu8 bReal = 0;` |
|        - |  2881 | `	sxi32 rc;` |
|       18 |  2882 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2883 | `		return 0;` |
|        - |  2884 | `	}` |
|       18 |  2885 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2886 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2887 | `	zEnd = z + n;` |
|       18 |  2888 | `	if( n == 0 ) return 0;` |
|       18 |  2889 | `	zTail = 0;` |
|       18 |  2890 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2891 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2892 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2893 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2894 | `	return bReal ? 2 : 1;` |
|       10 |  2895 |  |
|        - |  2896 |  |
|        - |  2897 | `/*` |
|        - |  2898 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2899 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2900 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2901 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2902 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2903 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2904 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2905 | ` * throw.` |
|        - |  2906 | ` *` |
|        - |  2907 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2908 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2909 | ` */` |
|       98 |  2910 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2911 |  |
|        - |  2912 | `	sxu32 i;` |
|        - |  2913 | `	ph7_type_alt *aAlts;` |
|        - |  2914 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2915 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2916 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2917 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2918 | `	}` |
|       88 |  2919 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2920 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2921 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2922 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2923 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2924 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2925 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2926 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2927 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2928 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2929 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2930 | `	}` |
|        - |  2931 | `	/* Object handling */` |
|       88 |  2932 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2933 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2934 | `		if( bHasClassAlt ){` |
|       14 |  2935 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2936 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2937 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2938 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2939 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2940 | `			}` |
|       26 |  2941 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2942 | `				ph7_class *pExpected;` |
|        - |  2943 | `				SyString *pCN;` |
|       22 |  2944 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2945 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2946 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2947 | `					pExpected = pSelfNow;` |
|       22 |  2948 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2949 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2950 | `				}else{` |
|       22 |  2951 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2952 | `				}` |
|       22 |  2953 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2954 | `					return SXRET_OK;` |
|        - |  2955 | `				}` |
|        8 |  2956 | `			}` |
|        2 |  2957 | `		}` |
|        9 |  2958 | `		return SXERR_INVALID;` |
|        - |  2959 | `	}` |
|        - |  2960 | `	/* Array handling */` |
|       72 |  2961 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2962 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2963 | `	}` |
|        - |  2964 | `	/* Scalar handling — exact match first */` |
|       66 |  2965 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2966 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2967 | `	}` |
|       42 |  2968 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2969 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2970 | `	}` |
|       38 |  2971 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2972 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2973 | `	}` |
|       18 |  2974 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2975 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2976 | `	}` |
|       18 |  2977 | `	if( bStrict ){` |
|        - |  2978 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2979 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2980 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2981 | `			return SXRET_OK;` |
|        - |  2982 | `		}` |
|      ! 0 |  2983 | `		return SXERR_INVALID;` |
|        - |  2984 | `	}` |
|        - |  2985 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2986 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2987 | `	 * to match PHP's union RFC. */` |
|        - |  2988 | `	{` |
|       18 |  2989 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2990 | `		if( bHasInt ){` |
|        - |  2991 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2992 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2993 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2994 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2995 | `				return SXRET_OK;` |
|        - |  2996 | `			}` |
|       18 |  2997 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2998 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2999 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3000 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3001 | `					return SXRET_OK;` |
|        - |  3002 | `				}` |
|      ! 0 |  3003 | `			}` |
|       18 |  3004 | `			if( kind == 1 ){` |
|        9 |  3005 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3006 | `				return SXRET_OK;` |
|        - |  3007 | `			}` |
|        4 |  3008 | `		}` |
|       10 |  3009 | `		if( bHasFloat ){` |
|       10 |  3010 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3011 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3012 | `				return SXRET_OK;` |
|        - |  3013 | `			}` |
|       10 |  3014 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3015 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3016 | `				return SXRET_OK;` |
|        - |  3017 | `			}` |
|        1 |  3018 | `		}` |
|        3 |  3019 | `		if( bHasString ){` |
|      ! 0 |  3020 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3021 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3022 | `				return SXRET_OK;` |
|        - |  3023 | `			}` |
|      ! 0 |  3024 | `		}` |
|        3 |  3025 | `		if( bHasBool ){` |
|      ! 0 |  3026 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3027 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3028 | `				return SXRET_OK;` |
|        - |  3029 | `			}` |
|      ! 0 |  3030 | `		}` |
|        - |  3031 | `	}` |
|        3 |  3032 | `	return SXERR_INVALID;` |
|       51 |  3033 |  |
|        - |  3034 |  |
|        - |  3035 | `/*` |
|        - |  3036 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3037 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3038 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3039 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3040 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3041 | ` */` |
|       36 |  3042 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3043 |  |
|       38 |  3044 | `	if( bStrict ){` |
|        - |  3045 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3046 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3047 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3048 | `			return SXRET_OK;` |
|        - |  3049 | `		}` |
|       10 |  3050 | `		return SXERR_INVALID;` |
|        - |  3051 | `	}` |
|        - |  3052 | `	{` |
|       28 |  3053 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3054 | `		if( xCast ) xCast(pVal);` |
|        - |  3055 | `	}` |
|       28 |  3056 | `	return SXRET_OK;` |
|       20 |  3057 |  |
|        - |  3058 |  |
|        - |  3059 | `/*` |
|        - |  3060 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3061 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3062 | ` *` |
|        - |  3063 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3064 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3065 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3066 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3067 | ` */` |
|       10 |  3068 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3069 |  |
|       12 |  3070 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       12 |  3071 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       12 |  3072 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       12 |  3073 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       12 |  3074 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        5 |  3075 | `		}` |
|       12 |  3076 | `		zBuf[nCopy] = 0;` |
|       12 |  3077 | `		return zBuf;` |
|        - |  3078 | `	}` |
|      ! 0 |  3079 | `	switch( nType ){` |
|      ! 0 |  3080 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3081 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3082 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3083 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3084 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3085 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3086 | `		default:             return "scalar";` |
|        - |  3087 | `	}` |
|        7 |  3088 |  |
|        - |  3089 |  |
|        - |  3090 | `/*` |
|        - |  3091 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3092 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3093 | ` */` |
|       18 |  3094 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3095 |  |
|       19 |  3096 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3097 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3098 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3099 | `	return zBuf;` |
|        1 |  3100 |  |
|        - |  3101 |  |
|    14488 |  3102 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3103 |  |
|        - |  3104 | `	SyHashEntry *pSlot;` |
|        - |  3105 | `	VmClassAttr *pVmAttr;` |
|        - |  3106 | `	ph7_class_attr *pAttr;` |
|    14490 |  3107 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14490 |  3108 | `	if( pSlot == 0 ){` |
|    14282 |  3109 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3110 | `	}` |
|      210 |  3111 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3112 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3113 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3114 | `		return SXRET_OK;` |
|        - |  3115 | `	}` |
|        - |  3116 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3117 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3118 | `	 * matching PHP's documented behavior. */` |
|      210 |  3119 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3120 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3121 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3122 |  |
|       16 |  3123 | `		if( rc == SXRET_OK ){` |
|        9 |  3124 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3125 | `			return SXRET_OK;` |
|        - |  3126 | `		}` |
|        7 |  3127 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3128 | `			char zBuf[128];` |
|        4 |  3129 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3130 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3131 | `		}` |
|        5 |  3132 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3133 | `	}` |
|        - |  3134 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3135 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3136 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3137 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3138 | `			return SXRET_OK;` |
|        - |  3139 | `		}` |
|        3 |  3140 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3141 | `	}` |
|        - |  3142 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3143 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3144 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3145 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3146 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3147 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3148 | `			return SXRET_OK;` |
|        - |  3149 | `		}` |
|        7 |  3150 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3151 | `	}` |
|      174 |  3152 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3153 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3154 | `		 * currently active on the self-stack. */` |
|       26 |  3155 | `		ph7_class *pExpected = 0;` |
|       26 |  3156 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3157 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3158 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3159 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3160 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3161 | `		}` |
|       26 |  3162 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3163 | `			pExpected = pSelfNow;` |
|       24 |  3164 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3165 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3166 | `		}else{` |
|       22 |  3167 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3168 | `		}` |
|       26 |  3169 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3170 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3171 | `		}` |
|       26 |  3172 | `		if( pExpected ){` |
|       22 |  3173 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3174 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3175 | `				char zBuf[128];` |
|        7 |  3176 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3177 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3178 | `			}` |
|        8 |  3179 | `		}` |
|       22 |  3180 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3181 | `		return SXRET_OK;` |
|        - |  3182 | `	}` |
|        - |  3183 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3184 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3185 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3186 | `		char zBuf[128];` |
|       10 |  3187 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3188 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3189 | `	}` |
|      144 |  3190 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3191 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3192 | `		if( xCast ){` |
|        - |  3193 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3194 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3195 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3196 | `			}` |
|       24 |  3197 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3198 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3199 | `			}` |
|        - |  3200 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3201 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3202 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3203 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3204 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3205 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3206 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3207 | `			}` |
|       12 |  3208 | `			xCast(pValue);` |
|        5 |  3209 | `		}` |
|        5 |  3210 | `	}` |
|      130 |  3211 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3212 | `	return SXRET_OK;` |
|     7246 |  3213 |  |
|        - |  3214 |  |
|        - |  3215 | `/*` |
|        - |  3216 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3217 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3218 | ` * information.` |
|        - |  3219 | ` * ------------------------------------` |
|        - |  3220 | ` * Simple boring wrapper function.` |
|        - |  3221 | ` * ------------------------------------` |
|        - |  3222 | ` */` |
|       16 |  3223 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3224 |  |
|        - |  3225 | `	va_list ap;` |
|        - |  3226 | `	sxi32 rc;` |
|       17 |  3227 | `	va_start(ap,zFormat);` |
|       17 |  3228 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3229 | `	va_end(ap);` |
|       17 |  3230 | `	return rc;` |
|        1 |  3231 |  |
|        - |  3232 | `/*` |
|        - |  3233 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3234 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3235 | ` */` |
|       36 |  3236 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3237 |  |
|        - |  3238 | `	ph7_class *pClass;` |
|        - |  3239 | `	ph7_class_instance *pThis;` |
|        - |  3240 | `	ph7_class_method *pCons;` |
|        - |  3241 | `	ph7_value sArg;` |
|        - |  3242 | `	ph7_value *apArg[1];` |
|        - |  3243 | `	SyBlob sMsg;` |
|        - |  3244 | `	SyString sMsgStr;` |
|        - |  3245 | `	VmFrame *pFrame;` |
|        - |  3246 | `	sxi32 rc;` |
|       38 |  3247 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       38 |  3248 | `	if( pClass == 0 ){` |
|      ! 0 |  3249 | `		return PH7_ABORT;` |
|        - |  3250 | `	}` |
|       38 |  3251 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       38 |  3252 | `	if( pThis == 0 ){` |
|      ! 0 |  3253 | `		return PH7_ABORT;` |
|        - |  3254 | `	}` |
|       38 |  3255 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       38 |  3256 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       18 |  3257 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       38 |  3258 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       38 |  3259 | `	if( pCons ){` |
|       38 |  3260 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       38 |  3261 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       38 |  3262 | `		apArg[0] = &sArg;` |
|       38 |  3263 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       38 |  3264 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  3265 | `	}` |
|       38 |  3266 | `	SyBlobRelease(&sMsg);` |
|       38 |  3267 | `	pFrame = pVm->pFrame;` |
|       38 |  3268 | `	if( pFrame ){` |
|       38 |  3269 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  3270 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  3271 | `	}` |
|       38 |  3272 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  3273 | `	PH7_ClassInstanceUnref(pThis);` |
|       38 |  3274 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3275 | `		return PH7_ABORT;` |
|        - |  3276 | `	}` |
|       34 |  3277 | `	return PH7_EXCEPTION;` |
|       20 |  3278 |  |
|        - |  3279 | `/*` |
|        - |  3280 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3281 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3282 | ` */` |
|        6 |  3283 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3284 |  |
|        - |  3285 | `	ph7_class *pClass;` |
|        - |  3286 | `	ph7_class_instance *pThis;` |
|        - |  3287 | `	ph7_class_method *pCons;` |
|        - |  3288 | `	ph7_value sArg;` |
|        - |  3289 | `	ph7_value *apArg[1];` |
|        - |  3290 | `	SyBlob sMsg;` |
|        - |  3291 | `	SyString sMsgStr;` |
|        - |  3292 | `	VmFrame *pFrame;` |
|        - |  3293 | `	sxi32 rc;` |
|        7 |  3294 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3295 | `	if( pClass == 0 ){` |
|      ! 0 |  3296 | `		return PH7_ABORT;` |
|        - |  3297 | `	}` |
|        7 |  3298 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3299 | `	if( pThis == 0 ){` |
|      ! 0 |  3300 | `		return PH7_ABORT;` |
|        - |  3301 | `	}` |
|        7 |  3302 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3303 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3304 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3305 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3306 | `	if( pCons ){` |
|        7 |  3307 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3308 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3309 | `		apArg[0] = &sArg;` |
|        7 |  3310 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3311 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3312 | `	}` |
|        7 |  3313 | `	SyBlobRelease(&sMsg);` |
|        7 |  3314 | `	pFrame = pVm->pFrame;` |
|        7 |  3315 | `	if( pFrame ){` |
|        7 |  3316 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3317 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3318 | `	}` |
|        7 |  3319 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3320 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3321 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3322 | `		return PH7_ABORT;` |
|        - |  3323 | `	}` |
|      ! 0 |  3324 | `	return PH7_EXCEPTION;` |
|        4 |  3325 |  |
|        - |  3326 | `/*` |
|        - |  3327 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3328 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3329 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3330 | ` */` |
|       16 |  3331 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3332 |  |
|       17 |  3333 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3334 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3335 | `	}` |
|       13 |  3336 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3337 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3338 | `		if( pThis && pThis->pClass ){` |
|        5 |  3339 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3340 | `			sxu32 n = pName->nByte;` |
|        5 |  3341 | `			if( n >= nBuf ){` |
|      ! 0 |  3342 | `				n = nBuf - 1;` |
|      ! 0 |  3343 | `			}` |
|        5 |  3344 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3345 | `			zBuf[n] = 0;` |
|        5 |  3346 | `			return zBuf;` |
|        - |  3347 | `		}` |
|      ! 0 |  3348 | `		return "object";` |
|        - |  3349 | `	}` |
|        9 |  3350 | `	return ph7_type_name(pVal);` |
|        9 |  3351 |  |
|        - |  3352 | `/*` |
|        - |  3353 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3354 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3355 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3356 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3357 | ` */` |
|       16 |  3358 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3359 |  |
|        - |  3360 | `	ph7_class *pClass;` |
|        - |  3361 | `	ph7_class_instance *pThis;` |
|        - |  3362 | `	ph7_class_method *pCons;` |
|        - |  3363 | `	ph7_value sArg;` |
|        - |  3364 | `	ph7_value *apArg[1];` |
|        - |  3365 | `	SyBlob sMsg;` |
|        - |  3366 | `	SyString sMsgStr;` |
|        - |  3367 | `	VmFrame *pFrame;` |
|        - |  3368 | `	sxi32 rc;` |
|       17 |  3369 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3370 | `	char zNameBuf[64];` |
|       17 |  3371 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3372 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3373 | `	if( pClass == 0 ){` |
|      ! 0 |  3374 | `		return PH7_ABORT;` |
|        - |  3375 | `	}` |
|       17 |  3376 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3377 | `	if( pThis == 0 ){` |
|      ! 0 |  3378 | `		return PH7_ABORT;` |
|        - |  3379 | `	}` |
|       17 |  3380 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3381 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3382 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3383 | `	if( pCons ){` |
|       17 |  3384 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3385 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3386 | `		apArg[0] = &sArg;` |
|       17 |  3387 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3388 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3389 | `	}` |
|       17 |  3390 | `	SyBlobRelease(&sMsg);` |
|       17 |  3391 | `	pFrame = pVm->pFrame;` |
|       17 |  3392 | `	if( pFrame ){` |
|       17 |  3393 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3394 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3395 | `	}` |
|       17 |  3396 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3397 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3398 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3399 | `		return PH7_ABORT;` |
|        - |  3400 | `	}` |
|       17 |  3401 | `	return PH7_EXCEPTION;` |
|        9 |  3402 |  |
|        - |  3403 | `/*` |
|        - |  3404 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3405 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3406 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3407 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3408 | ` */` |
|        - |  3409 | `/*` |
|        - |  3410 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3411 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3412 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3413 | ` */` |
|       24 |  3414 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3415 |  |
|        - |  3416 | `	sxu32 nCopy;` |
|       26 |  3417 | `	if( nBuf == 0 ) return "";` |
|       26 |  3418 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3419 | `		zBuf[0] = 0;` |
|      ! 0 |  3420 | `		return zBuf;` |
|        - |  3421 | `	}` |
|       26 |  3422 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3423 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3424 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3425 | `	zBuf[nCopy] = 0;` |
|       26 |  3426 | `	return zBuf;` |
|       14 |  3427 |  |
|        - |  3428 |  |
|      376 |  3429 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3430 |  |
|      378 |  3431 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3432 | `	const char *zGiven;` |
|        - |  3433 | `	char zBuf[128];` |
|        - |  3434 | `	char zTypeBuf[128];` |
|        - |  3435 | `	/* Untyped function: no enforcement. */` |
|      378 |  3436 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3437 | `		return SXRET_OK;` |
|        - |  3438 | `	}` |
|        - |  3439 | `	/* void return type: the function must not produce a value. */` |
|      378 |  3440 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3441 | `		if( pValue == 0 ){` |
|      134 |  3442 | `			return SXRET_OK;` |
|        - |  3443 | `		}` |
|        - |  3444 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3445 | `		 * still counts as "returned a value" here. */` |
|        3 |  3446 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3447 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3448 | `	}` |
|        - |  3449 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3450 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      244 |  3451 | `	if( pValue == 0 ){` |
|      ! 0 |  3452 | `		const char *zExpected = "value";` |
|      ! 0 |  3453 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3454 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3455 | `		}` |
|      ! 0 |  3456 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3457 | `	}` |
|        - |  3458 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3459 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3460 | `	 * bNullable=0 here. */` |
|      244 |  3461 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3462 | `		sxi32 rcU;` |
|      ! 0 |  3463 | `		int bNullable = 0;` |
|      ! 0 |  3464 | `		const char *zExpected = "union";` |
|        - |  3465 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3466 | `		{` |
|        - |  3467 | `			sxu32 i;` |
|      ! 0 |  3468 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3469 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3470 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3471 | `			}` |
|        - |  3472 | `		}` |
|      ! 0 |  3473 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3474 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3475 | `			return SXRET_OK;` |
|        - |  3476 | `		}` |
|      ! 0 |  3477 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3478 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3479 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3480 | `			zGiven = "null";` |
|      ! 0 |  3481 | `		}else{` |
|      ! 0 |  3482 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3483 | `		}` |
|      ! 0 |  3484 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3485 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3486 | `		}` |
|      ! 0 |  3487 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3488 | `	}` |
|        - |  3489 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3490 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3491 | `	 * it into the TypeError message. */` |
|      244 |  3492 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3493 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3494 | `		const char *zExpected;` |
|        - |  3495 | `		ph7_class *pExpected;` |
|        6 |  3496 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3497 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3498 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3499 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3500 | `		}` |
|        6 |  3501 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3502 | `			pExpected = pSelfNow;` |
|        4 |  3503 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3504 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3505 | `		}else{` |
|        3 |  3506 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3507 | `		}` |
|        6 |  3508 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3509 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3510 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3511 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3512 | `		}` |
|        6 |  3513 | `		if( pExpected ){` |
|        6 |  3514 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3515 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3516 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3517 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3518 | `			}` |
|        2 |  3519 | `		}` |
|        6 |  3520 | `		return SXRET_OK;` |
|        - |  3521 | `	}` |
|        - |  3522 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3523 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3524 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3525 | `	 * via the type-text leading '?'. */` |
|      240 |  3526 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3527 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3528 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3529 | `			return SXRET_OK;` |
|        - |  3530 | `		}` |
|      ! 0 |  3531 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3532 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3533 | `			"null");` |
|        - |  3534 | `	}` |
|        - |  3535 | `	/* Exact match? Done. */` |
|      234 |  3536 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3537 | `		return SXRET_OK;` |
|        - |  3538 | `	}` |
|        - |  3539 | `	/* Object->scalar is never compatible. */` |
|        8 |  3540 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3541 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3542 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3543 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3544 | `			zGiven);` |
|        - |  3545 | `	}` |
|        - |  3546 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3547 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3548 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3549 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3550 | `			ph7_type_name(pValue));` |
|        - |  3551 | `	}` |
|        - |  3552 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3553 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3554 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3555 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3556 | `	if( !bStrict` |
|        5 |  3557 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3558 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3559 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3560 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3561 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3562 | `			"string");` |
|        - |  3563 | `	}` |
|        6 |  3564 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3565 | `		return SXRET_OK;` |
|        - |  3566 | `	}` |
|        4 |  3567 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3568 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3569 | `		ph7_type_name(pValue));` |
|      190 |  3570 |  |
|        - |  3571 | `/*` |
|        - |  3572 | ` * Report a fatal named-argument error.` |
|        - |  3573 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3574 | ` */` |
|        6 |  3575 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3576 |  |
|        7 |  3577 | `	const char *zFunc = 0;` |
|        7 |  3578 | `	int nFunc = 0;` |
|        7 |  3579 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3580 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3581 |  |
|        - |  3582 | `/*` |
|        - |  3583 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3584 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3585 | ` * information.` |
|        - |  3586 | ` * ------------------------------------` |
|        - |  3587 | ` * Simple boring wrapper function.` |
|        - |  3588 | ` * ------------------------------------` |
|        - |  3589 | ` */` |
|       24 |  3590 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3591 |  |
|        - |  3592 | `	sxi32 rc;` |
|       26 |  3593 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3594 | `	return rc;` |
|        2 |  3595 |  |
|        - |  3596 | `/*` |
|        - |  3597 | ` * Resolve function context from the current frame.` |
|        - |  3598 | ` */` |
|     1018 |  3599 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3600 |  |
|        - |  3601 | `	VmFrame *pFrame;` |
|        - |  3602 | `	ph7_vm_func *pFunc;` |
|     1019 |  3603 | `	*pzFuncName = 0;` |
|     1019 |  3604 | `	*pnFuncLen = 0;` |
|     1019 |  3605 | `	pFrame = pVm->pFrame;` |
|     1019 |  3606 | `	if( pFrame == 0 ){` |
|      ! 0 |  3607 | `		return;` |
|        - |  3608 | `	}` |
|     1019 |  3609 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3610 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3611 | `		return;` |
|        - |  3612 | `	}` |
|       25 |  3613 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3614 | `	if( pFunc == 0 ){` |
|      ! 0 |  3615 | `		return;` |
|        - |  3616 | `	}` |
|       25 |  3617 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3618 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3619 |  |
|        - |  3620 | `/*` |
|        - |  3621 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3622 | ` */` |
|      524 |  3623 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3624 |  |
|        - |  3625 | `	SyBlob sOut;` |
|        - |  3626 | `	SyString *pFile;` |
|      525 |  3627 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3628 | `		return PH7_OK;` |
|        - |  3629 | `	}` |
|      525 |  3630 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3631 | `		zClass = "Exception";` |
|      ! 0 |  3632 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3633 | `	}` |
|      525 |  3634 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3635 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3636 | `	}` |
|      525 |  3637 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3638 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3639 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3640 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3641 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3642 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3643 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3644 | `	}` |
|      525 |  3645 | `	if( pFile ){` |
|      525 |  3646 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  3647 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3648 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  3649 | `	}` |
|      525 |  3650 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  3651 | `	if( pFile ){` |
|      525 |  3652 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  3653 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3654 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3655 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3656 | `		}else{` |
|      501 |  3657 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3658 | `		}` |
|      262 |  3659 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3660 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3661 | `	}else{` |
|      ! 0 |  3662 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3663 | `	}` |
|      525 |  3664 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  3665 | `	if( pFile ){` |
|      525 |  3666 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  3667 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  3668 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3669 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  3670 | `	}` |
|      525 |  3671 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  3672 | `	SyBlobRelease(&sOut);` |
|      525 |  3673 | `	return PH7_ABORT;` |
|      263 |  3674 |  |
|        - |  3675 | `/*` |
|        - |  3676 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  3677 | ` *` |
|        - |  3678 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  3679 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  3680 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  3681 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  3682 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  3683 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  3684 | ` */` |
|      858 |  3685 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  3686 |  |
|      860 |  3687 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  3688 | `		if( pVm->pCoalesceObj ){` |
|        7 |  3689 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  3690 | `		}` |
|        7 |  3691 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  3692 | `		pVm->pCoalesceObj = 0;` |
|        7 |  3693 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  3694 | `	}` |
|      860 |  3695 |  |
|        - |  3696 | `/*` |
|        - |  3697 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  3698 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  3699 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  3700 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  3701 | ` *` |
|        - |  3702 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  3703 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  3704 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  3705 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  3706 | ` */` |
|        4 |  3707 | `static sxi32 VmThrowFromVm(` |
|        - |  3708 | `	ph7_vm *pVm,` |
|        - |  3709 | `	const char *zClass,` |
|        - |  3710 | `	const char *zMsg,` |
|        - |  3711 | `	sxu32 nMsg` |
|        1 |  3712 | `){` |
|        - |  3713 | `	ph7_class *pClass;` |
|        - |  3714 | `	ph7_class_instance *pThis;` |
|        - |  3715 | `	ph7_class_method *pCons;` |
|        - |  3716 | `	VmFrame *pFrame;` |
|        - |  3717 | `	sxi32 rc;` |
|        5 |  3718 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  3719 | `	if( pClass == 0 ){` |
|      ! 0 |  3720 | `		return SXERR_ABORT;` |
|        - |  3721 | `	}` |
|        5 |  3722 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  3723 | `	if( pThis == 0 ){` |
|      ! 0 |  3724 | `		return SXERR_ABORT;` |
|        - |  3725 | `	}` |
|        5 |  3726 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3727 | `	if( pCons ){` |
|        - |  3728 | `		ph7_value sArg;` |
|        - |  3729 | `		ph7_value *apArg[1];` |
|        - |  3730 | `		SyString sMsgStr;` |
|        5 |  3731 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  3732 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  3733 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  3734 | `		apArg[0] = &sArg;` |
|        5 |  3735 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  3736 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3737 | `	}` |
|        5 |  3738 | `	pFrame = pVm->pFrame;` |
|        5 |  3739 | `	if( pFrame ){` |
|        5 |  3740 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3741 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3742 | `	}` |
|        5 |  3743 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  3744 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3745 | `	return rc;` |
|        3 |  3746 |  |
|        - |  3747 | `/*` |
|        - |  3748 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3749 | ` */` |
|      574 |  3750 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3751 |  |
|        - |  3752 | `	ph7_vm *pVm;` |
|        - |  3753 | `	ph7_class *pClass;` |
|        - |  3754 | `	ph7_class_instance *pThis;` |
|        - |  3755 | `	ph7_class_method *pCons;` |
|        - |  3756 | `	ph7_value sArg;` |
|        - |  3757 | `	ph7_value *apArg[1];` |
|        - |  3758 | `	SyBlob sMsg;` |
|        - |  3759 | `	SyString sMsgStr;` |
|        - |  3760 | `	VmFrame *pFrame;` |
|        - |  3761 | `	va_list ap;` |
|        - |  3762 | `	sxi32 rc;` |
|        - |  3763 |  |
|      576 |  3764 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3765 | `		return PH7_ABORT;` |
|        - |  3766 | `	}` |
|      576 |  3767 | `	pVm = pCtx->pVm;` |
|      576 |  3768 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3769 | `		zClass = "Error";` |
|      ! 0 |  3770 | `	}` |
|      576 |  3771 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  3772 | `	if( pClass == 0 ){` |
|      ! 0 |  3773 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3774 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3775 | `			zClass` |
|        - |  3776 | `			);` |
|        - |  3777 | `	}` |
|      576 |  3778 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  3779 | `	if( pThis == 0 ){` |
|      ! 0 |  3780 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3781 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3782 | `			);` |
|        - |  3783 | `	}` |
|        - |  3784 |  |
|      576 |  3785 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  3786 | `	va_start(ap,zFormat);` |
|      576 |  3787 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  3788 | `	va_end(ap);` |
|        - |  3789 |  |
|      576 |  3790 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  3791 | `	if( pCons ){` |
|      576 |  3792 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  3793 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  3794 | `		apArg[0] = &sArg;` |
|      576 |  3795 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  3796 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  3797 | `	}` |
|      576 |  3798 | `	SyBlobRelease(&sMsg);` |
|        - |  3799 |  |
|      576 |  3800 | `	pFrame = pVm->pFrame;` |
|      576 |  3801 | `	if( pFrame ){` |
|      576 |  3802 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  3803 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  3804 | `	}` |
|      576 |  3805 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  3806 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  3807 | `	if( rc == SXERR_ABORT ){` |
|      491 |  3808 | `		return PH7_ABORT;` |
|        - |  3809 | `	}` |
|       86 |  3810 | `	return PH7_EXCEPTION;` |
|      289 |  3811 |  |
|        - |  3812 | `/*` |
|        - |  3813 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3814 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3815 | ` */` |
|      ! 0 |  3816 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3817 |  |
|        - |  3818 | `	ph7_vm *pVm;` |
|        - |  3819 | `	SyBlob sMsg;` |
|      ! 0 |  3820 | `	const char *zFuncName = 0;` |
|      ! 0 |  3821 | `	int nFuncLen = 0;` |
|        - |  3822 | `	va_list ap;` |
|        - |  3823 | `	sxi32 rc;` |
|        - |  3824 |  |
|      ! 0 |  3825 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3826 | `		return PH7_OK;` |
|        - |  3827 | `	}` |
|      ! 0 |  3828 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3829 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3830 | `		zClass = "Error";` |
|      ! 0 |  3831 | `	}` |
|        - |  3832 |  |
|      ! 0 |  3833 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3834 |  |
|      ! 0 |  3835 | `	va_start(ap,zFormat);` |
|      ! 0 |  3836 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3837 | `	va_end(ap);` |
|        - |  3838 |  |
|      ! 0 |  3839 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3840 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3841 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3842 | `	}` |
|      ! 0 |  3843 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3844 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3845 | `	}` |
|      ! 0 |  3846 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3847 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3848 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3849 | `	return rc;` |
|      ! 0 |  3850 |  |
|        - |  3851 | `/*` |
|        - |  3852 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3853 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3854 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3855 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3856 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3857 | ` * when VmByteCodeExec returns.` |
|        - |  3858 | ` */` |
|      144 |  3859 | `static sxi32 VmSuspendCtx(` |
|        - |  3860 | `	ph7_vm *pVm,` |
|        - |  3861 | `	ph7_exec_ctx *pCtx,` |
|        - |  3862 | `	sxi32 pc,` |
|        - |  3863 | `	sxi32 nTos` |
|        - |  3864 | `	)` |
|        2 |  3865 |  |
|       72 |  3866 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3867 | `	pCtx->pc = pc;` |
|      146 |  3868 | `	pCtx->nTos = nTos;` |
|      146 |  3869 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3870 | `	return PH7_SUSPEND;` |
|        2 |  3871 |  |
|        - |  3872 | `/*` |
|        - |  3873 | ` * Resolve named-argument mapping.` |
|        - |  3874 | ` *` |
|        - |  3875 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3876 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3877 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3878 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3879 | ` * every formal parameter that received a value.` |
|        - |  3880 | ` *` |
|        - |  3881 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3882 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3883 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3884 | ` */` |
|       98 |  3885 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3886 | `	ph7_vm *pVm,` |
|        - |  3887 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3888 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3889 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3890 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3891 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3892 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3893 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3894 |  |
|        2 |  3895 |  |
|      100 |  3896 | `	sxi32 posIdx = 0;` |
|        - |  3897 | `	sxu32 i;` |
|        - |  3898 | `	char zErrMsg[256];` |
|      100 |  3899 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3900 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3901 | `		aSlot[i] = -2;` |
|      100 |  3902 | `	}` |
|      290 |  3903 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3904 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3905 | `			/* Named argument — find formal by name */` |
|      184 |  3906 | `			int found = 0;` |
|        - |  3907 | `			sxu32 k;` |
|      304 |  3908 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3909 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3910 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3911 | `						pMap->aNames[i].zString,` |
|      402 |  3912 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3913 | `					if( aUsed[k] ){` |
|        7 |  3914 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3915 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3916 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3917 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3918 | `						return PH7_ABORT;` |
|        - |  3919 | `					}` |
|      168 |  3920 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3921 | `					aUsed[k] = 1;` |
|      168 |  3922 | `					found = 1;` |
|      168 |  3923 | `					break;` |
|        - |  3924 | `				}` |
|       62 |  3925 | `			}` |
|      180 |  3926 | `			if( !found ){` |
|       14 |  3927 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3928 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3929 | `				}else{` |
|        4 |  3930 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3931 | `						"Unknown named parameter $%.*s",` |
|        2 |  3932 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3933 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3934 | `					return PH7_ABORT;` |
|        - |  3935 | `				}` |
|        5 |  3936 | `			}` |
|       90 |  3937 | `		}else{` |
|        - |  3938 | `			/* Positional argument */` |
|       16 |  3939 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3940 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3941 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3942 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3943 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3944 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3945 | `					return PH7_ABORT;` |
|        - |  3946 | `				}` |
|       16 |  3947 | `				aSlot[i] = posIdx;` |
|       16 |  3948 | `				aUsed[posIdx] = 1;` |
|        7 |  3949 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3950 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3951 | `			}` |
|       16 |  3952 | `			posIdx++;` |
|        - |  3953 | `		}` |
|       97 |  3954 | `	}` |
|       93 |  3955 | `	return SXRET_OK;` |
|       51 |  3956 |  |
|        - |  3957 | `/*` |
|        - |  3958 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3959 | ` *` |
|        - |  3960 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3961 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3962 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3963 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3964 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3965 | ` * then the program execution is halted.` |
|        - |  3966 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3967 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3968 | ` * or to reset the VM to it's initial state.` |
|        - |  3969 | ` */` |
|    44640 |  3970 | `static sxi32 VmByteCodeExec(` |
|        - |  3971 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3972 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3973 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3974 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3975 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3976 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3977 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3978 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3979 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3980 | `	)` |
|        2 |  3981 |  |
|        - |  3982 | `	VmInstr *pInstr;` |
|        - |  3983 | `	ph7_value *pTos;` |
|        - |  3984 | `	SySet aArg;` |
|        - |  3985 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3986 | `	sxi32 pc;` |
|        - |  3987 | `	sxi32 rc;` |
|        - |  3988 | `	/* Argument container */` |
|    44642 |  3989 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    44642 |  3990 | `	if( nTos < 0 ){` |
|    41518 |  3991 | `		pTos = &pStack[-1];` |
|    20760 |  3992 | `	}else{` |
|     3126 |  3993 | `		pTos = &pStack[nTos];` |
|        - |  3994 | `	}` |
|    44642 |  3995 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    44642 |  3996 | `	pc = nPc;` |
|        - |  3997 | `/*` |
|        - |  3998 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3999 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4000 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4001 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4002 | ` */` |
|        - |  4003 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4004 | `	{ \` |
|        - |  4005 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4006 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4007 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4008 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4009 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4010 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4011 | `				break; \` |
|        - |  4012 | `			} \` |
|        - |  4013 | `			goto Exception; \` |
|        - |  4014 | `		} \` |
|        - |  4015 | `	}` |
|        - |  4016 | `	/* Execute as much as we can */` |
|  5879338 |  4017 | `	for(;;){` |
|        - |  4018 | `		/* Fetch the instruction to execute */` |
| 11757974 |  4019 | `		pInstr = &aInstr[pc];` |
| 11757974 |  4020 | `		rc = SXRET_OK;` |
|        - |  4021 | `/*` |
|        - |  4022 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4023 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4024 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4025 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4026 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4027 | ` */` |
| 11757974 |  4028 | `		switch(pInstr->iOp){` |
|        - |  4029 | `/*` |
|        - |  4030 | ` * DONE: P1 * *` |
|        - |  4031 | ` *` |
|        - |  4032 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4033 | ` * and return immediately.` |
|        - |  4034 | ` */` |
|    21944 |  4035 | `case PH7_OP_DONE:` |
|        - |  4036 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4037 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4038 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4039 | `	 * callback trampolines, and the main script. */` |
|    43890 |  4040 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      378 |  4041 | `		ph7_value *pRetVal = 0;` |
|      378 |  4042 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      246 |  4043 | `			pRetVal = pTos;` |
|      122 |  4044 | `		}` |
|      378 |  4045 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      378 |  4046 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      372 |  4047 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4048 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4049 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4050 | `				pTos--;` |
|      ! 0 |  4051 | `			}` |
|      ! 0 |  4052 | `			goto Exception;` |
|        - |  4053 | `		}` |
|        - |  4054 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4055 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4056 | `		 * defensively we clear the pointer after a successful check). */` |
|      372 |  4057 | `		pEnforceRetFunc = 0;` |
|      185 |  4058 | `	}` |
|    43884 |  4059 | `	if( pInstr->iP1 ){` |
|        - |  4060 | `#ifdef UNTRUST` |
|        - |  4061 | `		if( pTos < pStack ){` |
|        - |  4062 | `			goto Abort;` |
|        - |  4063 | `		}` |
|        - |  4064 | `#endif` |
|    26676 |  4065 | `		if( pLastRef ){` |
|    16324 |  4066 | `			*pLastRef = pTos->nIdx;` |
|     8161 |  4067 | `		}` |
|    26676 |  4068 | `		if( pResult ){` |
|        - |  4069 | `			/* Execution result */` |
|    25212 |  4070 | `			PH7_MemObjStore(pTos,pResult);` |
|    12605 |  4071 | `		}` |
|    26676 |  4072 | `		VmPopOperand(&pTos,1);` |
|    30547 |  4073 | `	}else if( pLastRef ){` |
|        - |  4074 | `		/* Nothing referenced */` |
|     1920 |  4075 | `		*pLastRef = SXU32_HIGH;` |
|      959 |  4076 | `	}` |
|        - |  4077 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4078 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4079 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4080 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4081 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4082 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4083 | `	 * block can override it.` |
|        - |  4084 | `	 */` |
|    43886 |  4085 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4086 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4087 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4088 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4089 | `		pExc->pFrame = 0;` |
|        3 |  4090 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4091 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4092 | `			pExc->iFinallyDone = 1;` |
|        - |  4093 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4094 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4095 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4096 | `				goto Abort;` |
|        - |  4097 | `			}` |
|        1 |  4098 | `		}` |
|        1 |  4099 | `	}` |
|    43884 |  4100 | `	goto Done;` |
|        - |  4101 | `/*` |
|        - |  4102 | ` * HALT: P1 * *` |
|        - |  4103 | ` *` |
|        - |  4104 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4105 | ` * and abort immediately.` |
|        - |  4106 | ` */` |
|        7 |  4107 | `case PH7_OP_HALT:` |
|       15 |  4108 | `	if( pInstr->iP1 ){` |
|        - |  4109 | `#ifdef UNTRUST` |
|        - |  4110 | `		if( pTos < pStack ){` |
|        - |  4111 | `			goto Abort;` |
|        - |  4112 | `		}` |
|        - |  4113 | `#endif` |
|       15 |  4114 | `		if( pLastRef ){` |
|        3 |  4115 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4116 | `		}` |
|       15 |  4117 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4118 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4119 | `				/* Output the exit message */` |
|       16 |  4120 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4121 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4122 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4123 | `			}` |
|       10 |  4124 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4125 | `			/* Record exit status */` |
|        5 |  4126 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4127 | `		}` |
|       15 |  4128 | `		VmPopOperand(&pTos,1);` |
|        7 |  4129 | `	}else if( pLastRef ){` |
|        - |  4130 | `		/* Nothing referenced */` |
|      ! 0 |  4131 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4132 | `	}` |
|        - |  4133 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4134 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4135 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4136 | `	 */` |
|       15 |  4137 | `	pVm->bHaltRequested = 1;` |
|       15 |  4138 | `	goto Abort;` |
|        - |  4139 | `/*` |
|        - |  4140 | ` * JMP: * P2 *` |
|        - |  4141 | ` *` |
|        - |  4142 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4143 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4144 | ` */` |
|   250464 |  4145 | `case PH7_OP_JMP:` |
|   500974 |  4146 | `	pc = pInstr->iP2 - 1;` |
|   500974 |  4147 | `	break;` |
|        - |  4148 | `/*` |
|        - |  4149 | ` * JZ: P1 P2 *` |
|        - |  4150 | ` *` |
|        - |  4151 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4152 | ` * entry in the stack if P1 is zero.` |
|        - |  4153 | ` */` |
|   594611 |  4154 | `case PH7_OP_JZ:` |
|        - |  4155 | `#ifdef UNTRUST` |
|        - |  4156 | `	if( pTos < pStack ){` |
|        - |  4157 | `		goto Abort;` |
|        - |  4158 | `	}` |
|        - |  4159 | `#endif` |
|        - |  4160 | `	/* Get a boolean value */` |
|  1189312 |  4161 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4162 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4163 | `	}` |
|  1189312 |  4164 | `	if( !pTos->x.iVal ){` |
|        - |  4165 | `		/* Take the jump */` |
|   611628 |  4166 | `		pc = pInstr->iP2 - 1;` |
|   305813 |  4167 | `	}` |
|  1189312 |  4168 | `	if( !pInstr->iP1 ){` |
|   942814 |  4169 | `		VmPopOperand(&pTos,1);` |
|   471428 |  4170 | `	}` |
|  1189312 |  4171 | `	break;` |
|        - |  4172 | `/*` |
|        - |  4173 | ` * JNZ: P1 P2 *` |
|        - |  4174 | ` *` |
|        - |  4175 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4176 | ` * entry in the stack if P1 is zero.` |
|        - |  4177 | ` */` |
|    61323 |  4178 | `case PH7_OP_JNZ:` |
|        - |  4179 | `#ifdef UNTRUST` |
|        - |  4180 | `	if( pTos < pStack ){` |
|        - |  4181 | `		goto Abort;` |
|        - |  4182 | `	}` |
|        - |  4183 | `#endif` |
|        - |  4184 | `	/* Get a boolean value */` |
|   122648 |  4185 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4186 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4187 | `	}` |
|   122648 |  4188 | `	if( pTos->x.iVal ){` |
|        - |  4189 | `		/* Take the jump */` |
|     5558 |  4190 | `		pc = pInstr->iP2 - 1;` |
|     2778 |  4191 | `	}` |
|   122648 |  4192 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4193 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4194 | `	}` |
|   122648 |  4195 | `	break;` |
|        - |  4196 | `/*` |
|        - |  4197 | ` * NOOP: * * *` |
|        - |  4198 | ` *` |
|        - |  4199 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4200 | ` * destination.` |
|        - |  4201 | ` */` |
|      ! 0 |  4202 | `case PH7_OP_NOOP:` |
|      ! 0 |  4203 | `	break;` |
|        - |  4204 | `/*` |
|        - |  4205 | ` * POP: P1 * *` |
|        - |  4206 | ` *` |
|        - |  4207 | ` * Pop P1 elements from the operand stack.` |
|        - |  4208 | ` */` |
|   461031 |  4209 | `case PH7_OP_POP: {` |
|   922108 |  4210 | `	sxi32 n = pInstr->iP1;` |
|   922108 |  4211 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4212 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4213 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4214 | `	}` |
|   922108 |  4215 | `	VmPopOperand(&pTos,n);` |
|   922108 |  4216 | `	break;` |
|        - |  4217 | `				 }` |
|        - |  4218 | `/*` |
|        - |  4219 | ` * DUP: * * *` |
|        - |  4220 | ` *` |
|        - |  4221 | ` * Duplicate the top of the stack.` |
|        - |  4222 | ` */` |
|       41 |  4223 | `case PH7_OP_DUP:` |
|        - |  4224 | `#ifdef UNTRUST` |
|        - |  4225 | `	if( pTos < pStack ){` |
|        - |  4226 | `		goto Abort;` |
|        - |  4227 | `	}` |
|        - |  4228 | `#endif` |
|       84 |  4229 | `	pTos++;` |
|       84 |  4230 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4231 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4232 | `	break;` |
|        - |  4233 | `/*` |
|        - |  4234 | ` * NSSWITCH: * * P3` |
|        - |  4235 | ` *` |
|        - |  4236 | ` * Switch the active namespace at runtime.` |
|        - |  4237 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4238 | ` */` |
|     7777 |  4239 | `case PH7_OP_NSSWITCH:` |
|    15556 |  4240 | `	SyBlobReset(&pVm->sNamespace);` |
|    15556 |  4241 | `	if( pInstr->p3 ){` |
|       98 |  4242 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4243 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4244 | `	}` |
|        - |  4245 | `	/* Clear namespace-scoped use-const imports */` |
|    15556 |  4246 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15556 |  4247 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15556 |  4248 | `	break;` |
|        - |  4249 | `/* OP_USECONST P1 * P3` |
|        - |  4250 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4251 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4252 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4253 | ` */` |
|        7 |  4254 | `case PH7_OP_USECONST: {` |
|       16 |  4255 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4256 | `	if( azPair ){` |
|       16 |  4257 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4258 | `	}` |
|       16 |  4259 | `	break;` |
|        - |  4260 | `				}` |
|        - |  4261 | `/*` |
|        - |  4262 | ` * CVT_INT: * * *` |
|        - |  4263 | ` *` |
|        - |  4264 | ` * Force the top of the stack to be an integer.` |
|        - |  4265 | ` */` |
|       80 |  4266 | `case PH7_OP_CVT_INT:` |
|        - |  4267 | `#ifdef UNTRUST` |
|        - |  4268 | `	if( pTos < pStack ){` |
|        - |  4269 | `		goto Abort;` |
|        - |  4270 | `	}` |
|        - |  4271 | `#endif` |
|      162 |  4272 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4273 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4274 | `	}` |
|        - |  4275 | `	/* Invalidate any prior representation */` |
|      162 |  4276 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4277 | `	break;` |
|        - |  4278 | `/*` |
|        - |  4279 | ` * CVT_REAL: * * *` |
|        - |  4280 | ` *` |
|        - |  4281 | ` * Force the top of the stack to be a real.` |
|        - |  4282 | ` */` |
|        5 |  4283 | `case PH7_OP_CVT_REAL:` |
|        - |  4284 | `#ifdef UNTRUST` |
|        - |  4285 | `	if( pTos < pStack ){` |
|        - |  4286 | `		goto Abort;` |
|        - |  4287 | `	}` |
|        - |  4288 | `#endif` |
|       11 |  4289 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4290 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4291 | `	}` |
|        - |  4292 | `	/* Invalidate any prior representation */` |
|       11 |  4293 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4294 | `	break;` |
|        - |  4295 | `/*` |
|        - |  4296 | ` * CVT_STR: * * *` |
|        - |  4297 | ` *` |
|        - |  4298 | ` * Force the top of the stack to be a string.` |
|        - |  4299 | ` */` |
|      163 |  4300 | `case PH7_OP_CVT_STR:` |
|        - |  4301 | `#ifdef UNTRUST` |
|        - |  4302 | `	if( pTos < pStack ){` |
|        - |  4303 | `		goto Abort;` |
|        - |  4304 | `	}` |
|        - |  4305 | `#endif` |
|      328 |  4306 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4307 | `		PH7_MemObjToString(pTos);` |
|      153 |  4308 | `	}` |
|      328 |  4309 | `	break;` |
|        - |  4310 | `/*` |
|        - |  4311 | ` * CVT_BOOL: * * *` |
|        - |  4312 | ` *` |
|        - |  4313 | ` * Force the top of the stack to be a boolean.` |
|        - |  4314 | ` */` |
|        5 |  4315 | `case PH7_OP_CVT_BOOL:` |
|        - |  4316 | `#ifdef UNTRUST` |
|        - |  4317 | `	if( pTos < pStack ){` |
|        - |  4318 | `		goto Abort;` |
|        - |  4319 | `	}` |
|        - |  4320 | `#endif` |
|       11 |  4321 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4322 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4323 | `	}` |
|       11 |  4324 | `	break;` |
|        - |  4325 | `/*` |
|        - |  4326 | ` * CVT_NULL: * * *` |
|        - |  4327 | ` *` |
|        - |  4328 | ` * Nullify the top of the stack.` |
|        - |  4329 | ` */` |
|        3 |  4330 | `case PH7_OP_CVT_NULL:` |
|        - |  4331 | `#ifdef UNTRUST` |
|        - |  4332 | `	if( pTos < pStack ){` |
|        - |  4333 | `		goto Abort;` |
|        - |  4334 | `	}` |
|        - |  4335 | `#endif` |
|        7 |  4336 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4337 | `	break;` |
|        - |  4338 | `/*` |
|        - |  4339 | ` * CVT_NUMC: * * *` |
|        - |  4340 | ` *` |
|        - |  4341 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4342 | ` */` |
|      ! 0 |  4343 | `case PH7_OP_CVT_NUMC:` |
|        - |  4344 | `#ifdef UNTRUST` |
|        - |  4345 | `	if( pTos < pStack ){` |
|        - |  4346 | `		goto Abort;` |
|        - |  4347 | `	}` |
|        - |  4348 | `#endif` |
|        - |  4349 | `	/* Force a numeric cast */` |
|      ! 0 |  4350 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4351 | `	break;` |
|        - |  4352 | `/*` |
|        - |  4353 | ` * CVT_ARRAY: * * *` |
|        - |  4354 | ` *` |
|        - |  4355 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4356 | ` */` |
|       10 |  4357 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4358 | `#ifdef UNTRUST` |
|        - |  4359 | `	if( pTos < pStack ){` |
|        - |  4360 | `		goto Abort;` |
|        - |  4361 | `	}` |
|        - |  4362 | `#endif` |
|        - |  4363 | `	/* Force a hashmap cast */` |
|       21 |  4364 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4365 | `	if( rc != SXRET_OK ){` |
|        - |  4366 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4367 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4368 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4369 | `	}` |
|       21 |  4370 | `	break;` |
|        - |  4371 | `/*` |
|        - |  4372 | ` * CVT_OBJ: * * *` |
|        - |  4373 | ` *` |
|        - |  4374 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4375 | ` */` |
|        8 |  4376 | `case PH7_OP_CVT_OBJ:` |
|        - |  4377 | `#ifdef UNTRUST` |
|        - |  4378 | `	if( pTos < pStack ){` |
|        - |  4379 | `		goto Abort;` |
|        - |  4380 | `	}` |
|        - |  4381 | `#endif` |
|       17 |  4382 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4383 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4384 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4385 | `	}` |
|       17 |  4386 | `	break;` |
|        - |  4387 | `/*` |
|        - |  4388 | ` * ERR_CTRL * * *` |
|        - |  4389 | ` *` |
|        - |  4390 | ` * Error control operator.` |
|        - |  4391 | ` */` |
|    15948 |  4392 | `case PH7_OP_ERR_CTRL:` |
|        - |  4393 | `	/*` |
|        - |  4394 | `	 * TICKET 1433-038:` |
|        - |  4395 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4396 | `	 * use the public API,to control error output.` |
|        - |  4397 | `	 */` |
|    31896 |  4398 | `	break;` |
|        - |  4399 | `/*` |
|        - |  4400 | ` * IS_A * * *` |
|        - |  4401 | ` *` |
|        - |  4402 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4403 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4404 | ` * holding a class name or an object).` |
|        - |  4405 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4406 | ` */` |
|       64 |  4407 | `case PH7_OP_IS_A:{` |
|      130 |  4408 | `	ph7_value *pNos = &pTos[-1];` |
|      130 |  4409 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4410 | `#ifdef UNTRUST` |
|        - |  4411 | `	if( pNos < pStack ){` |
|        - |  4412 | `		goto Abort;` |
|        - |  4413 | `	}` |
|        - |  4414 | `#endif` |
|      130 |  4415 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      128 |  4416 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      128 |  4417 | `		ph7_class *pClass = 0;` |
|        - |  4418 | `		/* Extract the target class */` |
|      128 |  4419 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4420 | `			/* Instance already loaded */` |
|      ! 0 |  4421 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      128 |  4422 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      128 |  4423 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      128 |  4424 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4425 | `			/* Handle self/static/parent keywords */` |
|      128 |  4426 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4427 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      126 |  4428 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4429 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      125 |  4430 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4431 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4432 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4433 | `					pClass = pSelf->pBase;` |
|        2 |  4434 | `				}` |
|        3 |  4435 | `			}else{` |
|      118 |  4436 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4437 | `			}` |
|       63 |  4438 | `		}` |
|      128 |  4439 | `		if( pClass ){` |
|        - |  4440 | `			/* Perform the query */` |
|      128 |  4441 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       63 |  4442 | `		}` |
|       63 |  4443 | `	}` |
|        - |  4444 | `	/* Push result */` |
|      130 |  4445 | `	VmPopOperand(&pTos,1);` |
|      130 |  4446 | `	PH7_MemObjRelease(pTos);` |
|      130 |  4447 | `	pTos->x.iVal = iRes;` |
|      130 |  4448 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      130 |  4449 | `	break;` |
|        - |  4450 | `				 }` |
|        - |  4451 |  |
|        - |  4452 | `/*` |
|        - |  4453 | ` * LOADC P1 P2 *` |
|        - |  4454 | ` *` |
|        - |  4455 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4456 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4457 | ` */` |
|  1009295 |  4458 | `case PH7_OP_LOADC: {` |
|        - |  4459 | `	ph7_value *pObj;` |
|        - |  4460 | `	/* Reserve a room */` |
|  2018636 |  4461 | `	pTos++;` |
|  3018164 |  4462 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2018636 |  4463 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4464 | `			SyHashEntry *pEntry;` |
|        - |  4465 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4466 | `			{` |
|        - |  4467 | `				SyHashEntry *pConstImport;` |
|    29441 |  4468 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19626 |  4469 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19628 |  4470 | `				if( pConstImport ){` |
|       11 |  4471 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4472 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4473 | `					if( pEntry ){` |
|       11 |  4474 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4475 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4476 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4477 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4478 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4479 | `						break;` |
|        - |  4480 | `					}` |
|        - |  4481 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4482 | `				}` |
|        - |  4483 | `			}` |
|        - |  4484 | `			/* Candidate for expansion via user defined callbacks */` |
|    19618 |  4485 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19618 |  4486 | `			if( pEntry ){` |
|    19612 |  4487 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4488 | `				/* Set a NULL default value */` |
|    19612 |  4489 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19612 |  4490 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4491 | `				/* Invoke the callback and deal with the expanded value */` |
|    19612 |  4492 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4493 | `				/* Mark as constant */` |
|    19612 |  4494 | `				pTos->nIdx = SXU32_HIGH;` |
|    19612 |  4495 | `				break;` |
|        - |  4496 | `			}` |
|        - |  4497 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4498 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4499 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4500 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4501 | `			{` |
|        8 |  4502 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4503 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4504 | `				sxu32 j;` |
|        8 |  4505 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4506 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4507 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4508 | `				}` |
|        8 |  4509 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4510 | `					/* Try current_namespace\name */` |
|      ! 0 |  4511 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4512 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4513 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4514 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4515 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4516 | `					if( pEntry ){` |
|      ! 0 |  4517 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4518 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4519 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4520 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4521 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4522 | `						break;` |
|        - |  4523 | `					}` |
|        - |  4524 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4525 | `				}` |
|        8 |  4526 | `				if( isQualified ){` |
|        - |  4527 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4528 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4529 | `					SyBlob sErr;` |
|        3 |  4530 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4531 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4532 | `					if( pErrFile ){` |
|        3 |  4533 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4534 | `					}` |
|        3 |  4535 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4536 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4537 | `					SyBlobRelease(&sErr);` |
|        3 |  4538 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4539 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4540 | `					goto LoadC_Done;` |
|        - |  4541 | `				}` |
|        - |  4542 | `			}` |
|        2 |  4543 | `		}` |
|  1999014 |  4544 | `		PH7_MemObjLoad(pObj,pTos);` |
|   999530 |  4545 | `	}else{` |
|        - |  4546 | `		/* Set a NULL value */` |
|      ! 0 |  4547 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4548 | `	}` |
|   999485 |  4549 | `LoadC_Done:` |
|        - |  4550 | `	/* Mark as constant */` |
|  1999016 |  4551 | `	pTos->nIdx = SXU32_HIGH;` |
|  1999016 |  4552 | `	break;` |
|        - |  4553 | `				  }` |
|        - |  4554 | `/*` |
|        - |  4555 | ` * LOAD: P1 * P3` |
|        - |  4556 | ` *` |
|        - |  4557 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4558 | ` * from the P3 operand.` |
|        - |  4559 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4560 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4561 | ` */` |
|  1571735 |  4562 | `case PH7_OP_LOAD:{` |
|        - |  4563 | `	ph7_value *pObj;` |
|        - |  4564 | `	SyString sName;` |
|  3143692 |  4565 | `	if( pInstr->p3 == 0 ){` |
|        - |  4566 | `		/* Take the variable name from the top of the stack */` |
|        - |  4567 | `#ifdef UNTRUST` |
|        - |  4568 | `		if( pTos < pStack ){` |
|        - |  4569 | `			goto Abort;` |
|        - |  4570 | `		}` |
|        - |  4571 | `#endif` |
|        - |  4572 | `		/* Force a string cast */` |
|       19 |  4573 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4574 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4575 | `		}` |
|       19 |  4576 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4577 | `	}else{` |
|  3143674 |  4578 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4579 | `		/* Reserve a room for the target object */` |
|  3143674 |  4580 | `		pTos++;` |
|        - |  4581 | `	}` |
|        - |  4582 | `	/* Extract the requested memory object */` |
|  3143692 |  4583 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3143692 |  4584 | `	if( pObj == 0 ){` |
|      836 |  4585 | `		if( pInstr->iP1 ){` |
|        - |  4586 | `			/* Variable not found,load NULL */` |
|      836 |  4587 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4588 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4589 | `			}else{` |
|      836 |  4590 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4591 | `			}` |
|      836 |  4592 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1572154 |  4593 | `			break;` |
|      ! 0 |  4594 | `		}else{` |
|        - |  4595 | `			/* Fatal error */` |
|      ! 0 |  4596 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4597 | `			goto Abort;` |
|        - |  4598 | `		}` |
|        - |  4599 | `	}` |
|        - |  4600 | `	/* Load variable contents */` |
|  3142858 |  4601 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3142858 |  4602 | `	pTos->nIdx = pObj->nIdx;` |
|  3142858 |  4603 | `	break;` |
|        - |  4604 | `				   }` |
|        - |  4605 | `/*` |
|        - |  4606 | ` * LOAD_MAP P1 * *` |
|        - |  4607 | ` *` |
|        - |  4608 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4609 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4610 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4611 | ` */` |
|    22642 |  4612 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4613 | `	ph7_hashmap *pMap;` |
|        - |  4614 | `	/* Allocate a new hashmap instance */` |
|    45286 |  4615 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45286 |  4616 | `	if( pMap == 0 ){` |
|      ! 0 |  4617 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4618 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4619 | `		goto Abort;` |
|        - |  4620 | `	}` |
|    45286 |  4621 | `	if( pInstr->iP1 > 0 ){` |
|     2734 |  4622 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2734 |  4623 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4624 | `		/* Perform the insertion */` |
|     8310 |  4625 | `		while( pEntry < pTos ){` |
|     5594 |  4626 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4627 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4628 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4629 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  4630 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  4631 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  4632 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4633 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4634 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4635 | `						 * map dangling. */` |
|      ! 0 |  4636 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4637 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4638 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4639 | `						break;` |
|        - |  4640 | `					}` |
|       27 |  4641 | `				}else{` |
|        - |  4642 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  4643 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  4644 | `					break;` |
|        1 |  4645 | `				}` |
|     5552 |  4646 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4647 | `				/* Insertion by reference */` |
|      151 |  4648 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  4649 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  4650 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4651 | `					);` |
|       51 |  4652 | `			}else{` |
|        - |  4653 | `				/* Standard insertion */` |
|     8138 |  4654 | `				PH7_HashmapInsert(pMap,` |
|     5424 |  4655 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2712 |  4656 | `					&pEntry[1]` |
|        - |  4657 | `				);` |
|        - |  4658 | `			}` |
|        - |  4659 | `			/* Next pair on the stack */` |
|     5578 |  4660 | `			pEntry += 2;` |
|        2 |  4661 | `		}` |
|        - |  4662 | `		/* Pop P1 elements */` |
|     2734 |  4663 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2734 |  4664 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4665 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  4666 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  4667 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4668 | `				goto Abort;` |
|        - |  4669 | `			}` |
|        - |  4670 | `			{` |
|       17 |  4671 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  4672 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  4673 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  4674 | `					break;` |
|        - |  4675 | `				}` |
|        - |  4676 | `			}` |
|       15 |  4677 | `			goto Exception;` |
|        - |  4678 | `		}` |
|     1358 |  4679 | `	}` |
|        - |  4680 | `	/* Push the hashmap */` |
|    45270 |  4681 | `	pTos++;` |
|    45270 |  4682 | `	pTos->nIdx = SXU32_HIGH;` |
|    45270 |  4683 | `	pTos->x.pOther = pMap;` |
|    45270 |  4684 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45270 |  4685 | `	break;` |
|        - |  4686 | `					  }` |
|        - |  4687 | `/*` |
|        - |  4688 | ` * LOAD_LIST: P1 * *` |
|        - |  4689 | ` *` |
|        - |  4690 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4691 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4692 | ` * Caveats:` |
|        - |  4693 | ` *  This implementation support only a single nesting level.` |
|        - |  4694 | ` */` |
|       48 |  4695 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4696 | `	ph7_value *pEntry;` |
|       98 |  4697 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4698 | `		/* Empty list,break immediately */` |
|      ! 0 |  4699 | `		break;` |
|        - |  4700 | `	}` |
|       98 |  4701 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4702 | `#ifdef UNTRUST` |
|        - |  4703 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4704 | `		goto Abort;` |
|        - |  4705 | `	}` |
|        - |  4706 | `#endif` |
|       98 |  4707 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4708 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4709 | `		ph7_hashmap_node *pNode;` |
|        - |  4710 | `		ph7_value sKey,*pObj;` |
|        - |  4711 | `		/* Start Copying */` |
|       91 |  4712 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4713 | `		while( pEntry <= pTos ){` |
|      193 |  4714 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4715 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4716 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4717 | `					if( rc == SXRET_OK ){` |
|        - |  4718 | `						/* Store node value */` |
|      165 |  4719 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4720 | `					}else{` |
|        - |  4721 | `						/* Undefined array key */` |
|        - |  4722 | `						char zMsg[128];` |
|      ! 0 |  4723 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4724 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4725 | `						PH7_MemObjRelease(pObj);` |
|        - |  4726 | `					}` |
|       82 |  4727 | `				}` |
|       82 |  4728 | `			}` |
|      193 |  4729 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4730 | `			pEntry++;` |
|        1 |  4731 | `		}` |
|       46 |  4732 | `	}else{` |
|        - |  4733 | `		/* Source is not an array */` |
|        - |  4734 | `		ph7_value *pObj;` |
|       18 |  4735 | `		while( pEntry <= pTos ){` |
|       12 |  4736 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4737 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4738 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4739 | `				}` |
|        5 |  4740 | `			}` |
|       12 |  4741 | `			pEntry++;` |
|        2 |  4742 | `		}` |
|        8 |  4743 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4744 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4745 | `			const char *zType = "unknown";` |
|        3 |  4746 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4747 | `			char zMsg[256];` |
|        3 |  4748 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4749 | `				zType = "string";` |
|        1 |  4750 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4751 | `				zType = "int";` |
|      ! 0 |  4752 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4753 | `				zType = "float";` |
|      ! 0 |  4754 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4755 | `				zType = "object";` |
|      ! 0 |  4756 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4757 | `				zType = "resource";` |
|      ! 0 |  4758 | `			}` |
|        3 |  4759 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4760 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4761 | `		}` |
|        - |  4762 | `	}` |
|       98 |  4763 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4764 | `	break;` |
|        - |  4765 | `					   }` |
|        - |  4766 | `/*` |
|        - |  4767 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4768 | ` *` |
|        - |  4769 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4770 | ` * from the stack.` |
|        - |  4771 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4772 | ` * instead.` |
|        - |  4773 | ` */` |
|   250334 |  4774 | `case PH7_OP_LOAD_IDX: {` |
|   500714 |  4775 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   500714 |  4776 | `	ph7_hashmap *pMap = 0;` |
|        - |  4777 | `	ph7_value *pIdx;` |
|   500714 |  4778 | `	pIdx = 0;` |
|   500714 |  4779 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4780 | `		if( !pInstr->iP2){` |
|        - |  4781 | `			/* No available index,load NULL */` |
|      ! 0 |  4782 | `			if( pTos >= pStack ){` |
|      ! 0 |  4783 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4784 | `			}else{` |
|        - |  4785 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4786 | `				pTos++;` |
|      ! 0 |  4787 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4788 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4789 | `			}` |
|        - |  4790 | `			/* Emit a notice */` |
|      ! 0 |  4791 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4792 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4793 | `			break;` |
|        - |  4794 | `		}` |
|      ! 0 |  4795 | `	}else{` |
|   500714 |  4796 | `		pIdx = pTos;` |
|   500714 |  4797 | `		pTos--;` |
|        - |  4798 | `	}` |
|   500714 |  4799 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4800 | `		/* String access */` |
|   387716 |  4801 | `		if( pIdx ){` |
|        - |  4802 | `			sxu32 nOfft;` |
|   387716 |  4803 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4804 | `				/* Force an int cast */` |
|      ! 0 |  4805 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4806 | `			}` |
|   387716 |  4807 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  4808 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4809 | `				/* Invalid offset,load null */` |
|      ! 0 |  4810 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4811 | `			}else{` |
|   387716 |  4812 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  4813 | `				int c = zData[nOfft];` |
|   387716 |  4814 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  4815 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  4816 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4817 | `			}` |
|   193881 |  4818 | `		}else{` |
|        - |  4819 | `			/* No available index,load NULL */` |
|      ! 0 |  4820 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4821 | `		}` |
|   387716 |  4822 | `		break;` |
|        - |  4823 | `	}` |
|   113000 |  4824 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4825 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  4826 | `		 * iP2 codes:` |
|        - |  4827 | `		 *   0 = read       → offsetGet` |
|        - |  4828 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  4829 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  4830 | `		 *   4 = isset()    → offsetExists` |
|        - |  4831 | `		 *   5 = unset()    → offsetUnset` |
|        - |  4832 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  4833 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  4834 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  4835 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  4836 | `			ph7_class_method *pMeth;` |
|        - |  4837 | `			ph7_value sResult;` |
|        - |  4838 | `			ph7_value *apArg[1];` |
|      124 |  4839 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  4840 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  4841 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4842 | `					"Cannot use [] for reading");` |
|      ! 0 |  4843 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4844 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4845 | `				break;` |
|        - |  4846 | `			}` |
|      124 |  4847 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  4848 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  4849 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  4850 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4851 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  4852 | `				apArg[0] = pIdx;` |
|       51 |  4853 | `				if( pMeth ){` |
|       51 |  4854 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  4855 | `				}` |
|       99 |  4856 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  4857 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4858 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  4859 | `				apArg[0] = pIdx;` |
|        9 |  4860 | `				if( pMeth ){` |
|        9 |  4861 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  4862 | `				}` |
|        5 |  4863 | `			}else{` |
|       66 |  4864 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4865 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  4866 | `				apArg[0] = pIdx;` |
|       66 |  4867 | `				if( pMeth ){` |
|       66 |  4868 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  4869 | `				}` |
|        - |  4870 | `			}` |
|      124 |  4871 | `			if( pInstr->iP2 == 4 ){` |
|        - |  4872 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  4873 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  4874 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  4875 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  4876 | `				PH7_MemObjRelease(pTos);` |
|       33 |  4877 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  4878 | `				if( bExists ){` |
|       17 |  4879 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  4880 | `					pTos->x.iVal = 1;` |
|        9 |  4881 | `				}else{` |
|       17 |  4882 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  4883 | `				}` |
|      108 |  4884 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  4885 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  4886 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  4887 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4888 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4889 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  4890 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  4891 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  4892 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  4893 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  4894 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  4895 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  4896 | `				PH7_MemObjRelease(pTos);` |
|       11 |  4897 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  4898 | `				if( !bExists ){` |
|        3 |  4899 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  4900 | `				}else{` |
|        9 |  4901 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4902 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  4903 | `					ph7_value sValue;` |
|        9 |  4904 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4905 | `					apArg[0] = pIdx;` |
|        9 |  4906 | `					if( pGet ){` |
|        9 |  4907 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  4908 | `					}` |
|        9 |  4909 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  4910 | `					PH7_MemObjRelease(&sValue);` |
|        - |  4911 | `				}` |
|       11 |  4912 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  4913 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  4914 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  4915 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  4916 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  4917 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  4918 | `				 *     and push NULL.` |
|        - |  4919 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  4920 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  4921 | `				int bShouldArm = !bExists;` |
|        - |  4922 | `				ph7_value sValue;` |
|        9 |  4923 | `				PH7_MemObjRelease(&sResult);` |
|        - |  4924 | `				/* Reset any prior arming defensively */` |
|        9 |  4925 | `				VmCoalesceDisarm(pVm);` |
|        9 |  4926 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4927 | `				if( bExists ){` |
|        5 |  4928 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4929 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  4930 | `					apArg[0] = pIdx;` |
|        5 |  4931 | `					if( pGet ){` |
|        5 |  4932 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  4933 | `					}` |
|        5 |  4934 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  4935 | `						bShouldArm = 1;` |
|        1 |  4936 | `					}` |
|        2 |  4937 | `				}` |
|        9 |  4938 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4939 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4940 | `				if( bShouldArm ){` |
|        - |  4941 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  4942 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  4943 | `					 * intervening expression evaluation. */` |
|        7 |  4944 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  4945 | `					if( pIdx ){` |
|        7 |  4946 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  4947 | `					}` |
|        7 |  4948 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  4949 | `					pInst->iRef++;` |
|        7 |  4950 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  4951 | `				}else{` |
|        3 |  4952 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  4953 | `				}` |
|        9 |  4954 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  4955 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  4956 | `				break;` |
|      ! 0 |  4957 | `			}else{` |
|        - |  4958 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  4959 | `				PH7_MemObjRelease(pTos);` |
|       66 |  4960 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  4961 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4962 | `			}` |
|      106 |  4963 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  4964 | `			if( pIdx ){` |
|      106 |  4965 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  4966 | `			}` |
|      106 |  4967 | `			break;` |
|        - |  4968 | `		}` |
|        - |  4969 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  4970 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  4971 | `		if( pInst ){` |
|        - |  4972 | `			char zMsg[256];` |
|        3 |  4973 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  4974 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  4975 | `				"Cannot use object of type %.*s as array",` |
|        2 |  4976 | `				(int)pName->nByte,pName->zString);` |
|        3 |  4977 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  4978 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  4979 | `			PH7_MemObjRelease(pTos);` |
|        3 |  4980 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4981 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  4982 | `			break;` |
|        - |  4983 | `		}` |
|      ! 0 |  4984 | `	}` |
|   112876 |  4985 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4986 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4987 | `			ph7_value *pObj;` |
|        3 |  4988 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4989 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4990 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4991 | `			}` |
|        1 |  4992 | `		}` |
|        1 |  4993 | `	}` |
|   112876 |  4994 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   112876 |  4995 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   112876 |  4996 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  4997 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4998 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4999 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5000 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5001 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5002 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  5003 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  5004 | `		}` |
|        - |  5005 | `		/* Point to the hashmap */` |
|   112876 |  5006 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   112876 |  5007 | `		if( pIdx ){` |
|        - |  5008 | `			/* Load the desired entry */` |
|   112876 |  5009 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56437 |  5010 | `		}` |
|   112876 |  5011 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5012 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5013 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5014 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5015 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5016 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5017 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5018 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5019 | `			 * correct for the outermost write. */` |
|       19 |  5020 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5021 | `			if( !needWrite && pNode ){` |
|       13 |  5022 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5023 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5024 | `					needWrite = 1;` |
|        3 |  5025 | `				}` |
|        6 |  5026 | `			}` |
|       19 |  5027 | `			if( needWrite ){` |
|       13 |  5028 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5029 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5030 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5031 | `					 * into the new map's storage. */` |
|        7 |  5032 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5033 | `					if( pIdx ){` |
|        7 |  5034 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5035 | `					}` |
|        3 |  5036 | `				}` |
|        6 |  5037 | `			}` |
|        9 |  5038 | `		}` |
|   112876 |  5039 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5040 | `			/* Create a new empty entry */` |
|      273 |  5041 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5042 | `			if( rc == SXRET_OK ){` |
|        - |  5043 | `				/* Point to the last inserted entry */` |
|      273 |  5044 | `				pNode = pMap->pLast;` |
|      136 |  5045 | `			}` |
|      136 |  5046 | `		}` |
|    56437 |  5047 | `	}` |
|   112876 |  5048 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5049 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5050 | `		char zMsg[128];` |
|      ! 0 |  5051 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5052 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5053 | `		}` |
|      ! 0 |  5054 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5055 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5056 | `	}` |
|   112876 |  5057 | `	if( pIdx ){` |
|   112876 |  5058 | `		PH7_MemObjRelease(pIdx);` |
|    56437 |  5059 | `	}` |
|   112876 |  5060 | `	if( rc == SXRET_OK ){` |
|        - |  5061 | `		/* Load entry contents */` |
|    50058 |  5062 | `		if( pMap->iRef < 2 ){` |
|        - |  5063 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5064 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5065 | `			 */` |
|       28 |  5066 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5067 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5068 | `		}else{` |
|    50032 |  5069 | `			pTos->nIdx = pNode->nValIdx;` |
|    50032 |  5070 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50032 |  5071 | `			PH7_HashmapUnref(pMap);` |
|        - |  5072 | `		}` |
|    25030 |  5073 | `	}else{` |
|        - |  5074 | `		/* No such entry,load NULL */` |
|    62820 |  5075 | `		PH7_MemObjRelease(pTos);` |
|    62820 |  5076 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5077 | `	}` |
|   112876 |  5078 | `	break;` |
|        - |  5079 | `					  }` |
|        - |  5080 | `/*` |
|        - |  5081 | ` * LOAD_CLOSURE * * P3` |
|        - |  5082 | ` *` |
|        - |  5083 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5084 | ` * name in the stack.` |
|        - |  5085 | ` */` |
|       61 |  5086 | `case PH7_OP_LOAD_CLOSURE:{` |
|      124 |  5087 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      124 |  5088 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5089 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5090 | `		ph7_vm_func *pClosure;` |
|        - |  5091 | `		char *zName;` |
|        - |  5092 | `		sxu32 mLen;` |
|        - |  5093 | `		sxu32 n;` |
|        - |  5094 | `		/* Create a new VM function */` |
|      124 |  5095 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5096 | `		/* Generate an unique closure name */` |
|      124 |  5097 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      124 |  5098 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5099 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5100 | `			goto Abort;` |
|        - |  5101 | `		}` |
|      124 |  5102 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      124 |  5103 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5104 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5105 | `		}` |
|        - |  5106 | `		/* Zero the stucture */` |
|      124 |  5107 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5108 | `		/* Perform a structure assignment on read-only items */` |
|      124 |  5109 | `		pClosure->aArgs = pFunc->aArgs;` |
|      124 |  5110 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      124 |  5111 | `		pClosure->aStatic = pFunc->aStatic;` |
|      124 |  5112 | `		pClosure->iFlags = pFunc->iFlags;` |
|      124 |  5113 | `		pClosure->pUserData = pFunc->pUserData;` |
|      124 |  5114 | `		pClosure->sSignature = pFunc->sSignature;` |
|      124 |  5115 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      124 |  5116 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      124 |  5117 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      124 |  5118 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      124 |  5119 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5120 | `		/* Register the closure */` |
|      124 |  5121 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5122 | `		/* Set up closure environment */` |
|      124 |  5123 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      124 |  5124 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      312 |  5125 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5126 | `			ph7_value *pValue;` |
|      190 |  5127 | `			pEnv = &aEnv[n];` |
|      190 |  5128 | `			sEnv.sName  = pEnv->sName;` |
|      190 |  5129 | `			sEnv.iFlags = pEnv->iFlags;` |
|      190 |  5130 | `			sEnv.nIdx = SXU32_HIGH;` |
|      190 |  5131 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      190 |  5132 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5133 | `				/* Pass by reference */` |
|      ! 0 |  5134 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5135 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5136 | `					);` |
|      ! 0 |  5137 | `			}` |
|        - |  5138 | `			/* Standard pass by value */` |
|      190 |  5139 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      190 |  5140 | `			if( pValue ){` |
|        - |  5141 | `				/* Copy imported value */` |
|       72 |  5142 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5143 | `			}` |
|        - |  5144 | `			/* Insert the imported variable */` |
|      190 |  5145 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       96 |  5146 | `		}` |
|        - |  5147 | `		/* Finally,load the closure name on the stack */` |
|      124 |  5148 | `		pTos++;` |
|      124 |  5149 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       61 |  5150 | `	}` |
|      124 |  5151 | `	break;` |
|        - |  5152 | `						 }` |
|        - |  5153 | `/*` |
|        - |  5154 | ` * STORE * P2 P3` |
|        - |  5155 | ` *` |
|        - |  5156 | ` * Perform a store (Assignment) operation.` |
|        - |  5157 | ` */` |
|   145218 |  5158 | `case PH7_OP_STORE: {` |
|        - |  5159 | `	ph7_value *pObj;` |
|        - |  5160 | `	SyString sName;` |
|        - |  5161 | `#ifdef UNTRUST` |
|        - |  5162 | `	if( pTos < pStack ){` |
|        - |  5163 | `		goto Abort;` |
|        - |  5164 | `	}` |
|        - |  5165 | `#endif` |
|   290438 |  5166 | `	if( pInstr->iP2 ){` |
|        - |  5167 | `		sxu32 nIdx;` |
|        - |  5168 | `		sxi32 rcT;` |
|        - |  5169 | `		/* Member store operation */` |
|     5132 |  5170 | `		nIdx = pTos->nIdx;` |
|     5132 |  5171 | `		VmPopOperand(&pTos,1);` |
|     5132 |  5172 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5173 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5174 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5175 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5176 | `		}else{` |
|        - |  5177 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5178 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5128 |  5179 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5128 |  5180 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5181 | `				goto Abort;` |
|        - |  5182 | `			}` |
|     5128 |  5183 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5184 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5185 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5186 | `				 * propagate out of the VM loop. */` |
|       37 |  5187 | `				VmPopOperand(&pTos,1);` |
|        - |  5188 | `				{` |
|       37 |  5189 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5190 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5191 | `						pc = pFrm2->iExceptionJump - 1;` |
|   145237 |  5192 | `						break;` |
|        - |  5193 | `					}` |
|        - |  5194 | `				}` |
|      ! 0 |  5195 | `				goto Exception;` |
|        - |  5196 | `			}` |
|        - |  5197 | `			/* Point to the desired memory object */` |
|     5092 |  5198 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5092 |  5199 | `			if( pObj ){` |
|        - |  5200 | `				/* Perform the store operation */` |
|     5092 |  5201 | `				PH7_MemObjStore(pTos,pObj);` |
|     2545 |  5202 | `			}` |
|        - |  5203 | `		}` |
|     5096 |  5204 | `		break;` |
|   285308 |  5205 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5206 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5207 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5208 | `			/* Force a string cast */` |
|      ! 0 |  5209 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5210 | `		}` |
|        7 |  5211 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5212 | `		pTos--;` |
|        - |  5213 | `#ifdef UNTRUST` |
|        - |  5214 | `		if( pTos < pStack  ){` |
|        - |  5215 | `			goto Abort;` |
|        - |  5216 | `		}` |
|        - |  5217 | `#endif` |
|        4 |  5218 | `	}else{` |
|   285302 |  5219 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5220 | `	}` |
|        - |  5221 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   285308 |  5222 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   285308 |  5223 | `	if( pObj == 0 ){` |
|      ! 0 |  5224 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5225 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5226 | `		goto Abort;` |
|        - |  5227 | `	}` |
|   285308 |  5228 | `	if( !pInstr->p3 ){` |
|        7 |  5229 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5230 | `	}` |
|        - |  5231 | `	/* Perform the store operation */` |
|   285308 |  5232 | `	PH7_MemObjStore(pTos,pObj);` |
|   285308 |  5233 | `	break;` |
|        - |  5234 | `				   }` |
|        - |  5235 | `/*` |
|        - |  5236 | ` * STORE_IDX:   P1 * P3` |
|        - |  5237 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5238 | ` *` |
|        - |  5239 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5240 | ` */` |
|    96496 |  5241 | `case PH7_OP_STORE_IDX:` |
|        - |  5242 | `case PH7_OP_STORE_IDX_REF: {` |
|   192994 |  5243 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5244 | `	ph7_value *pKey;` |
|        - |  5245 | `	sxu32 nIdx;` |
|   192994 |  5246 | `	if( pInstr->iP1 ){` |
|        - |  5247 | `		/* Key is next on stack */` |
|    63140 |  5248 | `		pKey = pTos;` |
|    63140 |  5249 | `		pTos--;` |
|    31571 |  5250 | `	}else{` |
|   129856 |  5251 | `		pKey = 0;` |
|        - |  5252 | `	}` |
|   192994 |  5253 | `	nIdx = pTos->nIdx;` |
|        - |  5254 | `	{` |
|        - |  5255 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5256 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5257 | `		 * the backing variable slot at nIdx. */` |
|   192994 |  5258 | `		ph7_class_instance *pInst = 0;` |
|   192994 |  5259 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5260 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   192978 |  5261 | `		}else if( nIdx != SXU32_HIGH ){` |
|   192962 |  5262 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   192962 |  5263 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5264 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5265 | `			}` |
|    96480 |  5266 | `		}` |
|   192994 |  5267 | `		if( pInst ){` |
|       34 |  5268 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5269 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5270 | `				ph7_class_method *pMeth;` |
|        - |  5271 | `				ph7_value sNullKey;` |
|        - |  5272 | `				ph7_value *apArg[2];` |
|       32 |  5273 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5274 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5275 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5276 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5277 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5278 | `					break;` |
|        - |  5279 | `				}` |
|       32 |  5280 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5281 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5282 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5283 | `				VmPopOperand(&pTos,1);` |
|       32 |  5284 | `				if( pKey == 0 ){` |
|        7 |  5285 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5286 | `					apArg[0] = &sNullKey;` |
|        4 |  5287 | `				}else{` |
|       26 |  5288 | `					apArg[0] = pKey;` |
|        - |  5289 | `				}` |
|       32 |  5290 | `				apArg[1] = pTos;` |
|       32 |  5291 | `				if( pMeth ){` |
|       32 |  5292 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5293 | `				}` |
|       32 |  5294 | `				if( pKey ){` |
|       26 |  5295 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5296 | `				}else{` |
|        7 |  5297 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5298 | `				}` |
|        - |  5299 | `				/* Pop the value */` |
|       32 |  5300 | `				VmPopOperand(&pTos,1);` |
|       32 |  5301 | `				break;` |
|        - |  5302 | `			}` |
|        - |  5303 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5304 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5305 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5306 | `			 * a few lines below). Match PHP. */` |
|        - |  5307 | `			{` |
|        - |  5308 | `				char zMsg[256];` |
|        3 |  5309 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5310 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5311 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5312 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5313 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5314 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5315 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5316 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5317 | `				break;` |
|        - |  5318 | `			}` |
|        - |  5319 | `		}` |
|        - |  5320 | `	}` |
|   192962 |  5321 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5322 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5323 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5324 | `		 * checking true sharing count, then re-add after separation. */` |
|   192910 |  5325 | `		if( nIdx != SXU32_HIGH ){` |
|   192910 |  5326 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   289364 |  5327 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   192910 |  5328 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5329 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5330 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5331 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5332 | `				 * refcounts if the backing array was already separated. */` |
|   192910 |  5333 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   192910 |  5334 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   192910 |  5335 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   192910 |  5336 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   192910 |  5337 | `					pTos->x.pOther = pMap;` |
|    96456 |  5338 | `				}else{` |
|        - |  5339 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5340 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5341 | `					pMap = pCur;` |
|        - |  5342 | `				}` |
|    96456 |  5343 | `			}else{` |
|      ! 0 |  5344 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5345 | `			}` |
|    96456 |  5346 | `		}else{` |
|      ! 0 |  5347 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5348 | `		}` |
|   192910 |  5349 | `		if( pMap->iRef < 2 ){` |
|        - |  5350 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5351 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5352 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5353 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5354 | `			pMap->iRef = 2;` |
|      ! 0 |  5355 | `		}` |
|    96456 |  5356 | `	}else{` |
|        - |  5357 | `		ph7_value *pObj;` |
|       53 |  5358 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5359 | `		if( pObj == 0 ){` |
|      ! 0 |  5360 | `			if( pKey ){` |
|      ! 0 |  5361 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5362 | `			}` |
|      ! 0 |  5363 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5364 | `			break;` |
|        - |  5365 | `		}` |
|        - |  5366 | `		/* Phase#1: Load the array */` |
|       53 |  5367 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5368 | `			VmPopOperand(&pTos,1);` |
|       53 |  5369 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5370 | `				/* Force a string cast */` |
|      ! 0 |  5371 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5372 | `			}` |
|       53 |  5373 | `			if( pKey == 0 ){` |
|        - |  5374 | `				/* Append string */` |
|        3 |  5375 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5376 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5377 | `				}` |
|        2 |  5378 | `			}else{` |
|        - |  5379 | `				sxu32 nOfft;` |
|       51 |  5380 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5381 | `					/* Force an int cast */` |
|       51 |  5382 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5383 | `				}` |
|       51 |  5384 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5385 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5386 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5387 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5388 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5389 | `				}else{` |
|      ! 0 |  5390 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5391 | `						/* Perform an append operation */` |
|      ! 0 |  5392 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5393 | `					}` |
|        - |  5394 | `				}` |
|        - |  5395 | `			}` |
|       53 |  5396 | `			if( pKey ){` |
|       51 |  5397 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5398 | `			}` |
|       53 |  5399 | `			break;` |
|      ! 0 |  5400 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5401 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5402 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5403 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5404 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5405 | `				goto Abort;` |
|        - |  5406 | `			}` |
|      ! 0 |  5407 | `		}` |
|        - |  5408 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5409 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5410 | `	}` |
|   192910 |  5411 | `	VmPopOperand(&pTos,1);` |
|        - |  5412 | `	/* Phase#2: Perform the insertion */` |
|   192910 |  5413 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5414 | `		/* Insertion by reference */` |
|       15 |  5415 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5416 | `	}else{` |
|   192896 |  5417 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5418 | `	}` |
|   192910 |  5419 | `	if( pKey ){` |
|    63064 |  5420 | `		PH7_MemObjRelease(pKey);` |
|    31531 |  5421 | `	}` |
|   192910 |  5422 | `	break;` |
|        - |  5423 | `					   }` |
|        - |  5424 | `/*` |
|        - |  5425 | ` * INCR: P1 * *` |
|        - |  5426 | ` *` |
|        - |  5427 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5428 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5429 | ` * the stack and increment after that.` |
|        - |  5430 | ` */` |
|   167653 |  5431 | `case PH7_OP_INCR:` |
|        - |  5432 | `#ifdef UNTRUST` |
|        - |  5433 | `	if( pTos < pStack ){` |
|        - |  5434 | `		goto Abort;` |
|        - |  5435 | `	}` |
|        - |  5436 | `#endif` |
|   335352 |  5437 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335352 |  5438 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5439 | `			ph7_value *pObj;` |
|   335352 |  5440 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335352 |  5441 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5442 | `					/* Perl-style string increment.` |
|        - |  5443 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5444 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5445 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5446 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5447 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5448 | `					}` |
|       49 |  5449 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5450 | `					if( pInstr->iP1 ){` |
|        - |  5451 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5452 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5453 | `					}` |
|       25 |  5454 | `				}else{` |
|        - |  5455 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5456 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5457 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5458 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5459 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5460 | `					 * so its old-value view survives the coercion. */` |
|   335304 |  5461 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5462 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5463 | `					}` |
|        - |  5464 | `					/* Force a numeric cast on the variable */` |
|   335304 |  5465 | `					PH7_MemObjToNumeric(pObj);` |
|   335304 |  5466 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5467 | `						pObj->rVal++;` |
|        - |  5468 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5469 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5470 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5471 | `						 * integer-valued real. */` |
|        9 |  5472 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5473 | `					}else{` |
|   335296 |  5474 | `						pObj->x.iVal++;` |
|        - |  5475 | `					}` |
|   335304 |  5476 | `					if( pInstr->iP1 ){` |
|        - |  5477 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5478 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5479 | `					}` |
|        - |  5480 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5481 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5482 | `				}` |
|   167697 |  5483 | `			}` |
|   167699 |  5484 | `		}else{` |
|      ! 0 |  5485 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5486 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5487 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5488 | `				}else{` |
|        - |  5489 | `					/* Force a numeric cast */` |
|      ! 0 |  5490 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5491 | `					/* Pre-increment */` |
|      ! 0 |  5492 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5493 | `						pTos->rVal++;` |
|        - |  5494 | `						/* Try to get an integer representation */` |
|      ! 0 |  5495 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5496 | `					}else{` |
|      ! 0 |  5497 | `						pTos->x.iVal++;` |
|      ! 0 |  5498 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5499 | `					}` |
|        - |  5500 | `				}` |
|      ! 0 |  5501 | `			}` |
|        - |  5502 | `		}` |
|   167697 |  5503 | `	}` |
|   335352 |  5504 | `	break;` |
|        - |  5505 | `/*` |
|        - |  5506 | ` * DECR: P1 * *` |
|        - |  5507 | ` *` |
|        - |  5508 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5509 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5510 | ` * and decrement after that.` |
|        - |  5511 | ` */` |
|       14 |  5512 | `case PH7_OP_DECR:` |
|        - |  5513 | `#ifdef UNTRUST` |
|        - |  5514 | `	if( pTos < pStack ){` |
|        - |  5515 | `		goto Abort;` |
|        - |  5516 | `	}` |
|        - |  5517 | `#endif` |
|        - |  5518 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5519 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5520 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5521 | `			ph7_value *pObj;` |
|       27 |  5522 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5523 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5524 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5525 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5526 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5527 | `					if( pInstr->iP1 ){` |
|        - |  5528 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5529 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5530 | `					}` |
|        - |  5531 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5532 | `				}else{` |
|        - |  5533 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5534 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5535 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5536 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5537 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5538 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5539 | `					}` |
|       21 |  5540 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5541 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5542 | `						pObj->rVal--;` |
|        - |  5543 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5544 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5545 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5546 | `						 * integer-valued real. */` |
|        9 |  5547 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5548 | `					}else{` |
|       13 |  5549 | `						pObj->x.iVal--;` |
|        - |  5550 | `					}` |
|       21 |  5551 | `					if( pInstr->iP1 ){` |
|        - |  5552 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5553 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5554 | `					}` |
|        - |  5555 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5556 | `				}` |
|       13 |  5557 | `			}` |
|       14 |  5558 | `		}else{` |
|      ! 0 |  5559 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5560 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5561 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5562 | `				}else{` |
|        - |  5563 | `					/* Force a numeric cast */` |
|      ! 0 |  5564 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5565 | `					/* Pre-decrement */` |
|      ! 0 |  5566 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5567 | `						pTos->rVal--;` |
|        - |  5568 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5569 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5570 | `					}else{` |
|      ! 0 |  5571 | `						pTos->x.iVal--;` |
|      ! 0 |  5572 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5573 | `					}` |
|        - |  5574 | `				}` |
|      ! 0 |  5575 | `			}` |
|        - |  5576 | `		}` |
|       13 |  5577 | `	}` |
|       29 |  5578 | `	break;` |
|        - |  5579 | `/*` |
|        - |  5580 | ` * UMINUS: * * *` |
|        - |  5581 | ` *` |
|        - |  5582 | ` * Perform a unary minus operation.` |
|        - |  5583 | ` */` |
|    29534 |  5584 | `case PH7_OP_UMINUS:` |
|        - |  5585 | `#ifdef UNTRUST` |
|        - |  5586 | `	if( pTos < pStack ){` |
|        - |  5587 | `		goto Abort;` |
|        - |  5588 | `	}` |
|        - |  5589 | `#endif` |
|        - |  5590 | `	/* Force a numeric (integer,real or both) cast */` |
|    59070 |  5591 | `	PH7_MemObjToNumeric(pTos);` |
|    59070 |  5592 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5593 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5594 | `	}` |
|    59070 |  5595 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59040 |  5596 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29519 |  5597 | `	}` |
|    59070 |  5598 | `	break;` |
|        - |  5599 | `/*` |
|        - |  5600 | ` * UPLUS: * * *` |
|        - |  5601 | ` *` |
|        - |  5602 | ` * Perform a unary plus operation.` |
|        - |  5603 | ` */` |
|       18 |  5604 | `case PH7_OP_UPLUS:` |
|        - |  5605 | `#ifdef UNTRUST` |
|        - |  5606 | `	if( pTos < pStack ){` |
|        - |  5607 | `		goto Abort;` |
|        - |  5608 | `	}` |
|        - |  5609 | `#endif` |
|        - |  5610 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5611 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5612 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5613 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5614 | `	}` |
|       37 |  5615 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5616 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5617 | `	}` |
|       37 |  5618 | `	break;` |
|        - |  5619 | `/*` |
|        - |  5620 | ` * OP_LNOT: * * *` |
|        - |  5621 | ` *` |
|        - |  5622 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5623 | ` * with its complement.` |
|        - |  5624 | ` */` |
|    44761 |  5625 | `case PH7_OP_LNOT:` |
|        - |  5626 | `#ifdef UNTRUST` |
|        - |  5627 | `	if( pTos < pStack ){` |
|        - |  5628 | `		goto Abort;` |
|        - |  5629 | `	}` |
|        - |  5630 | `#endif` |
|        - |  5631 | `	/* Force a boolean cast */` |
|    89568 |  5632 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5633 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5634 | `	}` |
|    89568 |  5635 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89568 |  5636 | `	break;` |
|        - |  5637 | `/*` |
|        - |  5638 | ` * OP_BITNOT: * * *` |
|        - |  5639 | ` *` |
|        - |  5640 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5641 | ` * with its ones-complement.` |
|        - |  5642 | ` */` |
|       14 |  5643 | `case PH7_OP_BITNOT:` |
|        - |  5644 | `#ifdef UNTRUST` |
|        - |  5645 | `	if( pTos < pStack ){` |
|        - |  5646 | `		goto Abort;` |
|        - |  5647 | `	}` |
|        - |  5648 | `#endif` |
|        - |  5649 | `	/* Force an integer cast */` |
|       30 |  5650 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5651 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5652 | `	}` |
|       30 |  5653 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  5654 | `	break;` |
|        - |  5655 | `/* OP_MUL * * *` |
|        - |  5656 | ` * OP_MUL_STORE * * *` |
|        - |  5657 | ` *` |
|        - |  5658 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5659 | ` * and push the result back onto the stack.` |
|        - |  5660 | ` */` |
|     1290 |  5661 | `case PH7_OP_MUL:` |
|        - |  5662 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  5663 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5664 | `	/* Force the operand to be numeric */` |
|        - |  5665 | `#ifdef UNTRUST` |
|        - |  5666 | `	if( pNos < pStack ){` |
|        - |  5667 | `		goto Abort;` |
|        - |  5668 | `	}` |
|        - |  5669 | `#endif` |
|     2582 |  5670 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  5671 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5672 | `	/* Perform the requested operation */` |
|     2582 |  5673 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5674 | `		/* Floating point arithemic */` |
|        - |  5675 | `		ph7_real a,b,r;` |
|       21 |  5676 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5677 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5678 | `		}` |
|       21 |  5679 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5680 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5681 | `		}` |
|       21 |  5682 | `		a = pNos->rVal;` |
|       21 |  5683 | `		b = pTos->rVal;` |
|       21 |  5684 | `		r = a * b;` |
|        - |  5685 | `		/* Push the result */` |
|       21 |  5686 | `		pNos->rVal = r;` |
|       21 |  5687 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5688 | `		/* Try to get an integer representation */` |
|       21 |  5689 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  5690 | `	}else{` |
|        - |  5691 | `		/* Integer arithmetic */` |
|        - |  5692 | `		sxi64 a,b,r;` |
|     2562 |  5693 | `		a = pNos->x.iVal;` |
|     2562 |  5694 | `		b = pTos->x.iVal;` |
|     2562 |  5695 | `		r = a * b;` |
|        - |  5696 | `		/* Push the result */` |
|     2562 |  5697 | `		pNos->x.iVal = r;` |
|     2562 |  5698 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5699 | `	}` |
|     2582 |  5700 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5701 | `		ph7_value *pObj;` |
|       32 |  5702 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5703 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5704 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5705 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5706 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5707 | `		}` |
|       15 |  5708 | `	}` |
|     2582 |  5709 | `	VmPopOperand(&pTos,1);` |
|     2582 |  5710 | `	break;` |
|        - |  5711 | `				 }` |
|        - |  5712 | `/* OP_POW * * *` |
|        - |  5713 | ` * OP_POW_STORE * * *` |
|        - |  5714 | ` *` |
|        - |  5715 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5716 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5717 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5718 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5719 | ` */` |
|       67 |  5720 | `case PH7_OP_POW:` |
|        - |  5721 | `case PH7_OP_POW_STORE: {` |
|      135 |  5722 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  5723 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5724 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5725 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5726 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5727 | `	 */` |
|      135 |  5728 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  5729 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5730 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5731 | `	int bBothInt;` |
|      135 |  5732 | `	int usedInt = 0;` |
|        - |  5733 | `	ph7_real a, b, r;` |
|        - |  5734 | `#endif` |
|      135 |  5735 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5736 | `#ifdef UNTRUST` |
|        - |  5737 | `	if( pNos < pStack ){` |
|        - |  5738 | `		goto Abort;` |
|        - |  5739 | `	}` |
|        - |  5740 | `#endif` |
|      135 |  5741 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  5742 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5743 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  5744 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  5745 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  5746 | `	if( bBothInt ){` |
|      123 |  5747 | `		base_i = pBase->x.iVal;` |
|      123 |  5748 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5749 | `	}` |
|      135 |  5750 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5751 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5752 | `	}` |
|      135 |  5753 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  5754 | `		PH7_MemObjToReal(pExp);` |
|       66 |  5755 | `	}` |
|      135 |  5756 | `	a = pBase->rVal;` |
|      135 |  5757 | `	b = pExp->rVal;` |
|      135 |  5758 | `	r = pow(a, b);` |
|        - |  5759 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5760 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5761 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5762 | `	 * representable as double but not as signed int64. */` |
|      135 |  5763 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5764 | `		sxi64 result_i = 1;` |
|      117 |  5765 | `		sxi64 cur_base = base_i;` |
|      117 |  5766 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5767 | `		int overflow = 0;` |
|      401 |  5768 | `		while( cur_exp > 0 ){` |
|      289 |  5769 | `			if( cur_exp & 1 ){` |
|      189 |  5770 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5771 | `					overflow = 1;` |
|        3 |  5772 | `					break;` |
|        - |  5773 | `				}` |
|       93 |  5774 | `			}` |
|      287 |  5775 | `			cur_exp >>= 1;` |
|      287 |  5776 | `			if( cur_exp > 0 ){` |
|      181 |  5777 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5778 | `					overflow = 1;` |
|        3 |  5779 | `					break;` |
|        - |  5780 | `				}` |
|       89 |  5781 | `			}` |
|        1 |  5782 | `		}` |
|      117 |  5783 | `		if( !overflow ){` |
|      113 |  5784 | `			pNos->x.iVal = result_i;` |
|      113 |  5785 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5786 | `			usedInt = 1;` |
|       56 |  5787 | `		}` |
|       58 |  5788 | `	}` |
|      135 |  5789 | `	if( !usedInt ){` |
|       23 |  5790 | `		pNos->rVal = r;` |
|       23 |  5791 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  5792 | `	}` |
|        - |  5793 | `#else` |
|        - |  5794 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5795 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5796 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5797 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5798 | `	 * represented. */` |
|        - |  5799 | `	base_i = pBase->x.iVal;` |
|        - |  5800 | `	exp_i  = pExp->x.iVal;` |
|        - |  5801 | `	{` |
|        - |  5802 | `		sxi64 result_i = 1;` |
|        - |  5803 | `		sxi64 cur_base = base_i;` |
|        - |  5804 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5805 | `		if( cur_exp < 0 ){` |
|        - |  5806 | `			result_i = 0;` |
|        - |  5807 | `		}else{` |
|        - |  5808 | `			while( cur_exp > 0 ){` |
|        - |  5809 | `				if( cur_exp & 1 ){` |
|        - |  5810 | `					result_i *= cur_base;` |
|        - |  5811 | `				}` |
|        - |  5812 | `				cur_exp >>= 1;` |
|        - |  5813 | `				if( cur_exp > 0 ){` |
|        - |  5814 | `					cur_base *= cur_base;` |
|        - |  5815 | `				}` |
|        - |  5816 | `			}` |
|        - |  5817 | `		}` |
|        - |  5818 | `		pNos->x.iVal = result_i;` |
|        - |  5819 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5820 | `	}` |
|        - |  5821 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  5822 | `	if( bStore ){` |
|        - |  5823 | `		ph7_value *pObj;` |
|       23 |  5824 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5825 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5826 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5827 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5828 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5829 | `		}` |
|       11 |  5830 | `	}` |
|      135 |  5831 | `	VmPopOperand(&pTos,1);` |
|      135 |  5832 | `	break;` |
|        - |  5833 | `				 }` |
|        - |  5834 | `/* OP_ADD * * *` |
|        - |  5835 | ` *` |
|        - |  5836 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5837 | ` * and push the result back onto the stack.` |
|        - |  5838 | ` */` |
|      528 |  5839 | `case PH7_OP_ADD:{` |
|     1058 |  5840 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5841 | `#ifdef UNTRUST` |
|        - |  5842 | `	if( pNos < pStack ){` |
|        - |  5843 | `		goto Abort;` |
|        - |  5844 | `	}` |
|        - |  5845 | `#endif` |
|        - |  5846 | `	/* Perform the addition */` |
|     1058 |  5847 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1058 |  5848 | `	VmPopOperand(&pTos,1);` |
|     1058 |  5849 | `	break;` |
|        - |  5850 | `				}` |
|        - |  5851 | `/*` |
|        - |  5852 | ` * OP_ADD_STORE * * *` |
|        - |  5853 | ` *` |
|        - |  5854 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5855 | ` * and push the result back onto the stack.` |
|        - |  5856 | ` */` |
|      502 |  5857 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5858 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5859 | `	ph7_value *pObj;` |
|        - |  5860 | `	sxu32 nIdx;` |
|        - |  5861 | `#ifdef UNTRUST` |
|        - |  5862 | `	if( pNos < pStack ){` |
|        - |  5863 | `		goto Abort;` |
|        - |  5864 | `	}` |
|        - |  5865 | `#endif` |
|        - |  5866 | `	/* Perform the addition */` |
|     1006 |  5867 | `	nIdx = pTos->nIdx;` |
|     1006 |  5868 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5869 | `	/* Peform the store operation */` |
|     1006 |  5870 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5871 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5872 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5873 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5874 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5875 | `	}` |
|        - |  5876 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5877 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5878 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5879 | `	break;` |
|        - |  5880 | `				}` |
|        - |  5881 | `/* OP_SUB * * *` |
|        - |  5882 | ` *` |
|        - |  5883 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5884 | ` * first (what was next on the stack) from the second (the` |
|        - |  5885 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5886 | ` */` |
|      349 |  5887 | `case PH7_OP_SUB: {` |
|      700 |  5888 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5889 | `#ifdef UNTRUST` |
|        - |  5890 | `	if( pNos < pStack ){` |
|        - |  5891 | `		goto Abort;` |
|        - |  5892 | `	}` |
|        - |  5893 | `#endif` |
|      700 |  5894 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5895 | `		/* Floating point arithemic */` |
|        - |  5896 | `		ph7_real a,b,r;` |
|       97 |  5897 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5898 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5899 | `		}` |
|       97 |  5900 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5901 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5902 | `		}` |
|       97 |  5903 | `		a = pNos->rVal;` |
|       97 |  5904 | `		b = pTos->rVal;` |
|       97 |  5905 | `		r = a - b;` |
|        - |  5906 | `		/* Push the result */` |
|       97 |  5907 | `		pNos->rVal = r;` |
|       97 |  5908 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5909 | `		/* Try to get an integer representation */` |
|       97 |  5910 | `		PH7_MemObjTryInteger(pNos);` |
|       49 |  5911 | `	}else{` |
|        - |  5912 | `		/* Integer arithmetic */` |
|        - |  5913 | `		sxi64 a,b,r;` |
|      604 |  5914 | `		a = pNos->x.iVal;` |
|      604 |  5915 | `		b = pTos->x.iVal;` |
|      604 |  5916 | `		r = a - b;` |
|        - |  5917 | `		/* Push the result */` |
|      604 |  5918 | `		pNos->x.iVal = r;` |
|      604 |  5919 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5920 | `	}` |
|      700 |  5921 | `	VmPopOperand(&pTos,1);` |
|      700 |  5922 | `	break;` |
|        - |  5923 | `				 }` |
|        - |  5924 | `/* OP_SUB_STORE * * *` |
|        - |  5925 | ` *` |
|        - |  5926 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5927 | ` * first (what was next on the stack) from the second (the` |
|        - |  5928 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5929 | ` */` |
|        4 |  5930 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5931 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5932 | `	ph7_value *pObj;` |
|        - |  5933 | `#ifdef UNTRUST` |
|        - |  5934 | `	if( pNos < pStack ){` |
|        - |  5935 | `		goto Abort;` |
|        - |  5936 | `	}` |
|        - |  5937 | `#endif` |
|       10 |  5938 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5939 | `		/* Floating point arithemic */` |
|        - |  5940 | `		ph7_real a,b,r;` |
|      ! 0 |  5941 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5942 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5943 | `		}` |
|      ! 0 |  5944 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5945 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5946 | `		}` |
|      ! 0 |  5947 | `		a = pTos->rVal;` |
|      ! 0 |  5948 | `		b = pNos->rVal;` |
|      ! 0 |  5949 | `		r = a - b;` |
|        - |  5950 | `		/* Push the result */` |
|      ! 0 |  5951 | `		pNos->rVal = r;` |
|      ! 0 |  5952 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5953 | `		/* Try to get an integer representation */` |
|      ! 0 |  5954 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5955 | `	}else{` |
|        - |  5956 | `		/* Integer arithmetic */` |
|        - |  5957 | `		sxi64 a,b,r;` |
|       10 |  5958 | `		a = pTos->x.iVal;` |
|       10 |  5959 | `		b = pNos->x.iVal;` |
|       10 |  5960 | `		r = a - b;` |
|        - |  5961 | `		/* Push the result */` |
|       10 |  5962 | `		pNos->x.iVal = r;` |
|       10 |  5963 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5964 | `	}` |
|       10 |  5965 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5966 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5967 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5968 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5969 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5970 | `	}` |
|       10 |  5971 | `	VmPopOperand(&pTos,1);` |
|       10 |  5972 | `	break;` |
|        - |  5973 | `				 }` |
|        - |  5974 |  |
|        - |  5975 | `/*` |
|        - |  5976 | ` * OP_MOD * * *` |
|        - |  5977 | ` *` |
|        - |  5978 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5979 | ` * first (what was next on the stack) from the second (the` |
|        - |  5980 | ` * top of the stack) and push the remainder after division` |
|        - |  5981 | ` * onto the stack.` |
|        - |  5982 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5983 | ` */` |
|      308 |  5984 | `case PH7_OP_MOD:{` |
|      618 |  5985 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5986 | `	sxi64 a,b,r;` |
|        - |  5987 | `#ifdef UNTRUST` |
|        - |  5988 | `	if( pNos < pStack ){` |
|        - |  5989 | `		goto Abort;` |
|        - |  5990 | `	}` |
|        - |  5991 | `#endif` |
|        - |  5992 | `	/* Force the operands to be integer */` |
|      618 |  5993 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5994 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5995 | `	}` |
|      618 |  5996 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5997 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5998 | `	}` |
|        - |  5999 | `	/* Perform the requested operation */` |
|      618 |  6000 | `	a = pNos->x.iVal;` |
|      618 |  6001 | `	b = pTos->x.iVal;` |
|      618 |  6002 | `	if( b == 0 ){` |
|        3 |  6003 | `		r = 0;` |
|        3 |  6004 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6005 | `		/* goto Abort; */` |
|        2 |  6006 | `	}else{` |
|      615 |  6007 | `		r = a%b;` |
|        - |  6008 | `	}` |
|        - |  6009 | `	/* Push the result */` |
|      618 |  6010 | `	pNos->x.iVal = r;` |
|      618 |  6011 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  6012 | `	VmPopOperand(&pTos,1);` |
|      618 |  6013 | `	break;` |
|        - |  6014 | `				}` |
|        - |  6015 | `/*` |
|        - |  6016 | ` * OP_MOD_STORE * * *` |
|        - |  6017 | ` *` |
|        - |  6018 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6019 | ` * first (what was next on the stack) from the second (the` |
|        - |  6020 | ` * top of the stack) and push the remainder after division` |
|        - |  6021 | ` * onto the stack.` |
|        - |  6022 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6023 | ` */` |
|        1 |  6024 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6025 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6026 | `	ph7_value *pObj;` |
|        - |  6027 | `	sxi64 a,b,r;` |
|        - |  6028 | `#ifdef UNTRUST` |
|        - |  6029 | `	if( pNos < pStack ){` |
|        - |  6030 | `		goto Abort;` |
|        - |  6031 | `	}` |
|        - |  6032 | `#endif` |
|        - |  6033 | `	/* Force the operands to be integer */` |
|        3 |  6034 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6035 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6036 | `	}` |
|        3 |  6037 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6038 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6039 | `	}` |
|        - |  6040 | `	/* Perform the requested operation */` |
|        3 |  6041 | `	a = pTos->x.iVal;` |
|        3 |  6042 | `	b = pNos->x.iVal;` |
|        3 |  6043 | `	if( b == 0 ){` |
|      ! 0 |  6044 | `		r = 0;` |
|      ! 0 |  6045 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6046 | `		/* goto Abort; */` |
|      ! 0 |  6047 | `	}else{` |
|        3 |  6048 | `		r = a%b;` |
|        - |  6049 | `	}` |
|        - |  6050 | `	/* Push the result */` |
|        3 |  6051 | `	pNos->x.iVal = r;` |
|        3 |  6052 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6053 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6054 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6055 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6056 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6057 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6058 | `	}` |
|        3 |  6059 | `	VmPopOperand(&pTos,1);` |
|        3 |  6060 | `	break;` |
|        - |  6061 | `				}` |
|        - |  6062 | `/*` |
|        - |  6063 | ` * OP_DIV * * *` |
|        - |  6064 | ` *` |
|        - |  6065 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6066 | ` * first (what was next on the stack) from the second (the` |
|        - |  6067 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6068 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6069 | ` */` |
|       33 |  6070 | `case PH7_OP_DIV:{` |
|       68 |  6071 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6072 | `	ph7_real a,b,r;` |
|        - |  6073 | `#ifdef UNTRUST` |
|        - |  6074 | `	if( pNos < pStack ){` |
|        - |  6075 | `		goto Abort;` |
|        - |  6076 | `	}` |
|        - |  6077 | `#endif` |
|        - |  6078 | `	/* Force the operands to be real */` |
|       68 |  6079 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6080 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6081 | `	}` |
|       68 |  6082 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6083 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6084 | `	}` |
|        - |  6085 | `	/* Perform the requested operation */` |
|       68 |  6086 | `	a = pNos->rVal;` |
|       68 |  6087 | `	b = pTos->rVal;` |
|       68 |  6088 | `	if( b == 0 ){` |
|        - |  6089 | `		/* Division by zero */` |
|        3 |  6090 | `		pNos->rVal = 0;` |
|        3 |  6091 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6092 | `		/* goto Abort; */` |
|        2 |  6093 | `	}else{` |
|       65 |  6094 | `		r = a/b;` |
|        - |  6095 | `		/* Push the result */` |
|       65 |  6096 | `		pNos->rVal = r;` |
|       65 |  6097 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6098 | `		/* Try to get an integer representation */` |
|       65 |  6099 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6100 | `	}` |
|       68 |  6101 | `	VmPopOperand(&pTos,1);` |
|       68 |  6102 | `	break;` |
|        - |  6103 | `				}` |
|        - |  6104 | `/*` |
|        - |  6105 | ` * OP_DIV_STORE * * *` |
|        - |  6106 | ` *` |
|        - |  6107 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6108 | ` * first (what was next on the stack) from the second (the` |
|        - |  6109 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6110 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6111 | ` */` |
|        2 |  6112 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6113 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6114 | `	ph7_value *pObj;` |
|        - |  6115 | `	ph7_real a,b,r;` |
|        - |  6116 | `#ifdef UNTRUST` |
|        - |  6117 | `	if( pNos < pStack ){` |
|        - |  6118 | `		goto Abort;` |
|        - |  6119 | `	}` |
|        - |  6120 | `#endif` |
|        - |  6121 | `	/* Force the operands to be real */` |
|        5 |  6122 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6123 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6124 | `	}` |
|        5 |  6125 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6126 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6127 | `	}` |
|        - |  6128 | `	/* Perform the requested operation */` |
|        5 |  6129 | `	a = pTos->rVal;` |
|        5 |  6130 | `	b = pNos->rVal;` |
|        5 |  6131 | `	if( b == 0 ){` |
|        - |  6132 | `		/* Division by zero */` |
|      ! 0 |  6133 | `		r = 0;` |
|      ! 0 |  6134 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6135 | `		/* goto Abort; */` |
|      ! 0 |  6136 | `	}else{` |
|        5 |  6137 | `		r = a/b;` |
|        - |  6138 | `		/* Push the result */` |
|        5 |  6139 | `		pNos->rVal = r;` |
|        5 |  6140 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6141 | `		/* Try to get an integer representation */` |
|        5 |  6142 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6143 | `	}` |
|        5 |  6144 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6145 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6146 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6147 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6148 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6149 | `	}` |
|        5 |  6150 | `	VmPopOperand(&pTos,1);` |
|        5 |  6151 | `	break;` |
|        - |  6152 | `				}` |
|        - |  6153 | `/* OP_BAND * * *` |
|        - |  6154 | ` *` |
|        - |  6155 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6156 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6157 | ` * two elements.` |
|        - |  6158 | `*/` |
|        - |  6159 | `/* OP_BOR * * *` |
|        - |  6160 | ` *` |
|        - |  6161 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6162 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6163 | ` * two elements.` |
|        - |  6164 | ` */` |
|        - |  6165 | `/* OP_BXOR * * *` |
|        - |  6166 | ` *` |
|        - |  6167 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6168 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6169 | ` * two elements.` |
|        - |  6170 | ` */` |
|       43 |  6171 | `case PH7_OP_BAND:` |
|        - |  6172 | `case PH7_OP_BOR:` |
|        - |  6173 | `case PH7_OP_BXOR:{` |
|       88 |  6174 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6175 | `	sxi64 a,b,r;` |
|        - |  6176 | `#ifdef UNTRUST` |
|        - |  6177 | `	if( pNos < pStack ){` |
|        - |  6178 | `		goto Abort;` |
|        - |  6179 | `	}` |
|        - |  6180 | `#endif` |
|        - |  6181 | `	/* Force the operands to be integer */` |
|       88 |  6182 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6183 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6184 | `	}` |
|       88 |  6185 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6186 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6187 | `	}` |
|        - |  6188 | `	/* Perform the requested operation */` |
|       88 |  6189 | `	a = pNos->x.iVal;` |
|       88 |  6190 | `	b = pTos->x.iVal;` |
|       88 |  6191 | `	switch(pInstr->iOp){` |
|        7 |  6192 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6193 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6194 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6195 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6196 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6197 | `	case PH7_OP_BAND:` |
|       60 |  6198 | `	default:          r = a&b; break;` |
|        - |  6199 | `	}` |
|        - |  6200 | `	/* Push the result */` |
|       88 |  6201 | `	pNos->x.iVal = r;` |
|       88 |  6202 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6203 | `	VmPopOperand(&pTos,1);` |
|       88 |  6204 | `	break;` |
|        - |  6205 | `				 }` |
|        - |  6206 | `/* OP_BAND_STORE * * *` |
|        - |  6207 | ` *` |
|        - |  6208 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6209 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6210 | ` * two elements.` |
|        - |  6211 | `*/` |
|        - |  6212 | `/* OP_BOR_STORE * * *` |
|        - |  6213 | ` *` |
|        - |  6214 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6215 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6216 | ` * two elements.` |
|        - |  6217 | ` */` |
|        - |  6218 | `/* OP_BXOR_STORE * * *` |
|        - |  6219 | ` *` |
|        - |  6220 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6221 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6222 | ` * two elements.` |
|        - |  6223 | ` */` |
|       10 |  6224 | `case PH7_OP_BAND_STORE:` |
|        - |  6225 | `case PH7_OP_BOR_STORE:` |
|        - |  6226 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6227 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6228 | `	ph7_value *pObj;` |
|        - |  6229 | `	sxi64 a,b,r;` |
|        - |  6230 | `#ifdef UNTRUST` |
|        - |  6231 | `	if( pNos < pStack ){` |
|        - |  6232 | `		goto Abort;` |
|        - |  6233 | `	}` |
|        - |  6234 | `#endif` |
|        - |  6235 | `	/* Force the operands to be integer */` |
|       21 |  6236 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6237 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6238 | `	}` |
|       21 |  6239 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6240 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6241 | `	}` |
|        - |  6242 | `	/* Perform the requested operation */` |
|       21 |  6243 | `	a = pTos->x.iVal;` |
|       21 |  6244 | `	b = pNos->x.iVal;` |
|       21 |  6245 | `	switch(pInstr->iOp){` |
|        3 |  6246 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6247 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6248 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6249 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6250 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6251 | `	case PH7_OP_BAND:` |
|        7 |  6252 | `	default:          r = a&b; break;` |
|        - |  6253 | `	}` |
|        - |  6254 | `	/* Push the result */` |
|       21 |  6255 | `	pNos->x.iVal = r;` |
|       21 |  6256 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6257 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6258 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6259 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6260 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6261 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6262 | `	}` |
|       21 |  6263 | `	VmPopOperand(&pTos,1);` |
|       21 |  6264 | `	break;` |
|        - |  6265 | `				 }` |
|        - |  6266 | `/* OP_SHL * * *` |
|        - |  6267 | ` *` |
|        - |  6268 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6269 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6270 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6271 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6272 | ` */` |
|        - |  6273 | `/* OP_SHR * * *` |
|        - |  6274 | ` *` |
|        - |  6275 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6276 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6277 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6278 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6279 | ` */` |
|       12 |  6280 | `case PH7_OP_SHL:` |
|        - |  6281 | `case PH7_OP_SHR: {` |
|       25 |  6282 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6283 | `	sxi64 a,r;` |
|        - |  6284 | `	sxi32 b;` |
|        - |  6285 | `#ifdef UNTRUST` |
|        - |  6286 | `	if( pNos < pStack ){` |
|        - |  6287 | `		goto Abort;` |
|        - |  6288 | `	}` |
|        - |  6289 | `#endif` |
|        - |  6290 | `	/* Force the operands to be integer */` |
|       25 |  6291 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6292 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6293 | `	}` |
|       25 |  6294 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6295 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6296 | `	}` |
|        - |  6297 | `	/* Perform the requested operation */` |
|       25 |  6298 | `	a = pNos->x.iVal;` |
|       25 |  6299 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6300 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6301 | `		r = a << b;` |
|        8 |  6302 | `	}else{` |
|       11 |  6303 | `		r = a >> b;` |
|        - |  6304 | `	}` |
|        - |  6305 | `	/* Push the result */` |
|       25 |  6306 | `	pNos->x.iVal = r;` |
|       25 |  6307 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6308 | `	VmPopOperand(&pTos,1);` |
|       25 |  6309 | `	break;` |
|        - |  6310 | `				 }` |
|        - |  6311 | `/*  OP_SHL_STORE * * *` |
|        - |  6312 | ` *` |
|        - |  6313 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6314 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6315 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6316 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6317 | ` */` |
|        - |  6318 | `/* OP_SHR_STORE * * *` |
|        - |  6319 | ` *` |
|        - |  6320 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6321 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6322 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6323 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6324 | ` */` |
|        9 |  6325 | `case PH7_OP_SHL_STORE:` |
|        - |  6326 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6327 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6328 | `	ph7_value *pObj;` |
|        - |  6329 | `	sxi64 a,r;` |
|        - |  6330 | `	sxi32 b;` |
|        - |  6331 | `#ifdef UNTRUST` |
|        - |  6332 | `	if( pNos < pStack ){` |
|        - |  6333 | `		goto Abort;` |
|        - |  6334 | `	}` |
|        - |  6335 | `#endif` |
|        - |  6336 | `	/* Force the operands to be integer */` |
|       19 |  6337 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6338 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6339 | `	}` |
|       19 |  6340 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6341 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6342 | `	}` |
|        - |  6343 | `	/* Perform the requested operation */` |
|       19 |  6344 | `	a = pTos->x.iVal;` |
|       19 |  6345 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6346 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6347 | `		r = a << b;` |
|        5 |  6348 | `	}else{` |
|       11 |  6349 | `		r = a >> b;` |
|        - |  6350 | `	}` |
|        - |  6351 | `	/* Push the result */` |
|       19 |  6352 | `	pNos->x.iVal = r;` |
|       19 |  6353 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6354 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6355 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6356 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6357 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6358 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6359 | `	}` |
|       19 |  6360 | `	VmPopOperand(&pTos,1);` |
|       19 |  6361 | `	break;` |
|        - |  6362 | `				 }` |
|        - |  6363 | `/* CAT:  P1 * *` |
|        - |  6364 | ` *` |
|        - |  6365 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6366 | ` * back.` |
|        - |  6367 | ` */` |
|    71560 |  6368 | `case PH7_OP_CAT:{` |
|        - |  6369 | `	ph7_value *pNos,*pCur;` |
|   143122 |  6370 | `	if( pInstr->iP1 < 1 ){` |
|   115642 |  6371 | `		pNos = &pTos[-1];` |
|    57822 |  6372 | `	}else{` |
|    27482 |  6373 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6374 | `	}` |
|        - |  6375 | `#ifdef UNTRUST` |
|        - |  6376 | `	if( pNos < pStack ){` |
|        - |  6377 | `		goto Abort;` |
|        - |  6378 | `	}` |
|        - |  6379 | `#endif` |
|        - |  6380 | `	/* Force a string cast */` |
|   143122 |  6381 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1662 |  6382 | `		PH7_MemObjToString(pNos);` |
|      830 |  6383 | `	}` |
|   143122 |  6384 | `	pCur = &pNos[1];` |
|   288966 |  6385 | `	while( pCur <= pTos ){` |
|   145846 |  6386 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50918 |  6387 | `			PH7_MemObjToString(pCur);` |
|    25458 |  6388 | `		}` |
|        - |  6389 | `		/* Perform the concatenation */` |
|   145846 |  6390 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   145802 |  6391 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    72900 |  6392 | `		}` |
|   145846 |  6393 | `		SyBlobRelease(&pCur->sBlob);` |
|   145846 |  6394 | `		pCur++;` |
|        2 |  6395 | `	}` |
|   143122 |  6396 | `	pTos = pNos;` |
|   143122 |  6397 | `	break;` |
|        - |  6398 | `				}` |
|        - |  6399 | `/*  CAT_STORE: * * *` |
|        - |  6400 | ` *` |
|        - |  6401 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6402 | ` * back.` |
|        - |  6403 | ` */` |
|     4112 |  6404 | `case PH7_OP_CAT_STORE:{` |
|     8226 |  6405 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6406 | `	ph7_value *pObj;` |
|        - |  6407 | `#ifdef UNTRUST` |
|        - |  6408 | `	if( pNos < pStack ){` |
|        - |  6409 | `		goto Abort;` |
|        - |  6410 | `	}` |
|        - |  6411 | `#endif` |
|     8226 |  6412 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6413 | `		/* Force a string cast */` |
|        3 |  6414 | `		PH7_MemObjToString(pTos);` |
|        1 |  6415 | `	}` |
|     8226 |  6416 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6417 | `		/* Force a string cast */` |
|      ! 0 |  6418 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6419 | `	}` |
|        - |  6420 | `	/* Perform the concatenation (Reverse order) */` |
|     8226 |  6421 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8226 |  6422 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     4112 |  6423 | `	}` |
|        - |  6424 | `	/* Perform the store operation */` |
|     8226 |  6425 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6426 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8226 |  6427 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8226 |  6428 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8224 |  6429 | `		PH7_MemObjStore(pTos,pObj);` |
|     4111 |  6430 | `	}` |
|     8224 |  6431 | `	PH7_MemObjStore(pTos,pNos);` |
|     8224 |  6432 | `	VmPopOperand(&pTos,1);` |
|     8224 |  6433 | `	break;` |
|        - |  6434 | `				}` |
|        - |  6435 | `/* OP_AND: * * *` |
|        - |  6436 | ` *` |
|        - |  6437 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6438 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6439 | ` * stack.` |
|        - |  6440 | ` */` |
|        - |  6441 | `/* OP_OR: * * *` |
|        - |  6442 | ` *` |
|        - |  6443 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6444 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6445 | ` * stack.` |
|        - |  6446 | ` */` |
|   108089 |  6447 | `case PH7_OP_LAND:` |
|        - |  6448 | `case PH7_OP_LOR: {` |
|   216224 |  6449 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6450 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6451 | `#ifdef UNTRUST` |
|        - |  6452 | `	if( pNos < pStack ){` |
|        - |  6453 | `		goto Abort;` |
|        - |  6454 | `	}` |
|        - |  6455 | `#endif` |
|        - |  6456 | `	/* Force a boolean cast */` |
|   216224 |  6457 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6458 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6459 | `	}` |
|   216224 |  6460 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6461 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6462 | `	}` |
|   216224 |  6463 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216224 |  6464 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216224 |  6465 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6466 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99136 |  6467 | `		v1 = and_logic[v1*3+v2];` |
|    49591 |  6468 | `	}else{` |
|        - |  6469 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117090 |  6470 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6471 | `	}` |
|   216224 |  6472 | `	if( v1 == 2 ){` |
|      ! 0 |  6473 | `		v1 = 1;` |
|      ! 0 |  6474 | `	}` |
|   216224 |  6475 | `	VmPopOperand(&pTos,1);` |
|   216224 |  6476 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216224 |  6477 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216224 |  6478 | `	break;` |
|        - |  6479 | `				 }` |
|        - |  6480 | `/*` |
|        - |  6481 | ` * OP_NULLC: * * *` |
|        - |  6482 | ` * Null coalescing operator '??'.` |
|        - |  6483 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6484 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6485 | ` */` |
|        - |  6486 | `/*` |
|        - |  6487 | ` * OP_NULLC: * P2 *` |
|        - |  6488 | ` * Short-circuit null coalescing '??'.` |
|        - |  6489 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6490 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6491 | ` */` |
|       93 |  6492 | `case PH7_OP_NULLC: {` |
|        - |  6493 | `#ifdef UNTRUST` |
|        - |  6494 | `	if( pTos < pStack ){` |
|        - |  6495 | `		goto Abort;` |
|        - |  6496 | `	}` |
|        - |  6497 | `#endif` |
|      188 |  6498 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6499 | `		/* Left is not null — keep it and skip the RHS */` |
|      114 |  6500 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       58 |  6501 | `	}else{` |
|        - |  6502 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       76 |  6503 | `		VmPopOperand(&pTos, 1);` |
|        - |  6504 | `	}` |
|      188 |  6505 | `	break;` |
|        - |  6506 |  |
|        - |  6507 | `/*` |
|        - |  6508 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6509 | ` * Null coalescing assignment short-circuit.` |
|        - |  6510 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6511 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6512 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6513 | ` */` |
|       28 |  6514 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6515 | `#ifdef UNTRUST` |
|        - |  6516 | `	if( pTos < pStack ){` |
|        - |  6517 | `		goto Abort;` |
|        - |  6518 | `	}` |
|        - |  6519 | `#endif` |
|       58 |  6520 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6521 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6522 | `	}` |
|       58 |  6523 | `	break;` |
|        - |  6524 |  |
|        - |  6525 | `/*` |
|        - |  6526 | ` * OP_NULLC_STORE: * * *` |
|        - |  6527 | ` * Null coalescing assignment store.` |
|        - |  6528 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6529 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6530 | ` * expression result.` |
|        - |  6531 | ` */` |
|        - |  6532 | `/*` |
|        - |  6533 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6534 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6535 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6536 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6537 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6538 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6539 | ` */` |
|       51 |  6540 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6541 | `#ifdef UNTRUST` |
|        - |  6542 | `	if( pTos < pStack ){` |
|        - |  6543 | `		goto Abort;` |
|        - |  6544 | `	}` |
|        - |  6545 | `#endif` |
|      104 |  6546 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6547 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6548 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6549 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6550 | `	}` |
|      104 |  6551 | `	break;` |
|        - |  6552 |  |
|       17 |  6553 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6554 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6555 | `	ph7_value *pObj;` |
|        - |  6556 | `	sxu32 nIdx;` |
|        - |  6557 | `#ifdef UNTRUST` |
|        - |  6558 | `	if( pNos < pStack ){` |
|        - |  6559 | `		goto Abort;` |
|        - |  6560 | `	}` |
|        - |  6561 | `#endif` |
|        - |  6562 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6563 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6564 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6565 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6566 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6567 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6568 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6569 | `		ph7_value *apArg[2];` |
|        5 |  6570 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6571 | `		apArg[1] = pTos;` |
|        5 |  6572 | `		if( pSet ){` |
|        5 |  6573 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6574 | `		}` |
|        - |  6575 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6576 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6577 | `		VmPopOperand(&pTos,1);` |
|        - |  6578 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6579 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6580 | `		break;` |
|        - |  6581 | `	}` |
|       32 |  6582 | `	nIdx = pNos->nIdx;` |
|       32 |  6583 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6584 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6585 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6586 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6587 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6588 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6589 | `	}` |
|       32 |  6590 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6591 | `	VmPopOperand(&pTos,1);` |
|       32 |  6592 | `	break;` |
|        - |  6593 |  |
|        - |  6594 | `/*` |
|        - |  6595 | ` * OP_SPREAD: * * *` |
|        - |  6596 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6597 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6598 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6599 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6600 | ` */` |
|        9 |  6601 | `case PH7_OP_SPREAD: {` |
|        - |  6602 | `#ifdef UNTRUST` |
|        - |  6603 | `	if( pTos < pStack ){` |
|        - |  6604 | `		goto Abort;` |
|        - |  6605 | `	}` |
|        - |  6606 | `#endif` |
|       20 |  6607 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6608 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6609 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6610 | `		if( nEntry == 0 ){` |
|        - |  6611 | `			/* Empty array — remove from stack */` |
|        3 |  6612 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6613 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6614 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6615 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6616 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6617 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6618 | `				VM_STACK_GUARD);` |
|      ! 0 |  6619 | `		}else{` |
|        - |  6620 | `			ph7_hashmap_node *pNode2;` |
|        - |  6621 | `			ph7_value *pElem;` |
|        - |  6622 | `			sxu32 i;` |
|        - |  6623 | `			/* Overwrite TOS with first element */` |
|       18 |  6624 | `			pNode2 = pMap->pFirst;` |
|       18 |  6625 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6626 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6627 | `			if( pElem ){` |
|       18 |  6628 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6629 | `			}` |
|       18 |  6630 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6631 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6632 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6633 | `			pNode2 = pNode2->pPrev;` |
|        - |  6634 | `			/* Push remaining elements */` |
|       44 |  6635 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6636 | `				pTos++;` |
|       28 |  6637 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6638 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6639 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6640 | `				if( pElem ){` |
|       28 |  6641 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6642 | `				}` |
|       28 |  6643 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6644 | `			}` |
|       18 |  6645 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6646 | `		}` |
|        9 |  6647 | `	}` |
|        - |  6648 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6649 | `	break;` |
|        - |  6650 |  |
|        - |  6651 | `/*` |
|        - |  6652 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6653 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6654 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6655 | ` */` |
|       34 |  6656 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6657 | `#ifdef UNTRUST` |
|        - |  6658 | `	if( pTos < pStack ){` |
|        - |  6659 | `		goto Abort;` |
|        - |  6660 | `	}` |
|        - |  6661 | `#endif` |
|       70 |  6662 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  6663 | `	break;` |
|        - |  6664 |  |
|        - |  6665 | `/* OP_LXOR: * * *` |
|        - |  6666 | ` *` |
|        - |  6667 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6668 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6669 | ` * stack.` |
|        - |  6670 | ` * According to the PHP language reference manual:` |
|        - |  6671 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6672 | ` *  TRUE,but not both.` |
|        - |  6673 | ` */` |
|        5 |  6674 | `case PH7_OP_LXOR:{` |
|       11 |  6675 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6676 | `	sxi32 v = 0;` |
|        - |  6677 | `#ifdef UNTRUST` |
|        - |  6678 | `	if( pNos < pStack ){` |
|        - |  6679 | `		goto Abort;` |
|        - |  6680 | `	}` |
|        - |  6681 | `#endif` |
|        - |  6682 | `	/* Force a boolean cast */` |
|       11 |  6683 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6684 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6685 | `	}` |
|       11 |  6686 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6687 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6688 | `	}` |
|       11 |  6689 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6690 | `		v = 1;` |
|        3 |  6691 | `	}` |
|       11 |  6692 | `	VmPopOperand(&pTos,1);` |
|       11 |  6693 | `	pTos->x.iVal = v;` |
|       11 |  6694 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6695 | `	break;` |
|        - |  6696 | `				 }` |
|        - |  6697 | `/* OP_EQ P1 P2 P3` |
|        - |  6698 | ` *` |
|        - |  6699 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6700 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6701 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6702 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6703 | ` */` |
|        - |  6704 | `/* OP_NEQ P1 P2 P3` |
|        - |  6705 | ` *` |
|        - |  6706 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6707 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6708 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6709 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6710 | ` */` |
|     4557 |  6711 | `case PH7_OP_EQ:` |
|        - |  6712 | `case PH7_OP_NEQ: {` |
|     9116 |  6713 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6714 | `	/* Perform the comparison and act accordingly */` |
|        - |  6715 | `#ifdef UNTRUST` |
|        - |  6716 | `	if( pNos < pStack ){` |
|        - |  6717 | `		goto Abort;` |
|        - |  6718 | `	}` |
|        - |  6719 | `#endif` |
|     9116 |  6720 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9116 |  6721 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6722 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9107 |  6723 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9072 |  6724 | `		rc = rc == 0;` |
|     4537 |  6725 | `	}else{` |
|       28 |  6726 | `		rc = rc != 0;` |
|        - |  6727 | `	}` |
|     9116 |  6728 | `	VmPopOperand(&pTos,1);` |
|     9116 |  6729 | `	if( !pInstr->iP2 ){` |
|        - |  6730 | `		/* Push comparison result without taking the jump */` |
|     9116 |  6731 | `		PH7_MemObjRelease(pTos);` |
|     9116 |  6732 | `		pTos->x.iVal = rc;` |
|        - |  6733 | `		/* Invalidate any prior representation */` |
|     9116 |  6734 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4559 |  6735 | `	}else{` |
|      ! 0 |  6736 | `		if( rc ){` |
|        - |  6737 | `			/* Jump to the desired location */` |
|      ! 0 |  6738 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6739 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6740 | `		}` |
|        - |  6741 | `	}` |
|     9116 |  6742 | `	break;` |
|        - |  6743 | `				 }` |
|        - |  6744 | `/* OP_TEQ P1 P2 *` |
|        - |  6745 | ` *` |
|        - |  6746 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6747 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6748 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6749 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6750 | ` */` |
|   160791 |  6751 | `case PH7_OP_TEQ: {` |
|   321584 |  6752 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6753 | `	/* Perform the comparison and act accordingly */` |
|        - |  6754 | `#ifdef UNTRUST` |
|        - |  6755 | `	if( pNos < pStack ){` |
|        - |  6756 | `		goto Abort;` |
|        - |  6757 | `	}` |
|        - |  6758 | `#endif` |
|   321584 |  6759 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   321584 |  6760 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6761 | `		rc = 0;` |
|        2 |  6762 | `	}else{` |
|   321582 |  6763 | `		rc = rc == 0;` |
|        - |  6764 | `	}` |
|   321584 |  6765 | `	VmPopOperand(&pTos,1);` |
|   321584 |  6766 | `	if( !pInstr->iP2 ){` |
|        - |  6767 | `		/* Push comparison result without taking the jump */` |
|   321584 |  6768 | `		PH7_MemObjRelease(pTos);` |
|   321584 |  6769 | `		pTos->x.iVal = rc;` |
|        - |  6770 | `		/* Invalidate any prior representation */` |
|   321584 |  6771 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   160793 |  6772 | `	}else{` |
|      ! 0 |  6773 | `		if( rc ){` |
|        - |  6774 | `			/* Jump to the desired location */` |
|      ! 0 |  6775 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6776 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6777 | `		}` |
|        - |  6778 | `	}` |
|   321584 |  6779 | `	break;` |
|        - |  6780 | `				 }` |
|        - |  6781 | `/* OP_TNE P1 P2 *` |
|        - |  6782 | ` *` |
|        - |  6783 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6784 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6785 | ` * instruction.` |
|        - |  6786 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6787 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6788 | ` *` |
|        - |  6789 | ` */` |
|   123709 |  6790 | `case PH7_OP_TNE: {` |
|   247420 |  6791 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6792 | `	/* Perform the comparison and act accordingly */` |
|        - |  6793 | `#ifdef UNTRUST` |
|        - |  6794 | `	if( pNos < pStack ){` |
|        - |  6795 | `		goto Abort;` |
|        - |  6796 | `	}` |
|        - |  6797 | `#endif` |
|   247420 |  6798 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   247420 |  6799 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6800 | `		rc = 1;` |
|        2 |  6801 | `	}else{` |
|   247418 |  6802 | `		rc = rc != 0;` |
|        - |  6803 | `	}` |
|   247420 |  6804 | `	VmPopOperand(&pTos,1);` |
|   247420 |  6805 | `	if( !pInstr->iP2 ){` |
|        - |  6806 | `		/* Push comparison result without taking the jump */` |
|   247420 |  6807 | `		PH7_MemObjRelease(pTos);` |
|   247420 |  6808 | `		pTos->x.iVal = rc;` |
|        - |  6809 | `		/* Invalidate any prior representation */` |
|   247420 |  6810 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   123711 |  6811 | `	}else{` |
|      ! 0 |  6812 | `		if( rc ){` |
|        - |  6813 | `			/* Jump to the desired location */` |
|      ! 0 |  6814 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6815 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6816 | `		}` |
|        - |  6817 | `	}` |
|   247420 |  6818 | `	break;` |
|        - |  6819 | `				 }` |
|        - |  6820 | `/* OP_LT P1 P2 P3` |
|        - |  6821 | ` *` |
|        - |  6822 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6823 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6824 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6825 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6826 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6827 | ` *` |
|        - |  6828 | ` */` |
|        - |  6829 | `/* OP_LE P1 P2 P3` |
|        - |  6830 | ` *` |
|        - |  6831 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6832 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6833 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6834 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6835 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6836 | ` *` |
|        - |  6837 | ` */` |
|   112421 |  6838 | `case PH7_OP_LT:` |
|        - |  6839 | `case PH7_OP_LE: {` |
|   224888 |  6840 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6841 | `	/* Perform the comparison and act accordingly */` |
|        - |  6842 | `#ifdef UNTRUST` |
|        - |  6843 | `	if( pNos < pStack ){` |
|        - |  6844 | `		goto Abort;` |
|        - |  6845 | `	}` |
|        - |  6846 | `#endif` |
|   224888 |  6847 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224888 |  6848 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6849 | `		rc = 0;` |
|   224884 |  6850 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  6851 | `		rc = rc < 1;` |
|      805 |  6852 | `	}else{` |
|   223274 |  6853 | `		rc = rc < 0;` |
|        - |  6854 | `	}` |
|   224888 |  6855 | `	VmPopOperand(&pTos,1);` |
|   224888 |  6856 | `	if( !pInstr->iP2 ){` |
|        - |  6857 | `		/* Push comparison result without taking the jump */` |
|   224888 |  6858 | `		PH7_MemObjRelease(pTos);` |
|   224888 |  6859 | `		pTos->x.iVal = rc;` |
|        - |  6860 | `		/* Invalidate any prior representation */` |
|   224888 |  6861 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112467 |  6862 | `	}else{` |
|      ! 0 |  6863 | `		if( rc ){` |
|        - |  6864 | `			/* Jump to the desired location */` |
|      ! 0 |  6865 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6866 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6867 | `		}` |
|        - |  6868 | `	}` |
|   224888 |  6869 | `	break;` |
|        - |  6870 | `				}` |
|        - |  6871 | `/* OP_GT P1 P2 P3` |
|        - |  6872 | ` *` |
|        - |  6873 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6874 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6875 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6876 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6877 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6878 | ` *` |
|        - |  6879 | ` */` |
|        - |  6880 | `/* OP_GE P1 P2 P3` |
|        - |  6881 | ` *` |
|        - |  6882 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6883 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6884 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6885 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6886 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6887 | ` *` |
|        - |  6888 | ` */` |
|    55654 |  6889 | `case PH7_OP_GT:` |
|        - |  6890 | `case PH7_OP_GE: {` |
|   111310 |  6891 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6892 | `	/* Perform the comparison and act accordingly */` |
|        - |  6893 | `#ifdef UNTRUST` |
|        - |  6894 | `	if( pNos < pStack ){` |
|        - |  6895 | `		goto Abort;` |
|        - |  6896 | `	}` |
|        - |  6897 | `#endif` |
|   111310 |  6898 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111310 |  6899 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6900 | `		rc = 0;` |
|   111306 |  6901 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110878 |  6902 | `		rc = rc >= 0;` |
|    55440 |  6903 | `	}else{` |
|      426 |  6904 | `		rc = rc > 0;` |
|        - |  6905 | `	}` |
|   111310 |  6906 | `	VmPopOperand(&pTos,1);` |
|   111310 |  6907 | `	if( !pInstr->iP2 ){` |
|        - |  6908 | `		/* Push comparison result without taking the jump */` |
|   111310 |  6909 | `		PH7_MemObjRelease(pTos);` |
|   111310 |  6910 | `		pTos->x.iVal = rc;` |
|        - |  6911 | `		/* Invalidate any prior representation */` |
|   111310 |  6912 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55656 |  6913 | `	}else{` |
|      ! 0 |  6914 | `		if( rc ){` |
|        - |  6915 | `			/* Jump to the desired location */` |
|      ! 0 |  6916 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6917 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6918 | `		}` |
|        - |  6919 | `	}` |
|   111310 |  6920 | `	break;` |
|        - |  6921 | `				}` |
|        - |  6922 | `/* OP_SPACESHIP * * *` |
|        - |  6923 | ` *` |
|        - |  6924 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6925 | ` *   -1 if left < right` |
|        - |  6926 | ` *    0 if left == right` |
|        - |  6927 | ` *    1 if left > right` |
|        - |  6928 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6929 | ` */` |
|       25 |  6930 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6931 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6932 | `#ifdef UNTRUST` |
|        - |  6933 | `	if( pNos < pStack ){` |
|        - |  6934 | `		goto Abort;` |
|        - |  6935 | `	}` |
|        - |  6936 | `#endif` |
|       51 |  6937 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6938 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6939 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6940 | `		rc = 1;` |
|        4 |  6941 | `	}else{` |
|        - |  6942 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6943 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6944 | `	}` |
|       51 |  6945 | `	VmPopOperand(&pTos,1);` |
|       51 |  6946 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6947 | `	pTos->x.iVal = rc;` |
|       51 |  6948 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6949 | `	break;` |
|        - |  6950 | `				}` |
|        - |  6951 | `/* OP_SEQ P1 P2 *` |
|        - |  6952 | ` * Strict string comparison.` |
|        - |  6953 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6954 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6955 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6956 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6957 | ` * use PH7_OP_EQ.` |
|        - |  6958 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6959 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6960 | ` */` |
|        - |  6961 | `/* OP_SNE P1 P2 *` |
|        - |  6962 | ` * Strict string comparison.` |
|        - |  6963 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6964 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6965 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6966 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6967 | ` * use PH7_OP_EQ.` |
|        - |  6968 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6969 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6970 | ` */` |
|       18 |  6971 | `case PH7_OP_SEQ:` |
|        - |  6972 | `case PH7_OP_SNE: {` |
|       38 |  6973 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6974 | `	SyString s1,s2;` |
|        - |  6975 | `	/* Perform the comparison and act accordingly */` |
|        - |  6976 | `#ifdef UNTRUST` |
|        - |  6977 | `	if( pNos < pStack ){` |
|        - |  6978 | `		goto Abort;` |
|        - |  6979 | `	}` |
|        - |  6980 | `#endif` |
|        - |  6981 | `	/* Force a string cast */` |
|       38 |  6982 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6983 | `		PH7_MemObjToString(pTos);` |
|        2 |  6984 | `	}` |
|       38 |  6985 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6986 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6987 | `	}` |
|       38 |  6988 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6989 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6990 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6991 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6992 | `		rc = rc != 0;` |
|      ! 0 |  6993 | `	}else{` |
|       38 |  6994 | `		rc = rc == 0;` |
|        - |  6995 | `	}` |
|       38 |  6996 | `	VmPopOperand(&pTos,1);` |
|       38 |  6997 | `	if( !pInstr->iP2 ){` |
|        - |  6998 | `		/* Push comparison result without taking the jump */` |
|       38 |  6999 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7000 | `		pTos->x.iVal = rc;` |
|        - |  7001 | `		/* Invalidate any prior representation */` |
|       38 |  7002 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7003 | `	}else{` |
|      ! 0 |  7004 | `		if( rc ){` |
|        - |  7005 | `			/* Jump to the desired location */` |
|      ! 0 |  7006 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7007 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7008 | `		}` |
|        - |  7009 | `	}` |
|       38 |  7010 | `	break;` |
|        - |  7011 | `				 }` |
|        - |  7012 | `/*` |
|        - |  7013 | ` * OP_LOAD_REF * * *` |
|        - |  7014 | ` * Push the index of a referenced object on the stack.` |
|        - |  7015 | ` */` |
|       60 |  7016 | `case PH7_OP_LOAD_REF: {` |
|        - |  7017 | `	sxu32 nIdx;` |
|        - |  7018 | `#ifdef UNTRUST` |
|        - |  7019 | `	if( pTos < pStack ){` |
|        - |  7020 | `		goto Abort;` |
|        - |  7021 | `	}` |
|        - |  7022 | `#endif` |
|        - |  7023 | `	/* Extract memory object index */` |
|      121 |  7024 | `	nIdx = pTos->nIdx;` |
|      121 |  7025 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7026 | `		/* Nullify the object */` |
|      101 |  7027 | `		PH7_MemObjRelease(pTos);` |
|        - |  7028 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7029 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7030 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7031 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7032 | `	}` |
|      121 |  7033 | `	break;` |
|        - |  7034 | `					  }` |
|        - |  7035 | `/*` |
|        - |  7036 | ` * OP_STORE_REF * * P3` |
|        - |  7037 | ` * Perform an assignment operation by reference.` |
|        - |  7038 | ` */` |
|       16 |  7039 | ` case PH7_OP_STORE_REF: {` |
|       34 |  7040 | `	 SyString sName = { 0 , 0 };` |
|        - |  7041 | `	 VmFrame *pFrameLocal;` |
|        - |  7042 | `	SyHashEntry *pEntry;` |
|        - |  7043 | `	sxu32 nIdx;` |
|        - |  7044 | `#ifdef UNTRUST` |
|        - |  7045 | `	if( pTos < pStack ){` |
|        - |  7046 | `		goto Abort;` |
|        - |  7047 | `	}` |
|        - |  7048 | `#endif` |
|       34 |  7049 | `	if( pInstr->p3 == 0 ){` |
|        - |  7050 | `		char *zName;` |
|        - |  7051 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7052 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7053 | `			/* Force a string cast */` |
|      ! 0 |  7054 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7055 | `		}` |
|      ! 0 |  7056 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7057 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7058 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7059 | `			if( zName ){` |
|      ! 0 |  7060 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7061 | `			}` |
|      ! 0 |  7062 | `		}` |
|      ! 0 |  7063 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7064 | `		pTos--;` |
|      ! 0 |  7065 | `	}else{` |
|       34 |  7066 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7067 | `	}` |
|       34 |  7068 | `	nIdx = pTos->nIdx;` |
|       34 |  7069 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7070 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7071 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7072 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7073 | `		}else{` |
|        - |  7074 | `			ph7_value *pObj;` |
|        - |  7075 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7076 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7077 | `			if( pObj == 0 ){` |
|      ! 0 |  7078 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7079 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7080 | `				goto Abort;` |
|        - |  7081 | `			}` |
|        - |  7082 | `			/* Perform the store operation */` |
|      ! 0 |  7083 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7084 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7085 | `		}` |
|       34 |  7086 | `	}else if( sName.nByte > 0){` |
|       34 |  7087 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7088 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7089 | `		}else{` |
|       34 |  7090 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7091 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7092 | `			/* Query the local frame */` |
|       34 |  7093 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7094 | `			if( pEntry ){` |
|      ! 0 |  7095 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7096 | `			}else{` |
|       34 |  7097 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7098 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7099 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7100 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7101 | `				}` |
|       34 |  7102 | `				if( rc == SXRET_OK ){` |
|       34 |  7103 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7104 | `				}` |
|        - |  7105 | `			}` |
|        - |  7106 | `		}` |
|       16 |  7107 | `	}` |
|       34 |  7108 | `	break;` |
|        - |  7109 | `				 }` |
|        - |  7110 | `/*` |
|        - |  7111 | ` * OP_UPLINK P1 * *` |
|        - |  7112 | ` * Link a variable to the top active VM frame.` |
|        - |  7113 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7114 | ` */` |
|       30 |  7115 | `case PH7_OP_UPLINK: {` |
|       62 |  7116 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7117 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7118 | `		SyString sName;` |
|        - |  7119 | `		/* Perform the link */` |
|      132 |  7120 | `		while( pLink <= pTos ){` |
|       72 |  7121 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7122 | `				/* Force a string cast */` |
|      ! 0 |  7123 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7124 | `			}` |
|       72 |  7125 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7126 | `			if( sName.nByte > 0 ){` |
|       72 |  7127 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7128 | `			}` |
|       72 |  7129 | `			pLink++;` |
|        2 |  7130 | `		}` |
|       30 |  7131 | `	}` |
|       62 |  7132 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7133 | `	break;` |
|        - |  7134 | `					}` |
|        - |  7135 | `/*` |
|        - |  7136 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7137 | ` * Push an exception in the corresponding container so that` |
|        - |  7138 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7139 | ` */` |
|      178 |  7140 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      358 |  7141 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7142 | `	VmFrame *pFrameLocal;` |
|        - |  7143 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      358 |  7144 | `	pException->iFinallyDone = 0;` |
|      358 |  7145 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7146 | `	/* Create the exception frame */` |
|      358 |  7147 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      358 |  7148 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7149 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7150 | `		goto Abort;` |
|        - |  7151 | `	}` |
|        - |  7152 | `	/* Mark the special frame */` |
|      358 |  7153 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      358 |  7154 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7155 | `	/* Point to the frame that trigger the exception */` |
|      358 |  7156 | `	pFrameLocal = pFrameLocal->pParent;` |
|      358 |  7157 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      358 |  7158 | `	pException->pFrame = pFrameLocal;` |
|      358 |  7159 | `	break;` |
|        - |  7160 | `							}` |
|        - |  7161 | `/*` |
|        - |  7162 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7163 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7164 | ` */` |
|      177 |  7165 | `case PH7_OP_POP_EXCEPTION: {` |
|      356 |  7166 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      356 |  7167 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7168 | `		ph7_exception **apException;` |
|        - |  7169 | `		/* Pop the loaded exception */` |
|       32 |  7170 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7171 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7172 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7173 | `		}` |
|       15 |  7174 | `	}` |
|      356 |  7175 | `	pException->pFrame = 0;` |
|        - |  7176 | `	/* Leave the exception frame */` |
|      356 |  7177 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7178 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      356 |  7179 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7180 | `		sxi32 rcFinally;` |
|       20 |  7181 | `		pException->iFinallyDone = 1;` |
|       20 |  7182 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7183 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7184 | `			goto Abort;` |
|        - |  7185 | `		}` |
|        9 |  7186 | `	}` |
|      356 |  7187 | `	break;` |
|        - |  7188 | `							}` |
|        - |  7189 |  |
|        - |  7190 | `/*` |
|        - |  7191 | ` * OP_THROW * P2 *` |
|        - |  7192 | ` * Throw an user exception.` |
|        - |  7193 | ` */` |
|       73 |  7194 | `case PH7_OP_THROW: {` |
|      148 |  7195 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      148 |  7196 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7197 | `#ifdef UNTRUST` |
|        - |  7198 | `	if( pTos < pStack ){` |
|        - |  7199 | `		goto Abort;` |
|        - |  7200 | `	}` |
|        - |  7201 | `#endif` |
|      148 |  7202 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7203 | `	/* Tell the upper layer that an exception was thrown */` |
|      148 |  7204 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      148 |  7205 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      148 |  7206 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7207 | `		ph7_class *pThrowable;` |
|        - |  7208 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      148 |  7209 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      149 |  7210 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7211 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7212 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7213 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7214 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7215 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7216 | `			if( pErrorClass ){` |
|        3 |  7217 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7218 | `			}` |
|        3 |  7219 | `			if( pErrInst ){` |
|        - |  7220 | `				ph7_class_method *pCons;` |
|        3 |  7221 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7222 | `				if( pCons ){` |
|        - |  7223 | `					ph7_value sArg;` |
|        - |  7224 | `					ph7_value *apArg[1];` |
|        - |  7225 | `					SyString sMsgStr;` |
|        - |  7226 | `					static const char zErrMsg[] =` |
|        - |  7227 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7228 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7229 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7230 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7231 | `					apArg[0] = &sArg;` |
|        3 |  7232 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7233 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7234 | `				}` |
|        3 |  7235 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7236 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7237 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7238 | `					goto Abort;` |
|        - |  7239 | `				}` |
|        2 |  7240 | `			}else{` |
|        - |  7241 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7242 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7243 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7244 | `					goto Abort;` |
|        - |  7245 | `				}` |
|        - |  7246 | `			}` |
|        2 |  7247 | `		}else{` |
|        - |  7248 | `			/* Throw the exception */` |
|      146 |  7249 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      146 |  7250 | `			if( rc == SXERR_ABORT ){` |
|        - |  7251 | `				/* Abort processing immediately */` |
|       11 |  7252 | `				goto Abort;` |
|        - |  7253 | `			}` |
|        - |  7254 | `		}` |
|       70 |  7255 | `	}else{` |
|        - |  7256 | `		/* Expecting a class instance */` |
|      ! 0 |  7257 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7258 | `		if( rc == SXERR_ABORT ){` |
|        - |  7259 | `			/* Abort processing immediately */` |
|      ! 0 |  7260 | `			goto Abort;` |
|        - |  7261 | `		}` |
|        - |  7262 | `	}` |
|        - |  7263 | `	/* Pop the top entry */` |
|      138 |  7264 | `	VmPopOperand(&pTos,1);` |
|        - |  7265 | `	/* Perform an unconditional jump */` |
|      138 |  7266 | `	pc = nJump - 1;` |
|      138 |  7267 | `	break;` |
|        - |  7268 | `				   }` |
|        - |  7269 | `/*` |
|        - |  7270 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7271 | ` * Prepare a foreach step.` |
|        - |  7272 | ` */` |
|     6136 |  7273 | `case PH7_OP_FOREACH_INIT: {` |
|    12274 |  7274 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7275 | `	void *pName;` |
|        - |  7276 | `#ifdef UNTRUST` |
|        - |  7277 | `	if( pTos < pStack ){` |
|        - |  7278 | `		goto Abort;` |
|        - |  7279 | `	}` |
|        - |  7280 | `#endif` |
|    12274 |  7281 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7282 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7283 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7284 | `			/* Force a string cast */` |
|      ! 0 |  7285 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7286 | `		}` |
|        - |  7287 | `		/* Duplicate name */` |
|      ! 0 |  7288 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7289 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7290 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7291 | `		}` |
|      ! 0 |  7292 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7293 | `	}` |
|    12274 |  7294 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7295 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7296 | `			/* Force a string cast */` |
|      ! 0 |  7297 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7298 | `		}` |
|        - |  7299 | `		/* Duplicate name */` |
|      ! 0 |  7300 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7301 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7302 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7303 | `		}` |
|      ! 0 |  7304 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7305 | `	}` |
|        - |  7306 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12274 |  7307 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7308 | `		/* Jump out of the loop */` |
|      ! 0 |  7309 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7310 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7311 | `		}` |
|      ! 0 |  7312 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7313 | `	}else{` |
|        - |  7314 | `		ph7_foreach_step *pStep;` |
|    12274 |  7315 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12274 |  7316 | `		if( pStep == 0 ){` |
|      ! 0 |  7317 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7318 | `			/* Jump out of the loop */` |
|      ! 0 |  7319 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7320 | `		}else{` |
|        - |  7321 | `			/* Zero the structure */` |
|    12274 |  7322 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7323 | `			/* Prepare the step */` |
|    12274 |  7324 | `			pStep->iFlags = pInfo->iFlags;` |
|    12274 |  7325 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7326 | `				ph7_hashmap *pMap;` |
|        - |  7327 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7328 | `				 * source array so mutations don't affect other sharers. */` |
|    12240 |  7329 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7330 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7331 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7332 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7333 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7334 | `						 * variable still points at the same hashmap as` |
|        - |  7335 | `						 * the stack value. */` |
|        9 |  7336 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7337 | `							pCur->iRef--;` |
|        9 |  7338 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7339 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7340 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7341 | `						}` |
|        4 |  7342 | `					}` |
|        4 |  7343 | `				}` |
|    12240 |  7344 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7345 | `				/* Reset the internal loop cursor */` |
|    12240 |  7346 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7347 | `				/* Mark the step */` |
|    12240 |  7348 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12240 |  7349 | `				pStep->xIter.pMap = pMap;` |
|    12240 |  7350 | `				pMap->iRef++;` |
|     6121 |  7351 | `			}else{` |
|       36 |  7352 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7353 | `				ph7_class *pIteratorClass;` |
|        - |  7354 | `				/* Check if the object implements Iterator */` |
|       36 |  7355 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7356 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7357 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7358 | `					ph7_class_method *pRewind;` |
|       24 |  7359 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7360 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7361 | `					pThis->iRef++;` |
|       24 |  7362 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7363 | `					if( pRewind ){` |
|       24 |  7364 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7365 | `					}` |
|       13 |  7366 | `				}else{` |
|        - |  7367 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7368 | `					ph7_class *pIterAggClass;` |
|       14 |  7369 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7370 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7371 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7372 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7373 | `						ph7_class_method *pGetIter;` |
|        3 |  7374 | `						int iterAggOk = 0;` |
|        3 |  7375 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7376 | `						if( pGetIter ){` |
|        - |  7377 | `							ph7_value sResult;` |
|        3 |  7378 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7379 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7380 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7381 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7382 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7383 | `									ph7_class_method *pRewind;` |
|        3 |  7384 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7385 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7386 | `									pIterObj->iRef++;` |
|        - |  7387 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7388 | `									pStep->pOwner = pThis;` |
|        3 |  7389 | `									pThis->iRef++;` |
|        3 |  7390 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7391 | `									if( pRewind ){` |
|        3 |  7392 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7393 | `									}` |
|        3 |  7394 | `									iterAggOk = 1;` |
|        1 |  7395 | `								}` |
|        1 |  7396 | `							}` |
|        3 |  7397 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7398 | `						}` |
|        3 |  7399 | `						if( !iterAggOk ){` |
|        - |  7400 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7401 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7402 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7403 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7404 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7405 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7406 | `						}` |
|        2 |  7407 | `					}else{` |
|        - |  7408 | `						/* Plain object iteration via hAttr */` |
|       12 |  7409 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7410 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7411 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7412 | `						pThis->iRef++;` |
|        - |  7413 | `					}` |
|        - |  7414 | `				}` |
|        - |  7415 | `			}` |
|        - |  7416 | `		}` |
|    12274 |  7417 | `		if( pStep ){` |
|    12274 |  7418 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7419 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7420 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7421 | `				/* Jump out of the loop */` |
|      ! 0 |  7422 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7423 | `			}` |
|     6136 |  7424 | `		}` |
|        - |  7425 | `	}` |
|    12274 |  7426 | `	VmPopOperand(&pTos,1);` |
|    12274 |  7427 | `	break;` |
|        - |  7428 | `						  }` |
|        - |  7429 | `/*` |
|        - |  7430 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7431 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7432 | ` */` |
|   100644 |  7433 | `case PH7_OP_FOREACH_STEP: {` |
|   201290 |  7434 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7435 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7436 | `	ph7_value *pValue;` |
|        - |  7437 | `	VmFrame *pFrameLocal;` |
|        - |  7438 | `	/* Peek the last step */` |
|   201290 |  7439 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   201290 |  7440 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   201290 |  7441 | `	pFrameLocal = pVm->pFrame;` |
|   201290 |  7442 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   201290 |  7443 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   201156 |  7444 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7445 | `		ph7_hashmap_node *pNode;` |
|        - |  7446 | `		/* Extract the current node value */` |
|   201156 |  7447 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   201156 |  7448 | `		if( pNode == 0 ){` |
|        - |  7449 | `			/* No more entry to process */` |
|    12238 |  7450 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12238 |  7451 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7452 | `				/* Break the reference with the last element */` |
|        7 |  7453 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7454 | `			}` |
|        - |  7455 | `			/* Automatically reset the loop cursor */` |
|    12238 |  7456 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7457 | `			/* Cleanup the mess left behind */` |
|    12238 |  7458 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12238 |  7459 | `			SySetPop(&pInfo->aStep);` |
|    12238 |  7460 | `			PH7_HashmapUnref(pMap);` |
|     6120 |  7461 | `		}else{` |
|   188920 |  7462 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7463 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7464 | `				if( pKey ){` |
|      528 |  7465 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7466 | `				}` |
|      263 |  7467 | `			}` |
|   188920 |  7468 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7469 | `				SyHashEntry *pEntry;` |
|        - |  7470 | `				/* Pass by reference */` |
|       23 |  7471 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7472 | `				if( pEntry ){` |
|       21 |  7473 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7474 | `				}else{` |
|        4 |  7475 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7476 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7477 | `				}` |
|       12 |  7478 | `			}else{` |
|        - |  7479 | `				/* Make a copy of the entry value */` |
|   188898 |  7480 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   188898 |  7481 | `				if( pValue ){` |
|   188898 |  7482 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    94448 |  7483 | `				}` |
|        - |  7484 | `			}` |
|        2 |  7485 | `		}` |
|   100713 |  7486 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7487 | `		/* Iterator-based iteration.` |
|        - |  7488 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7489 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7490 | `		 */` |
|      106 |  7491 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7492 | `		ph7_class_method *pMethod;` |
|        - |  7493 | `		ph7_value sResult;` |
|      106 |  7494 | `		int isValid = 0;` |
|        - |  7495 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7496 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7497 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7498 | `		}else{` |
|       82 |  7499 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7500 | `			if( pMethod ){` |
|       82 |  7501 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7502 | `			}` |
|        - |  7503 | `		}` |
|        - |  7504 | `		/* Call valid() */` |
|      106 |  7505 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7506 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7507 | `		if( pMethod ){` |
|      106 |  7508 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7509 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7510 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7511 | `		}` |
|      106 |  7512 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7513 | `		if( !isValid ){` |
|        - |  7514 | `			/* Iterator exhausted */` |
|       24 |  7515 | `			pc = pInstr->iP2 - 1;` |
|        - |  7516 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7517 | `			if( pStep->pOwner ){` |
|        3 |  7518 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7519 | `			}` |
|       24 |  7520 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7521 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7522 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7523 | `		}else{` |
|        - |  7524 | `			/* Call current() to get value */` |
|       84 |  7525 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7526 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7527 | `			if( pMethod ){` |
|       84 |  7528 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7529 | `			}` |
|       84 |  7530 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7531 | `			if( pValue ){` |
|       84 |  7532 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7533 | `			}` |
|       84 |  7534 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7535 | `			/* Call key() if needed */` |
|       84 |  7536 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7537 | `				ph7_value sKey;` |
|       35 |  7538 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7539 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7540 | `				if( pMethod ){` |
|       35 |  7541 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7542 | `				}` |
|       35 |  7543 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7544 | `				if( pValue ){` |
|       35 |  7545 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7546 | `				}` |
|       35 |  7547 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7548 | `			}` |
|        - |  7549 | `		}` |
|       54 |  7550 | `	}else{` |
|       32 |  7551 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7552 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7553 | `		SyHashEntry *pEntry;` |
|        - |  7554 | `		/* Point to the next attribute */` |
|       36 |  7555 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7556 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7557 | `			/* Check access permission */` |
|       38 |  7558 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7559 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7560 | `					break; /* Access is granted */` |
|        - |  7561 | `			}` |
|        1 |  7562 | `		}` |
|       32 |  7563 | `		if( pEntry == 0 ){` |
|        - |  7564 | `			/* Clean up the mess left behind */` |
|       12 |  7565 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7566 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7567 | `				/* Break the reference with the last element */` |
|        3 |  7568 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7569 | `			}` |
|       12 |  7570 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7571 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7572 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7573 | `		}else{` |
|       22 |  7574 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7575 | `			ph7_value *pAttrValue;` |
|       22 |  7576 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7577 | `				/* Fill with the current attribute name */` |
|       22 |  7578 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7579 | `				if( pKey ){` |
|       22 |  7580 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7581 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7582 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7583 | `				}` |
|       10 |  7584 | `			}` |
|        - |  7585 | `			/* Extract attribute value */` |
|       22 |  7586 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7587 | `			if( pAttrValue ){` |
|       22 |  7588 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7589 | `					/* Pass by reference */` |
|        3 |  7590 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7591 | `					if( pEntry ){` |
|        3 |  7592 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7593 | `					}else{` |
|      ! 0 |  7594 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7595 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7596 | `					}` |
|        2 |  7597 | `				}else{` |
|        - |  7598 | `					/* Make a copy of the attribute value */` |
|       20 |  7599 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7600 | `					if( pValue ){` |
|       20 |  7601 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7602 | `					}` |
|        - |  7603 | `				}` |
|       10 |  7604 | `			}` |
|        - |  7605 | `		}` |
|        - |  7606 | `	}` |
|   201290 |  7607 | `	break;` |
|        - |  7608 | `						  }` |
|        - |  7609 | `/*` |
|        - |  7610 | ` * OP_MEMBER P1 P2` |
|        - |  7611 | ` * Load class attribute/method on the stack.` |
|        - |  7612 | ` */` |
|     3952 |  7613 | `case PH7_OP_MEMBER: {` |
|        - |  7614 | `	ph7_class_instance *pThis;` |
|        - |  7615 | `	ph7_value *pNos;` |
|        - |  7616 | `	SyString sName;` |
|     7906 |  7617 | `	if( !pInstr->iP1 ){` |
|     7678 |  7618 | `		pNos = &pTos[-1];` |
|        - |  7619 | `#ifdef UNTRUST` |
|        - |  7620 | `		if( pNos < pStack ){` |
|        - |  7621 | `			goto Abort;` |
|        - |  7622 | `		}` |
|        - |  7623 | `#endif` |
|     7678 |  7624 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7625 | `			ph7_class *pClass;` |
|        - |  7626 | `			/* Class already instantiated */` |
|     7676 |  7627 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7628 | `			/* Point to the instantiated class */` |
|     7676 |  7629 | `			pClass = pThis->pClass;` |
|        - |  7630 | `			/* Extract attribute name first */` |
|     7676 |  7631 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7676 |  7632 | `			if( pInstr->iP2 ){` |
|        - |  7633 | `				/* Method call */` |
|      778 |  7634 | `				ph7_class_method *pMeth = 0;` |
|      778 |  7635 | `				if( sName.nByte > 0 ){` |
|        - |  7636 | `					/* Extract the target method */` |
|      778 |  7637 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      388 |  7638 | `				}` |
|      778 |  7639 | `				if( pMeth == 0 ){` |
|      ! 0 |  7640 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7641 | `						&pClass->sName,&sName` |
|        - |  7642 | `						);` |
|        - |  7643 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7644 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7645 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7646 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7647 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7648 | `				}else{` |
|        - |  7649 | `					/* Push method name on the stack */` |
|      778 |  7650 | `					PH7_MemObjRelease(pTos);` |
|      778 |  7651 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      778 |  7652 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7653 | `				}` |
|      778 |  7654 | `				pTos->nIdx = SXU32_HIGH;` |
|      390 |  7655 | `			}else{` |
|        - |  7656 | `				/* Attribute access */` |
|     6900 |  7657 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7658 | `				SyHashEntry *pEntry;` |
|        - |  7659 | `				/* Extract the target attribute */` |
|     6900 |  7660 | `				if( sName.nByte > 0 ){` |
|     6900 |  7661 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6900 |  7662 | `					if( pEntry ){` |
|        - |  7663 | `						/* Point to the attribute value */` |
|     6898 |  7664 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3448 |  7665 | `					}` |
|     3449 |  7666 | `				}` |
|     6900 |  7667 | `				if( pObjAttr == 0 ){` |
|        - |  7668 | `					/* No such attribute,load null */` |
|        4 |  7669 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7670 | `						&pClass->sName,&sName);` |
|        - |  7671 | `					/* Call the __get magic method if available */` |
|        3 |  7672 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7673 | `				}` |
|     6900 |  7674 | `				VmPopOperand(&pTos,1);` |
|        - |  7675 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7676 | `				 * This is due to the following case:` |
|        - |  7677 | `				 *     (new TestClass())->foo;` |
|        - |  7678 | `				 */` |
|     6900 |  7679 | `				pThis->iRef++;` |
|     6900 |  7680 | `				PH7_MemObjRelease(pTos);` |
|     6900 |  7681 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6900 |  7682 | `				if( pObjAttr ){` |
|     6898 |  7683 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7684 | `					/* Check attribute access */` |
|     6898 |  7685 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7686 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7687 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7688 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7689 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7690 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6896 |  7691 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3490 |  7692 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  7693 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  7694 | `							int bIsLhs = 0;` |
|       82 |  7695 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  7696 | `								bIsLhs = 1;` |
|       39 |  7697 | `							}` |
|       82 |  7698 | `							if( !bIsLhs ){` |
|        3 |  7699 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7700 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7701 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7702 | `									goto Abort;` |
|        - |  7703 | `								}` |
|        - |  7704 | `								{` |
|        3 |  7705 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7706 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7707 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3952 |  7708 | `										break;` |
|        - |  7709 | `									}` |
|        - |  7710 | `								}` |
|      ! 0 |  7711 | `								goto Exception;` |
|        - |  7712 | `							}` |
|       39 |  7713 | `						}` |
|        - |  7714 | `						/* Load attribute */` |
|     6896 |  7715 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6896 |  7716 | `						if( pValue ){` |
|     6896 |  7717 | `							if( pThis->iRef < 2 ){` |
|        - |  7718 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7719 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7720 | `								 */` |
|        7 |  7721 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7722 | `							}else{` |
|        - |  7723 | `								/* Simple load */` |
|     6890 |  7724 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7725 | `							}` |
|     6896 |  7726 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6894 |  7727 | `								if( pThis->iRef > 1 ){` |
|        - |  7728 | `									/* Load attribute index */` |
|     6888 |  7729 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3443 |  7730 | `								}` |
|     3446 |  7731 | `							}` |
|     3447 |  7732 | `						}` |
|     3449 |  7733 | `					}else{` |
|        - |  7734 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7735 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7736 | `						char zMsg[256];` |
|      ! 0 |  7737 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7738 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7739 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7740 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7741 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7742 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7743 | `						goto Abort;` |
|        - |  7744 | `					}` |
|     3447 |  7745 | `				}` |
|        - |  7746 | `				/* Safely unreference the object */` |
|     6898 |  7747 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7748 | `			}` |
|     3838 |  7749 | `		}else{` |
|        3 |  7750 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7751 | `			VmPopOperand(&pTos,1);` |
|        3 |  7752 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7753 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7754 | `		}` |
|     3839 |  7755 | `	}else{` |
|        - |  7756 | `		/* Static member access using class name */` |
|      230 |  7757 | `		pNos = pTos;` |
|      230 |  7758 | `		pThis = 0;` |
|      230 |  7759 | `		if( !pInstr->p3 ){` |
|      192 |  7760 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  7761 | `			pNos--;` |
|        - |  7762 | `#ifdef UNTRUST` |
|        - |  7763 | `			if( pNos < pStack ){` |
|        - |  7764 | `				goto Abort;` |
|        - |  7765 | `			}` |
|        - |  7766 | `#endif` |
|       97 |  7767 | `		}else{` |
|        - |  7768 | `			/* Attribute name already computed */` |
|       40 |  7769 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7770 | `		}` |
|      230 |  7771 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      230 |  7772 | `			ph7_class *pClass = 0;` |
|      230 |  7773 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7774 | `				/* Class already instantiated */` |
|        5 |  7775 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7776 | `				pClass = pThis->pClass;` |
|        5 |  7777 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7778 | `			}else{` |
|        - |  7779 | `				/* Try to extract the target class */` |
|      226 |  7780 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      226 |  7781 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      226 |  7782 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7783 | `					/* Handle self/static/parent keywords */` |
|      226 |  7784 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7785 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7786 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7787 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7788 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7789 | `						}` |
|      196 |  7790 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7791 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      166 |  7792 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7793 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7794 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7795 | `							pClass = pSelf->pBase;` |
|       13 |  7796 | `						}` |
|       15 |  7797 | `					}else{` |
|      114 |  7798 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7799 | `					}` |
|      112 |  7800 | `				}` |
|        - |  7801 | `			}` |
|      230 |  7802 | `			if( pClass == 0 ){` |
|        - |  7803 | `				/* Undefined class */` |
|      ! 0 |  7804 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7805 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7806 | `					);` |
|      ! 0 |  7807 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7808 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7809 | `				}` |
|      ! 0 |  7810 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7811 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7812 | `			}else{` |
|      230 |  7813 | `				if( pInstr->iP2 ){` |
|        - |  7814 | `					/* Method call */` |
|       86 |  7815 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7816 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7817 | `						/* Extract the target method */` |
|       86 |  7818 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7819 | `					}` |
|       86 |  7820 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7821 | `						if( pMeth ){` |
|      ! 0 |  7822 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7823 | `								&pClass->sName,&sName` |
|        - |  7824 | `								);` |
|      ! 0 |  7825 | `						}else{` |
|      ! 0 |  7826 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7827 | `								&pClass->sName,&sName` |
|        - |  7828 | `								);` |
|        - |  7829 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7830 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7831 | `						}` |
|        - |  7832 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7833 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7834 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7835 | `						}` |
|      ! 0 |  7836 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7837 | `					}else{` |
|        - |  7838 | `						/* Push method name on the stack */` |
|       86 |  7839 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7840 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7841 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7842 | `					}` |
|       86 |  7843 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7844 | `				}else{` |
|        - |  7845 | `					/* Attribute access */` |
|      146 |  7846 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7847 | `					/* Check for special ::class pseudo-constant */` |
|      192 |  7848 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7849 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7850 | `						/* ::class returns the fully qualified class name */` |
|        - |  7851 | `						/* Pop the attribute name from the stack */` |
|       60 |  7852 | `						if( !pInstr->p3 ){` |
|       60 |  7853 | `							VmPopOperand(&pTos,1);` |
|       29 |  7854 | `						}` |
|       60 |  7855 | `						PH7_MemObjRelease(pTos);` |
|        - |  7856 | `						/* Load the class name */` |
|       60 |  7857 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7858 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7859 | `					}else{` |
|        - |  7860 | `						/* Extract the target attribute */` |
|       88 |  7861 | `						if( sName.nByte > 0 ){` |
|       88 |  7862 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       43 |  7863 | `						}` |
|       88 |  7864 | `						if( pAttr == 0 ){` |
|        - |  7865 | `							/* No such attribute,load null */` |
|      ! 0 |  7866 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7867 | `								&pClass->sName,&sName);` |
|        - |  7868 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7869 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7870 | `						}` |
|        - |  7871 | `						/* Pop the attribute name from the stack */` |
|       88 |  7872 | `						if( !pInstr->p3 ){` |
|       50 |  7873 | `							VmPopOperand(&pTos,1);` |
|       24 |  7874 | `						}` |
|       88 |  7875 | `						PH7_MemObjRelease(pTos);` |
|       88 |  7876 | `						pTos->nIdx = SXU32_HIGH;` |
|       88 |  7877 | `						if( pAttr ){` |
|       88 |  7878 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7879 | `								/* Access to a non static attribute */` |
|      ! 0 |  7880 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7881 | `									&pClass->sName,&pAttr->sName` |
|        - |  7882 | `									);` |
|      ! 0 |  7883 | `							}else{` |
|        - |  7884 | `								ph7_value *pValue;` |
|        - |  7885 | `								/* Check if the access to the attribute is allowed */` |
|       88 |  7886 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7887 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7888 | `									 * Same LHS-of-store peek as the instance path. */` |
|       82 |  7889 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       56 |  7890 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7891 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7892 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7893 | `										if( pS ){` |
|       28 |  7894 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7895 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7896 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7897 | `												int bIsLhs = 0;` |
|        8 |  7898 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7899 | `													bIsLhs = 1;` |
|        2 |  7900 | `												}` |
|        8 |  7901 | `												if( !bIsLhs ){` |
|        3 |  7902 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7903 | `													if( pThis ){` |
|      ! 0 |  7904 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7905 | `													}` |
|        3 |  7906 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7907 | `														goto Abort;` |
|        - |  7908 | `													}` |
|        - |  7909 | `													{` |
|        3 |  7910 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7911 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7912 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7913 | `															break;` |
|        - |  7914 | `														}` |
|        - |  7915 | `													}` |
|      ! 0 |  7916 | `													goto Exception;` |
|        - |  7917 | `												}` |
|        2 |  7918 | `											}` |
|       12 |  7919 | `										}` |
|       12 |  7920 | `									}` |
|        - |  7921 | `									/* Load the desired attribute */` |
|       82 |  7922 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       82 |  7923 | `									if( pValue ){` |
|       82 |  7924 | `										PH7_MemObjLoad(pValue,pTos);` |
|       82 |  7925 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7926 | `											/* Load index number */` |
|       38 |  7927 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7928 | `										}` |
|       40 |  7929 | `									}` |
|       42 |  7930 | `								}else{` |
|        - |  7931 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7932 | `									char zMsg[256];` |
|        5 |  7933 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7934 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7935 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7936 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7937 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7938 | `									}else{` |
|      ! 0 |  7939 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7940 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7941 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7942 | `									}` |
|        5 |  7943 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7944 | `									goto Abort;` |
|        - |  7945 | `								}` |
|        - |  7946 | `							}` |
|       40 |  7947 | `						}` |
|        - |  7948 | `					}` |
|        - |  7949 | `				}` |
|      224 |  7950 | `				if( pThis ){` |
|        - |  7951 | `					/* Safely unreference the object */` |
|        5 |  7952 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7953 | `				}` |
|        - |  7954 | `			}` |
|      113 |  7955 | `		}else{` |
|        - |  7956 | `			/* Pop operands */` |
|      ! 0 |  7957 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7958 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7959 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7960 | `			}` |
|      ! 0 |  7961 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7962 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7963 | `		}` |
|        - |  7964 | `	}` |
|     7898 |  7965 | `	break;` |
|        - |  7966 | `					}` |
|        - |  7967 | `/*` |
|        - |  7968 | ` * OP_NEW P1 * * *` |
|        - |  7969 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7970 | ` */` |
|      637 |  7971 | `case PH7_OP_NEW: {` |
|     1276 |  7972 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1276 |  7973 | `	ph7_class *pClass = 0;` |
|        - |  7974 | `	ph7_class_instance *pNew;` |
|     1276 |  7975 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7976 | `		/* Try to extract the desired class */` |
|     1913 |  7977 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1274 |  7978 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      637 |  7979 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7980 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7981 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7982 | `	}` |
|     1276 |  7983 | `	if( pClass == 0 ){` |
|        - |  7984 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7985 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7986 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7987 | `			);` |
|        - |  7988 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7989 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7990 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7991 | `			/* Pop given arguments */` |
|      ! 0 |  7992 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7993 | `		}` |
|      ! 0 |  7994 | `		goto Abort;` |
|      ! 0 |  7995 | `	}else{` |
|        - |  7996 | `		ph7_class_method *pCons;` |
|        - |  7997 | `		/* Create a new class instance */` |
|     1276 |  7998 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1276 |  7999 | `		if( pNew == 0 ){` |
|      ! 0 |  8000 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8001 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8002 | `				&pClass->sName` |
|        - |  8003 | `			);` |
|      ! 0 |  8004 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8005 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8006 | `				/* Pop given arguments */` |
|      ! 0 |  8007 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8008 | `			}` |
|      ! 0 |  8009 | `			break;` |
|        - |  8010 | `		}` |
|        - |  8011 | `		/* Check if a constructor is available */` |
|     1276 |  8012 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1276 |  8013 | `		if( pCons == 0 ){` |
|      916 |  8014 | `			SyString *pName = &pClass->sName;` |
|        - |  8015 | `			/* Check for a constructor with the same base class name */` |
|      916 |  8016 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      457 |  8017 | `		}` |
|     1276 |  8018 | `		if( pCons ){` |
|        - |  8019 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8020 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8021 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8022 | `			 * (including variadic string-key packing). */` |
|      362 |  8023 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8024 | `			sxi32 rcCons;` |
|      362 |  8025 | `			SySetReset(&aArg);` |
|      714 |  8026 | `			while( pArg < pTos ){` |
|      354 |  8027 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      354 |  8028 | `				pArg++;` |
|        2 |  8029 | `			}` |
|      362 |  8030 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8031 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8032 | `				sxu32 n;` |
|      102 |  8033 | `				n = SySetUsed(&aArg);` |
|        - |  8034 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8035 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8036 | `				 * after resolution). */` |
|      202 |  8037 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      102 |  8038 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      102 |  8039 | `					if( pFuncArg ){` |
|      102 |  8040 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8041 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8042 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8043 | `						}` |
|       50 |  8044 | `					}` |
|      102 |  8045 | `					n++;` |
|        2 |  8046 | `				}` |
|       50 |  8047 | `			}` |
|      362 |  8048 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8049 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      362 |  8050 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8051 | `				pNew->iRef = 1;` |
|      ! 0 |  8052 | `			}` |
|      362 |  8053 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8054 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8055 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8056 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8057 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8058 | `				sxi32 iResumePc;` |
|        5 |  8059 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8060 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8061 | `					goto Abort;` |
|        - |  8062 | `				}` |
|        5 |  8063 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8064 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8065 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8066 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8067 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8068 | `					}` |
|        5 |  8069 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8070 | `					pc = iResumePc;` |
|        5 |  8071 | `					break;` |
|        - |  8072 | `				}` |
|      ! 0 |  8073 | `				goto Exception;` |
|        - |  8074 | `			}` |
|      178 |  8075 | `		}` |
|     1272 |  8076 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8077 | `			/* Pop given arguments */` |
|      294 |  8078 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      146 |  8079 | `		}` |
|     1272 |  8080 | `		PH7_MemObjRelease(pTos);` |
|     1272 |  8081 | `		pTos->x.pOther = pNew;` |
|     1272 |  8082 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8083 | `	}` |
|     1272 |  8084 | `	break;` |
|        - |  8085 | `				 }` |
|        - |  8086 | `/*` |
|        - |  8087 | ` * OP_CLONE * * *` |
|        - |  8088 | ` * Perfome a clone operation.` |
|        - |  8089 | ` */` |
|       24 |  8090 | `case PH7_OP_CLONE: {` |
|        - |  8091 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8092 | `#ifdef UNTRUST` |
|        - |  8093 | `	if( pTos < pStack ){` |
|        - |  8094 | `		goto Abort;` |
|        - |  8095 | `	}` |
|        - |  8096 | `#endif` |
|        - |  8097 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8098 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8099 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8100 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8101 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8102 | `		break;` |
|        - |  8103 | `	}` |
|        - |  8104 | `	/* Point to the source */` |
|       46 |  8105 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8106 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8107 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8108 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8109 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8110 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8111 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8112 | `		break;` |
|        - |  8113 | `	}` |
|        - |  8114 | `	/* Perform the clone operation */` |
|       46 |  8115 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8116 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8117 | `	if( pClone == 0 ){` |
|      ! 0 |  8118 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8119 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8120 | `	}else{` |
|        - |  8121 | `		/* Load the cloned object */` |
|       46 |  8122 | `		pTos->x.pOther = pClone;` |
|       46 |  8123 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8124 | `	}` |
|       46 |  8125 | `	break;` |
|        - |  8126 | `				   }` |
|        - |  8127 | `/*` |
|        - |  8128 | ` * OP_SWITCH * * P3` |
|        - |  8129 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8130 | ` */` |
|       26 |  8131 | `case PH7_OP_SWITCH: {` |
|       54 |  8132 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8133 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8134 | `	ph7_value sValue,sCaseValue;` |
|        - |  8135 | `	sxu32 n,nEntry;` |
|        - |  8136 | `#ifdef UNTRUST` |
|        - |  8137 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8138 | `		goto Abort;` |
|        - |  8139 | `	}` |
|        - |  8140 | `#endif` |
|        - |  8141 | `	/* Point to the case table  */` |
|       54 |  8142 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8143 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8144 | `	/* Select the appropriate case block to execute */` |
|       54 |  8145 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8146 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8147 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8148 | `		pCase = &aCase[n];` |
|      130 |  8149 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8150 | `		/* Execute the case expression first */` |
|      130 |  8151 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8152 | `		/* Compare the two expression */` |
|      130 |  8153 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8154 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8155 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8156 | `		if( rc == 0 ){` |
|        - |  8157 | `			/* Value match,jump to this block */` |
|       52 |  8158 | `			pc = pCase->nStart - 1;` |
|       52 |  8159 | `			break;` |
|        - |  8160 | `		}` |
|       41 |  8161 | `	}` |
|       54 |  8162 | `	VmPopOperand(&pTos,1);` |
|       54 |  8163 | `	if( n >= nEntry ){` |
|        - |  8164 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8165 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8166 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8167 | `		}else{` |
|        - |  8168 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8169 | `			pc = pSwitch->nOut - 1;` |
|        - |  8170 | `		}` |
|        1 |  8171 | `	}` |
|       54 |  8172 | `	break;` |
|        - |  8173 | `					}` |
|        - |  8174 | `/*` |
|        - |  8175 | ` * OP_MATCH * * P3` |
|        - |  8176 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8177 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8178 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8179 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8180 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8181 | ` */` |
|       54 |  8182 | `case PH7_OP_MATCH: {` |
|      110 |  8183 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8184 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8185 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8186 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8187 | `	int matched = 0;` |
|        - |  8188 | `#ifdef UNTRUST` |
|        - |  8189 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8190 | `		goto Abort;` |
|        - |  8191 | `	}` |
|        - |  8192 | `#endif` |
|      110 |  8193 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8194 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8195 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8196 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8197 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8198 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8199 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8200 | `		pArm = &aArm[i];` |
|      240 |  8201 | `		if( pArm->bDefault ){` |
|       13 |  8202 | `			pDefault = pArm;` |
|       13 |  8203 | `			continue;` |
|        - |  8204 | `		}` |
|      228 |  8205 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8206 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8207 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8208 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8209 | `				continue;` |
|        - |  8210 | `			}` |
|      260 |  8211 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8212 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8213 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8214 | `			if( rc == 0 ){` |
|       93 |  8215 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8216 | `				matched = 1;` |
|       93 |  8217 | `				break;` |
|        - |  8218 | `			}` |
|       85 |  8219 | `		}` |
|      115 |  8220 | `	}` |
|      110 |  8221 | `	if( !matched && pDefault ){` |
|       13 |  8222 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8223 | `		matched = 1;` |
|        6 |  8224 | `	}` |
|      110 |  8225 | `	if( !matched ){` |
|        5 |  8226 | `		const char *zType = "unknown";` |
|        - |  8227 | `		char zMsg[128];` |
|        - |  8228 | `		sxu32 nMsg;` |
|        5 |  8229 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8230 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8231 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8232 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8233 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8234 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8235 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8236 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8237 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8238 | `		default: break;` |
|        - |  8239 | `		}` |
|        7 |  8240 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8241 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8242 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8243 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8244 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8245 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8246 | `		goto Abort;` |
|        - |  8247 | `	}` |
|      105 |  8248 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8249 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8250 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8251 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8252 | `	break;` |
|        - |  8253 | `					}` |
|        - |  8254 | `/*` |
|        - |  8255 | ` * OP_YIELD P1 P2 *` |
|        - |  8256 | ` *  Yield a value from a generator function.` |
|        - |  8257 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8258 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8259 | ` */` |
|       34 |  8260 | `case PH7_OP_YIELD: {` |
|        - |  8261 | `	ph7_generator *pGen;` |
|       70 |  8262 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8263 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8264 | `		goto Abort;` |
|        - |  8265 | `	}` |
|       70 |  8266 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8267 | `	if( pInstr->iP2 ){` |
|        - |  8268 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8269 | `#ifdef UNTRUST` |
|        - |  8270 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8271 | `#endif` |
|        7 |  8272 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8273 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8274 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8275 | `		VmPopOperand(&pTos, 1);` |
|        - |  8276 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8277 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8278 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8279 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8280 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8281 | `			}` |
|        1 |  8282 | `		}` |
|       67 |  8283 | `	}else if( pInstr->iP1 ){` |
|        - |  8284 | `		/* yield $value */` |
|        - |  8285 | `#ifdef UNTRUST` |
|        - |  8286 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8287 | `#endif` |
|       64 |  8288 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8289 | `		VmPopOperand(&pTos, 1);` |
|        - |  8290 | `		/* Auto-increment key */` |
|       64 |  8291 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8292 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8293 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8294 | `	}else{` |
|        - |  8295 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8296 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8297 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8298 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8299 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8300 | `	}` |
|        - |  8301 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8302 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8303 | `	goto Suspend;` |
|        - |  8304 |  |
|        - |  8305 | `/*` |
|        - |  8306 | ` * OP_CALL P1 * *` |
|        - |  8307 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8308 | ` *  function on the stack.` |
|        - |  8309 | ` */` |
|   355283 |  8310 | `case PH7_OP_CALL: {` |
|   710612 |  8311 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8312 | `	ph7_value *pArg;` |
|   710612 |  8313 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   710612 |  8314 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8315 | `	SyHashEntry *pEntry;` |
|        - |  8316 | `	SyString sName;` |
|        - |  8317 | `	/* Extract function name */` |
|   710612 |  8318 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8319 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8320 | `			ph7_value sResult;` |
|        - |  8321 | `			sxi32 rcArr;` |
|        3 |  8322 | `			SySetReset(&aArg);` |
|        3 |  8323 | `			while( pArg < pTos ){` |
|      ! 0 |  8324 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8325 | `				pArg++;` |
|      ! 0 |  8326 | `			}` |
|        3 |  8327 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8328 | `			/* May be a class instance and it's static method */` |
|        3 |  8329 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8330 | `			SySetReset(&aArg);` |
|        - |  8331 | `			/* Pop given arguments */` |
|        3 |  8332 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8333 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8334 | `			}` |
|        3 |  8335 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8336 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8337 | `				goto Abort;` |
|        - |  8338 | `			}` |
|        3 |  8339 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8340 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8341 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8342 | `				sxi32 iResumePc;` |
|        3 |  8343 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8344 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8345 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8346 | `					pc = iResumePc;` |
|        3 |  8347 | `					break;` |
|        - |  8348 | `				}` |
|      ! 0 |  8349 | `				goto Exception;` |
|        - |  8350 | `			}` |
|        - |  8351 | `			/* Copy result */` |
|      ! 0 |  8352 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8353 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8354 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8355 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8356 | `			ph7_value sResult;` |
|        - |  8357 | `			sxi32 rcInv;` |
|       84 |  8358 | `			SySetReset(&aArg);` |
|      200 |  8359 | `			while( pArg < pTos ){` |
|      118 |  8360 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8361 | `				pArg++;` |
|        2 |  8362 | `			}` |
|       84 |  8363 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8364 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8365 | `				(int)SySetUsed(&aArg),` |
|       82 |  8366 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8367 | `				&sResult,` |
|       82 |  8368 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8369 | `			SySetReset(&aArg);` |
|       84 |  8370 | `			if( nCallArgs > 0 ){` |
|       76 |  8371 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8372 | `			}` |
|       84 |  8373 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8374 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8375 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8376 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8377 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8378 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8379 | `				pThis->iRef++;` |
|       13 |  8380 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8381 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8382 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8383 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8384 | `					goto Abort;` |
|        - |  8385 | `				}` |
|        - |  8386 | `				{` |
|       13 |  8387 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8388 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8389 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8390 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8391 | `						break;` |
|        - |  8392 | `					}` |
|        - |  8393 | `				}` |
|      ! 0 |  8394 | `				goto Exception;` |
|        - |  8395 | `			}` |
|       72 |  8396 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8397 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8398 | `				goto Abort;` |
|        - |  8399 | `			}` |
|       72 |  8400 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8401 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8402 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8403 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8404 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8405 | `				sxi32 iResumePc;` |
|        7 |  8406 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8407 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8408 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8409 | `					pc = iResumePc;` |
|        5 |  8410 | `					break;` |
|        - |  8411 | `				}` |
|        3 |  8412 | `				goto Exception;` |
|        - |  8413 | `			}` |
|       66 |  8414 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8415 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8416 | `		}else{` |
|        - |  8417 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8418 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8419 | `			/* Pop given arguments */` |
|      ! 0 |  8420 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8421 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8422 | `			}` |
|        - |  8423 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8424 | `			PH7_MemObjRelease(pTos);` |
|        - |  8425 | `		}` |
|       66 |  8426 | `		break;` |
|        - |  8427 | `	}` |
|   710528 |  8428 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8429 | `	/* Check for a compiled function first.` |
|        - |  8430 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8431 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   710528 |  8432 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8433 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8434 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8435 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8436 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8437 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8438 | `	{` |
|   710528 |  8439 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   710528 |  8440 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8441 | `		const char *zFunc;` |
|        - |  8442 | `		const char *zEnd;` |
|        - |  8443 | `		const char *z;` |
|        - |  8444 | `		SyString sGlobal;` |
|       22 |  8445 | `		zFunc = sName.zString;` |
|       22 |  8446 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8447 | `		z = zEnd;` |
|        - |  8448 | `		/* Find last namespace separator */` |
|      194 |  8449 | `		while( z > zFunc ){` |
|      194 |  8450 | `			if( z[-1] == '\\' ){` |
|       22 |  8451 | `				break;` |
|        - |  8452 | `			}` |
|      174 |  8453 | `			z--;` |
|        2 |  8454 | `		}` |
|       22 |  8455 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8456 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8457 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8458 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8459 | `		}` |
|       10 |  8460 | `	}` |
|        - |  8461 | `	} /* end VmCallArgMap namespace scope */` |
|   710528 |  8462 | `	if( pEntry ){` |
|        - |  8463 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8464 | `		ph7_class_instance *pThis;` |
|        - |  8465 | `		ph7_value *pFrameStack;` |
|        - |  8466 | `		ph7_vm_func *pVmFunc;` |
|        - |  8467 | `		ph7_class *pSelf;` |
|        - |  8468 | `		VmFrame *pFrame;` |
|        - |  8469 | `		ph7_value *pObj;` |
|        - |  8470 | `		VmSlot sArg;` |
|        - |  8471 | `		sxu32 n;` |
|        - |  8472 | `		/* initialize fields */` |
|    18382 |  8473 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18382 |  8474 | `		pThis = 0;` |
|    18382 |  8475 | `		pSelf = 0;` |
|    18382 |  8476 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8477 | `			ph7_class_method *pMeth;` |
|        - |  8478 | `			/* Class method call */` |
|     3292 |  8479 | `			ph7_value *pTarget = &pTos[-1];` |
|     3292 |  8480 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8481 | `				/* Extract the 'this' pointer */` |
|     3292 |  8482 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8483 | `					/* Instance already loaded */` |
|     3202 |  8484 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3202 |  8485 | `					pThis->iRef++;` |
|     3202 |  8486 | `					pSelf = pThis->pClass;` |
|     1600 |  8487 | `				}` |
|     3292 |  8488 | `				if( pSelf == 0 ){` |
|       92 |  8489 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8490 | `						/* "Late Static Binding" class name */` |
|      128 |  8491 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8492 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8493 | `					}` |
|       92 |  8494 | `					if( pSelf == 0 ){` |
|       21 |  8495 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8496 | `					}` |
|       45 |  8497 | `				}` |
|     3292 |  8498 | `				if( pThis == 0  ){` |
|       92 |  8499 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8500 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8501 | `					if( pFrameLocal->pParent ){` |
|        - |  8502 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8503 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8504 | `						if( pThis ){` |
|       21 |  8505 | `							pThis->iRef++;` |
|       10 |  8506 | `						}` |
|       32 |  8507 | `					}` |
|       45 |  8508 | `				}` |
|     3292 |  8509 | `				VmPopOperand(&pTos,1);` |
|     3292 |  8510 | `				PH7_MemObjRelease(pTos);` |
|        - |  8511 | `				/* Synchronize pointers */` |
|     3292 |  8512 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8513 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8514 | `				 * user have already computed the random generated unique class method name` |
|        - |  8515 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8516 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8517 | `				 */` |
|     3292 |  8518 | `				while( pArg < pStack ){` |
|      ! 0 |  8519 | `					pArg++;` |
|      ! 0 |  8520 | `				}` |
|     3292 |  8521 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8522 | `					/* Check if the call is allowed */` |
|     3292 |  8523 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3292 |  8524 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8525 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8526 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8527 | `							char zMsg[256];` |
|      ! 0 |  8528 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8529 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8530 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8531 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8532 | `							/* Pop given arguments */` |
|      ! 0 |  8533 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8534 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8535 | `							}` |
|      ! 0 |  8536 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8537 | `							goto Abort;` |
|        - |  8538 | `						}` |
|        6 |  8539 | `					}` |
|     1645 |  8540 | `				}` |
|     1645 |  8541 | `			}` |
|     1645 |  8542 | `		}` |
|        - |  8543 | `		/* Check The recursion limit */` |
|    18382 |  8544 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8545 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8546 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8547 | `				&pVmFunc->sName);` |
|        - |  8548 | `			/* Pop given arguments */` |
|        3 |  8549 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8550 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8551 | `			}` |
|        - |  8552 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8553 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8554 | `			break;` |
|        - |  8555 | `		}` |
|    18380 |  8556 | `		if( pVmFunc->pNextName ){` |
|        - |  8557 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8558 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8559 | `		}` |
|    18380 |  8560 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8561 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8562 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8563 | `			ph7_generator *pGenerator;` |
|        - |  8564 | `			ph7_class_instance *pGenObj;` |
|        - |  8565 | `			ph7_value *pCtxAttr;` |
|        - |  8566 | `			SyString sAttrName;` |
|        - |  8567 | `			ph7_value **apCallArgs;` |
|        - |  8568 | `			int nGenArgs, iArg;` |
|        - |  8569 | `			/* Collect arguments from the operand stack */` |
|       24 |  8570 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8571 | `			apCallArgs = 0;` |
|       24 |  8572 | `			if( nGenArgs > 0 ){` |
|       14 |  8573 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8574 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8575 | `				if( apCallArgs == 0 ){` |
|        - |  8576 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8577 | `					nGenArgs = 0;` |
|      ! 0 |  8578 | `				}else{` |
|       10 |  8579 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8580 | `					int didReorder = 0;` |
|       10 |  8581 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8582 | `						/* Named-argument reordering for generator */` |
|        5 |  8583 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8584 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8585 | `						sxu32 nNV = nF;` |
|        5 |  8586 | `						sxi32 iVIdx = -1;` |
|        - |  8587 | `						sxi32 *aGSlot;` |
|        - |  8588 | `						sxu8 *aGUsed;` |
|        - |  8589 | `						sxu32 gi;` |
|       13 |  8590 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8591 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8592 | `						}` |
|        7 |  8593 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8594 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8595 | `						if( aGSlot ){` |
|        5 |  8596 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8597 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8598 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8599 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8600 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8601 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8602 | `								goto Abort;` |
|        - |  8603 | `							}` |
|        - |  8604 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8605 | `							 * append overflow (variadic / positional beyond` |
|        - |  8606 | `							 * formals) so downstream sees every argument. */` |
|        - |  8607 | `							{` |
|        5 |  8608 | `								int nOut = 0;` |
|       13 |  8609 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8610 | `									sxu32 gj;` |
|       13 |  8611 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8612 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8613 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8614 | `											break;` |
|        - |  8615 | `										}` |
|        3 |  8616 | `									}` |
|        5 |  8617 | `								}` |
|       13 |  8618 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8619 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8620 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8621 | `									}` |
|        5 |  8622 | `								}` |
|        5 |  8623 | `								nGenArgs = nOut;` |
|        - |  8624 | `							}` |
|        5 |  8625 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8626 | `							didReorder = 1;` |
|        2 |  8627 | `						}` |
|        - |  8628 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8629 | `						 * positional fill below — preserves arg order rather` |
|        - |  8630 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8631 | `					}` |
|       10 |  8632 | `					if( !didReorder ){` |
|       12 |  8633 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8634 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8635 | `						}` |
|        2 |  8636 | `					}` |
|        - |  8637 | `				}` |
|        4 |  8638 | `			}` |
|        - |  8639 | `			/* Create execution context and generator wrapper */` |
|       24 |  8640 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8641 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8642 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8643 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8644 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8645 | `				break;` |
|        - |  8646 | `			}` |
|       24 |  8647 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8648 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8649 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8650 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8651 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8652 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8653 | `				break;` |
|        - |  8654 | `			}` |
|        - |  8655 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8656 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8657 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8658 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8659 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8660 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8661 | `			if( apCallArgs ){` |
|       10 |  8662 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8663 | `			}` |
|       24 |  8664 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8665 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8666 | `				if( pThis ){` |
|      ! 0 |  8667 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8668 | `				}` |
|      ! 0 |  8669 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8670 | `					goto Abort;` |
|        - |  8671 | `				}` |
|      ! 0 |  8672 | `				break;` |
|        - |  8673 | `			}` |
|        - |  8674 | `			/* Create Generator class instance */` |
|       24 |  8675 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8676 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8677 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8678 | `				break;` |
|        - |  8679 | `			}` |
|        - |  8680 | `			/* Store generator in __ctx attribute */` |
|       24 |  8681 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8682 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8683 | `			if( pCtxAttr ){` |
|       24 |  8684 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8685 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8686 | `			}` |
|        - |  8687 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8688 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8689 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8690 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8691 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8692 | `			pGenObj->iRef++;` |
|       24 |  8693 | `			if( pThis ){` |
|      ! 0 |  8694 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8695 | `			}` |
|       24 |  8696 | `			break;` |
|        - |  8697 | `		}` |
|        - |  8698 | `		/* Extract the formal argument set */` |
|    18358 |  8699 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8700 | `		/* Create a new VM frame  */` |
|    18358 |  8701 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18358 |  8702 | `		if( rc != SXRET_OK ){` |
|        - |  8703 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8704 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8705 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8706 | `				&pVmFunc->sName);` |
|        - |  8707 | `			/* Pop given arguments */` |
|      ! 0 |  8708 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8709 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8710 | `			}` |
|        - |  8711 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8712 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8713 | `			break;` |
|        - |  8714 | `		}` |
|    18358 |  8715 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8716 | `			/* Install the '$this' variable */` |
|        - |  8717 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3220 |  8718 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3220 |  8719 | `			if( pObj ){` |
|        - |  8720 | `				/* Reflect the change */` |
|     3220 |  8721 | `				pObj->x.pOther = pThis;` |
|     3220 |  8722 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1609 |  8723 | `			}` |
|     1609 |  8724 | `		}` |
|    18358 |  8725 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8726 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8727 | `			/* Install static variables */` |
|      ! 0 |  8728 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8729 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8730 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8731 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8732 | `					/* Initialize the static variables */` |
|      ! 0 |  8733 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8734 | `					if( pObj ){` |
|        - |  8735 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8736 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8737 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8738 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8739 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8740 | `						}` |
|      ! 0 |  8741 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8742 | `					}else{` |
|      ! 0 |  8743 | `						continue;` |
|        - |  8744 | `					}` |
|      ! 0 |  8745 | `				}` |
|        - |  8746 | `				/* Install in the current frame */` |
|      ! 0 |  8747 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8748 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8749 | `			}` |
|      ! 0 |  8750 | `		}` |
|        - |  8751 | `		/* Push arguments in the local frame */` |
|        - |  8752 | `		{` |
|    18358 |  8753 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8754 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8755 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18358 |  8756 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18358 |  8757 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8758 | `			/* ============================================================` |
|        - |  8759 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8760 | `			 *` |
|        - |  8761 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8762 | `			 * or position, then install them in the frame.` |
|        - |  8763 | `			 * ============================================================ */` |
|       96 |  8764 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8765 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8766 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8767 | `			sxu32 nNonVariadic;` |
|        - |  8768 | `			sxi32 *aSlot;` |
|        - |  8769 | `			sxu8  *aUsed;` |
|        - |  8770 | `			sxu32 i;` |
|        - |  8771 | `			/* Find variadic parameter index */` |
|      292 |  8772 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8773 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8774 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8775 | `					break;` |
|        - |  8776 | `				}` |
|      100 |  8777 | `			}` |
|       96 |  8778 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8779 | `			/* Allocate mapping arrays */` |
|      143 |  8780 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8781 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8782 | `			if( aSlot == 0 ){` |
|      ! 0 |  8783 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8784 | `				goto Abort;` |
|        - |  8785 | `			}` |
|       96 |  8786 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8787 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8788 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8789 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8790 | `			if( rc == PH7_ABORT ){` |
|        7 |  8791 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8792 | `				goto Abort;` |
|        - |  8793 | `			}` |
|        - |  8794 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8795 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8796 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8797 | `				sxi32 iSrc = -1;` |
|      309 |  8798 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8799 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8800 | `						iSrc = (sxi32)i;` |
|      169 |  8801 | `						break;` |
|        - |  8802 | `					}` |
|       62 |  8803 | `				}` |
|      187 |  8804 | `				if( iSrc >= 0 ){` |
|        - |  8805 | `					/* Argument was provided — install with type checking */` |
|      169 |  8806 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8807 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8808 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8809 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8810 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8811 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8812 | `					}` |
|        - |  8813 | `					/* Type checking: union types */` |
|      169 |  8814 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8815 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8816 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8817 | `							bCallIsStrict);` |
|       13 |  8818 | `						if( rcU != SXRET_OK ){` |
|        - |  8819 | `							const char *zGiven;` |
|      ! 0 |  8820 | `							const char *zExpected = "union";` |
|        - |  8821 | `							char zBuf[128];` |
|        - |  8822 | `							char zTypeBuf[128];` |
|      ! 0 |  8823 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8824 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8825 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8826 | `								zGiven = "null";` |
|      ! 0 |  8827 | `							}else{` |
|      ! 0 |  8828 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8829 | `							}` |
|      ! 0 |  8830 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8831 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8832 | `							}` |
|      ! 0 |  8833 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8834 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8835 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8836 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8837 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8838 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8839 | `							pFrameStack = 0;` |
|      ! 0 |  8840 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8841 | `							goto SkipFuncBody;` |
|        - |  8842 | `						}` |
|      171 |  8843 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8844 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8845 | `						/* Scalar/class type checking */` |
|       17 |  8846 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8847 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8848 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8849 | `							if( pClass ){` |
|      ! 0 |  8850 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8851 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8852 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8853 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8854 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8855 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8856 | `									}` |
|      ! 0 |  8857 | `								}else{` |
|      ! 0 |  8858 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8859 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8860 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8861 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8862 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8863 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8864 | `									}` |
|        - |  8865 | `								}` |
|      ! 0 |  8866 | `							}` |
|       17 |  8867 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8868 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8869 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8870 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8871 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8872 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8873 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8874 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8875 | `								pFrameStack = 0;` |
|      ! 0 |  8876 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8877 | `								goto SkipFuncBody;` |
|        7 |  8878 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8879 | `								char zTypeBuf[128];` |
|      ! 0 |  8880 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8881 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8882 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8883 | `									ph7_type_name(pVal));` |
|      ! 0 |  8884 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8885 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8886 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8887 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8888 | `								pFrameStack = 0;` |
|      ! 0 |  8889 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8890 | `								goto SkipFuncBody;` |
|        - |  8891 | `							}` |
|        3 |  8892 | `						}` |
|        8 |  8893 | `					}` |
|        - |  8894 | `					/* Install: by reference or by value */` |
|      169 |  8895 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8896 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8897 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8898 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8899 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8900 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8901 | `							}` |
|      ! 0 |  8902 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8903 | `						}else{` |
|        7 |  8904 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8905 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8906 | `							if( pRefEntry == 0 ){` |
|        7 |  8907 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8908 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8909 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8910 | `								sArg.pUserData = 0;` |
|        5 |  8911 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8912 | `							}` |
|        5 |  8913 | `							pObj = 0;` |
|        - |  8914 | `						}` |
|        3 |  8915 | `					}else{` |
|      165 |  8916 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8917 | `					}` |
|      169 |  8918 | `					if( pObj ){` |
|      165 |  8919 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8920 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8921 | `						sArg.pUserData = 0;` |
|      165 |  8922 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8923 | `					}` |
|       85 |  8924 | `				}else{` |
|        - |  8925 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8926 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8927 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8928 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8929 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8930 | `						if( pObj ){` |
|       19 |  8931 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8932 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8933 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8934 | `							sArg.pUserData = 0;` |
|       19 |  8935 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8936 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8937 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8938 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8939 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8940 | `							}` |
|        9 |  8941 | `						}` |
|        9 |  8942 | `					}` |
|        - |  8943 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8944 | `				}` |
|       94 |  8945 | `			}` |
|        - |  8946 | `			/* Handle variadic parameter */` |
|       89 |  8947 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8948 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8949 | `				if( pObj ){` |
|        9 |  8950 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8951 | `					{` |
|        9 |  8952 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8953 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8954 | `							if( aSlot[i] == -1 ){` |
|       16 |  8955 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8956 | `									/* Named variadic entry: insert with string key */` |
|        - |  8957 | `									ph7_value sKey;` |
|       11 |  8958 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8959 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8960 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8961 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8962 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8963 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8964 | `								}else{` |
|        - |  8965 | `									/* Positional variadic entry */` |
|      ! 0 |  8966 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8967 | `								}` |
|        5 |  8968 | `							}` |
|       12 |  8969 | `						}` |
|        - |  8970 | `					}` |
|        9 |  8971 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8972 | `					sArg.pUserData = 0;` |
|        9 |  8973 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8974 | `				}` |
|        5 |  8975 | `			}else{` |
|        - |  8976 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8977 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8978 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8979 | `				 * the positional-only path's behavior. */` |
|       81 |  8980 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  8981 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  8982 | `					if( aSlot[i] == -2 ){` |
|        - |  8983 | `						char zAnonBuf[32];` |
|        - |  8984 | `						SyString sAnonName;` |
|      ! 0 |  8985 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8986 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8987 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8988 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8989 | `						if( pObj ){` |
|      ! 0 |  8990 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8991 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8992 | `							sArg.pUserData = 0;` |
|      ! 0 |  8993 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8994 | `						}` |
|      ! 0 |  8995 | `						nAnon++;` |
|      ! 0 |  8996 | `					}` |
|       79 |  8997 | `				}` |
|        - |  8998 | `			}` |
|        - |  8999 | `			/* Release all stack arguments */` |
|      267 |  9000 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9001 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9002 | `			}` |
|       89 |  9003 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9004 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9005 | `			n = nFormal;` |
|       45 |  9006 | `		}else{` |
|        - |  9007 | `		/* ============================================================` |
|        - |  9008 | `		 * Positional-only matching path (original)` |
|        - |  9009 | `		 * ============================================================ */` |
|    18264 |  9010 | `		n = 0;` |
|    48696 |  9011 | `		while( pArg < pTos ){` |
|    30506 |  9012 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9013 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9014 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9015 | `				if( pObj ){` |
|        - |  9016 | `					/* Initialize as empty array */` |
|       40 |  9017 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9018 | `					{` |
|       40 |  9019 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9020 | `						while( pArg < pTos ){` |
|        - |  9021 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9022 | `							 *` |
|        - |  9023 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9024 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9025 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9026 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9027 | `							 * fixing both wants a separate counter for elements` |
|        - |  9028 | `							 * already packed into the variadic array. */` |
|      114 |  9029 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9030 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9031 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9032 | `									bCallIsStrict);` |
|       16 |  9033 | `								if( rcU != SXRET_OK ){` |
|        - |  9034 | `									const char *zGiven;` |
|        3 |  9035 | `									const char *zExpected = "union";` |
|        - |  9036 | `									char zBuf[128];` |
|        - |  9037 | `									char zTypeBuf[128];` |
|        3 |  9038 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9039 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9040 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9041 | `										zGiven = "null";` |
|      ! 0 |  9042 | `									}else{` |
|        3 |  9043 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9044 | `									}` |
|        3 |  9045 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9046 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9047 | `									}` |
|        4 |  9048 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9049 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9050 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9051 | `										goto Abort;` |
|        - |  9052 | `									}` |
|        3 |  9053 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9054 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9055 | `									pFrameStack = 0;` |
|        3 |  9056 | `									rc = PH7_EXCEPTION;` |
|        3 |  9057 | `									goto SkipFuncBody;` |
|        - |  9058 | `								}` |
|       14 |  9059 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9060 | `								pArg++;` |
|       14 |  9061 | `								continue;` |
|        - |  9062 | `							}` |
|        - |  9063 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9064 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9065 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9066 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9067 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9068 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9069 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9070 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9071 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9072 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9073 | `										goto Abort;` |
|        - |  9074 | `									}` |
|        - |  9075 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9076 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9077 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9078 | `									pFrameStack = 0;` |
|      ! 0 |  9079 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9080 | `									goto SkipFuncBody;` |
|       13 |  9081 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9082 | `									char zTypeBuf[128];` |
|      ! 0 |  9083 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9084 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9085 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9086 | `										ph7_type_name(pArg));` |
|      ! 0 |  9087 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9088 | `										goto Abort;` |
|        - |  9089 | `									}` |
|      ! 0 |  9090 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9091 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9092 | `									pFrameStack = 0;` |
|      ! 0 |  9093 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9094 | `									goto SkipFuncBody;` |
|        - |  9095 | `								}` |
|        6 |  9096 | `							}` |
|      100 |  9097 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9098 | `							pArg++;` |
|        2 |  9099 | `						}` |
|        - |  9100 | `					}` |
|       38 |  9101 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9102 | `					sArg.pUserData = 0;` |
|       38 |  9103 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9104 | `				}` |
|       38 |  9105 | `				break; /* All remaining args consumed */` |
|        - |  9106 | `			}` |
|    30468 |  9107 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30250 |  9108 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9109 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9110 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9111 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9112 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9113 | `						goto Abort;` |
|        - |  9114 | `					}` |
|      ! 0 |  9115 | `				}` |
|        - |  9116 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30252 |  9117 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9118 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9119 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9120 | `						bCallIsStrict);` |
|       60 |  9121 | `					if( rcU != SXRET_OK ){` |
|        - |  9122 | `						const char *zGiven;` |
|       19 |  9123 | `						const char *zExpected = "union";` |
|        - |  9124 | `						char zBuf[128];` |
|        - |  9125 | `						char zTypeBuf[128];` |
|       19 |  9126 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9127 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9128 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9129 | `							zGiven = "null";` |
|        5 |  9130 | `						}else{` |
|        5 |  9131 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9132 | `						}` |
|       19 |  9133 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9134 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9135 | `						}` |
|       28 |  9136 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9137 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9138 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9139 | `							goto Abort;` |
|        - |  9140 | `						}` |
|       19 |  9141 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9142 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9143 | `						pFrameStack = 0;` |
|       19 |  9144 | `						rc = PH7_EXCEPTION;` |
|       19 |  9145 | `						goto SkipFuncBody;` |
|        - |  9146 | `					}` |
|       21 |  9147 | `				}else` |
|        - |  9148 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9149 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30218 |  9150 | `				if( aFormalArg[n].nType > 0` |
|    15811 |  9151 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1402 |  9152 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9153 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9154 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9155 | `						ph7_class *pClass;` |
|        - |  9156 | `						/* Try to extract the desired class */` |
|       26 |  9157 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9158 | `						if( pClass ){` |
|       22 |  9159 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9160 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9161 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9162 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9163 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9164 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9165 | `								}` |
|      ! 0 |  9166 | `							}else{` |
|        - |  9167 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9168 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9169 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9170 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9171 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9172 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9173 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9174 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9175 | `								}` |
|        - |  9176 | `							}` |
|       12 |  9177 | `						}` |
|     1390 |  9178 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9179 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9180 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9181 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9182 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9183 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9184 | `								goto Abort;` |
|        - |  9185 | `							}` |
|        - |  9186 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9187 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9188 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9189 | `							pFrameStack = 0;` |
|       11 |  9190 | `							rc = PH7_EXCEPTION;` |
|       11 |  9191 | `							goto SkipFuncBody;` |
|       16 |  9192 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9193 | `							char zTypeBuf[128];` |
|       11 |  9194 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9195 | `								&aFormalArg[n].sName,` |
|        6 |  9196 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9197 | `								ph7_type_name(pArg));` |
|        8 |  9198 | `							if( rc == PH7_ABORT ){` |
|        5 |  9199 | `								goto Abort;` |
|        - |  9200 | `							}` |
|        3 |  9201 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9202 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9203 | `							pFrameStack = 0;` |
|        3 |  9204 | `							rc = PH7_EXCEPTION;` |
|        3 |  9205 | `							goto SkipFuncBody;` |
|        - |  9206 | `						}` |
|        4 |  9207 | `					}` |
|      692 |  9208 | `				}` |
|    30218 |  9209 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9210 | `					/* Pass by reference */` |
|       58 |  9211 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9212 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9213 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9214 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9215 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9216 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9217 | `						}` |
|        - |  9218 | `						/* Switch to pass by value */` |
|      ! 0 |  9219 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9220 | `					}else{` |
|        - |  9221 | `						SyHashEntry *pRefEntry;` |
|        - |  9222 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9223 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9224 | `						if( pRefEntry == 0 ){` |
|       86 |  9225 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9226 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9227 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9228 | `							sArg.pUserData = 0;` |
|       58 |  9229 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9230 | `						}` |
|       58 |  9231 | `						pObj = 0;` |
|        - |  9232 | `					}` |
|       30 |  9233 | `				}else{` |
|        - |  9234 | `					/* Pass by value,make a copy of the given argument */` |
|    30162 |  9235 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9236 | `				}` |
|    15110 |  9237 | `			}else{` |
|        - |  9238 | `				char zName[32];` |
|        - |  9239 | `				SyString sArgName;` |
|        - |  9240 | `				/* Set a dummy name */` |
|      218 |  9241 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9242 | `				sArgName.zString = zName;` |
|        - |  9243 | `				/* Annonymous argument */` |
|      218 |  9244 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9245 | `			}` |
|    30434 |  9246 | `			if( pObj ){` |
|    30378 |  9247 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9248 | `				/* Insert argument index  */` |
|    30378 |  9249 | `				sArg.nIdx = pObj->nIdx;` |
|    30378 |  9250 | `				sArg.pUserData = 0;` |
|    30378 |  9251 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15188 |  9252 | `			}` |
|    30434 |  9253 | `			PH7_MemObjRelease(pArg);` |
|    30434 |  9254 | `			pArg++;` |
|    30434 |  9255 | `			++n;` |
|        2 |  9256 | `		}` |
|        - |  9257 | `		} /* end named vs positional branch */` |
|        - |  9258 | `		/* Set up closure environment */` |
|    18316 |  9259 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9260 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9261 | `			ph7_value *pValue;` |
|        - |  9262 | `			sxu32 iEnv;` |
|      178 |  9263 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      422 |  9264 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      246 |  9265 | `				pEnv = &aEnv[iEnv];` |
|      246 |  9266 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9267 | `					/* Do not install null value */` |
|      172 |  9268 | `					continue;` |
|        - |  9269 | `				}` |
|       76 |  9270 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9271 | `				if( pValue == 0 ){` |
|      ! 0 |  9272 | `					continue;` |
|        - |  9273 | `				}` |
|        - |  9274 | `				/* Invalidate any prior representation */` |
|       76 |  9275 | `				PH7_MemObjRelease(pValue);` |
|        - |  9276 | `				/* Duplicate bound variable value */` |
|       76 |  9277 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9278 | `			}` |
|       88 |  9279 | `		}` |
|        - |  9280 | `		/* Process default values for remaining formal parameters */` |
|    21128 |  9281 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2860 |  9282 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9283 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9284 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9285 | `				if( pObj ){` |
|       48 |  9286 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9287 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9288 | `					sArg.pUserData = 0;` |
|       48 |  9289 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9290 | `				}` |
|       48 |  9291 | `				n++;` |
|       48 |  9292 | `				break; /* Variadic is always last */` |
|        - |  9293 | `			}` |
|     2814 |  9294 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2808 |  9295 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2808 |  9296 | `				if( pObj ){` |
|        - |  9297 | `					/* Evaluate the default value and extract it's result */` |
|     2808 |  9298 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2808 |  9299 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9300 | `						goto Abort;` |
|        - |  9301 | `					}` |
|        - |  9302 | `					/* Insert argument index */` |
|     2808 |  9303 | `					sArg.nIdx = pObj->nIdx;` |
|     2808 |  9304 | `					sArg.pUserData = 0;` |
|     2808 |  9305 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9306 | `					/* Make sure the default argument is of the correct type */` |
|     2806 |  9307 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1832 |  9308 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9309 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9310 | `						/* Cast to the desired type */` |
|        3 |  9311 | `						xCast(pObj);` |
|        1 |  9312 | `					}` |
|     1403 |  9313 | `				}` |
|     1403 |  9314 | `			}` |
|     2814 |  9315 | `			++n;` |
|        2 |  9316 | `		}` |
|        - |  9317 | `		} /* end VmCallArgMap scope */` |
|        - |  9318 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9319 | `		 * does not return anything.` |
|        - |  9320 | `		 */` |
|    18316 |  9321 | `		PH7_MemObjRelease(pTos);` |
|    18316 |  9322 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9323 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18316 |  9324 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18316 |  9325 | `		if( pFrameStack == 0 ){` |
|        - |  9326 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9327 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9328 | `				&pVmFunc->sName);` |
|      ! 0 |  9329 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9330 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9331 | `			}` |
|      ! 0 |  9332 | `			break;` |
|        - |  9333 | `		}` |
|     9157 |  9334 | `SkipFuncBody:` |
|    18348 |  9335 | `		if( pSelf ){` |
|        - |  9336 | `			/* Push class name */` |
|     3290 |  9337 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1644 |  9338 | `		}` |
|        - |  9339 | `		/* Increment nesting level */` |
|    18348 |  9340 | `		pVm->nRecursionDepth++;` |
|    18348 |  9341 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9342 | `			/* Execute function body */` |
|    27473 |  9343 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18314 |  9344 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9157 |  9345 | `		}` |
|        - |  9346 | `		/* Decrement nesting level */` |
|    18348 |  9347 | `		pVm->nRecursionDepth--;` |
|    18348 |  9348 | `		if( pSelf ){` |
|        - |  9349 | `			/* Pop class name */` |
|     3290 |  9350 | `			(void)SySetPop(&pVm->aSelf);` |
|     1644 |  9351 | `		}` |
|        - |  9352 | `		/* Cleanup the mess left behind */` |
|    18348 |  9353 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9354 | `			/* Return by reference,reflect that */` |
|        9 |  9355 | `			if( n != SXU32_HIGH ){` |
|        9 |  9356 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9357 | `				sxu32 i;` |
|        - |  9358 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9359 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9360 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9361 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9362 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9363 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9364 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9365 | `								&pVmFunc->sName);` |
|      ! 0 |  9366 | `						}` |
|      ! 0 |  9367 | `						n = SXU32_HIGH;` |
|      ! 0 |  9368 | `						break;` |
|        - |  9369 | `					}` |
|        3 |  9370 | `				}` |
|        5 |  9371 | `			}else{` |
|      ! 0 |  9372 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9373 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9374 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9375 | `						&pVmFunc->sName);` |
|      ! 0 |  9376 | `				}` |
|        - |  9377 | `			}` |
|        9 |  9378 | `			pTos->nIdx = n;` |
|        4 |  9379 | `		}` |
|        - |  9380 | `		/* Cleanup the mess left behind */` |
|    18348 |  9381 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9382 | `			/* An exception was throw in this frame */` |
|       96 |  9383 | `			pFrame = pFrame->pParent;` |
|       96 |  9384 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9385 | `				/* Pop the resutlt */` |
|       62 |  9386 | `				VmPopOperand(&pTos,1);` |
|        - |  9387 | `				/* Jump to this destination */` |
|       62 |  9388 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9389 | `				rc = PH7_OK;` |
|       32 |  9390 | `			}else{` |
|       35 |  9391 | `				if( pFrame->pParent ){` |
|       35 |  9392 | `					rc = PH7_EXCEPTION;` |
|       18 |  9393 | `				}else{` |
|        - |  9394 | `					/* Continue normal execution */` |
|      ! 0 |  9395 | `					rc = PH7_OK;` |
|        - |  9396 | `				}` |
|        - |  9397 | `			}` |
|       47 |  9398 | `		}` |
|        - |  9399 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18348 |  9400 | `		if( pFrameStack ){` |
|    18316 |  9401 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9157 |  9402 | `		}` |
|        - |  9403 | `		/* Leave the frame */` |
|    18348 |  9404 | `		VmLeaveFrame(&(*pVm));` |
|    18348 |  9405 | `		if( rc == PH7_ABORT ){` |
|        - |  9406 | `			/* Abort processing immeditaley */` |
|       17 |  9407 | `			goto Abort;` |
|    18332 |  9408 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9409 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9410 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9411 | `			 * overwriting the state saved by the inner level.` |
|        - |  9412 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9413 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9414 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9415 | `			goto Suspend;` |
|    18294 |  9416 | `		}else if( rc == PH7_EXCEPTION ){` |
|       35 |  9417 | `			goto Exception;` |
|        - |  9418 | `		}` |
|     9131 |  9419 | `	}else{` |
|        - |  9420 | `		ph7_user_func *pFunc;` |
|        - |  9421 | `		ph7_context sCtx;` |
|        - |  9422 | `		ph7_value sRet;` |
|        - |  9423 | `		/* Look for an installed foreign function.` |
|        - |  9424 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9425 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9426 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9427 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   692148 |  9428 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9429 | `		{` |
|   692148 |  9430 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   692148 |  9431 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9432 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9433 | `			const char *zShort = sName.zString;` |
|        - |  9434 | `			sxu32 i;` |
|      334 |  9435 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9436 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9437 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9438 | `				}` |
|      158 |  9439 | `			}` |
|       22 |  9440 | `			if( zShort != sName.zString ){` |
|       22 |  9441 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9442 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9443 | `			}` |
|       10 |  9444 | `		}` |
|        - |  9445 | `		} /* end VmCallArgMap namespace scope */` |
|   692148 |  9446 | `		if( pEntry == 0 ){` |
|        - |  9447 | `			/* Call to undefined function */` |
|        5 |  9448 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9449 | `			/* Pop given arguments */` |
|        5 |  9450 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9451 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9452 | `			}` |
|        - |  9453 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9454 | `			PH7_MemObjRelease(pTos);` |
|       56 |  9455 | `			break;` |
|        - |  9456 | `		}` |
|   692144 |  9457 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9458 | `		/* Start collecting function arguments */` |
|   692144 |  9459 | `		SySetReset(&aArg);` |
|  1865704 |  9460 | `		while( pArg < pTos ){` |
|  1173562 |  9461 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1173562 |  9462 | `			pArg++;` |
|        2 |  9463 | `		}` |
|        - |  9464 | `		/* Assume a null return value */` |
|   692144 |  9465 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9466 | `		/* Init the call context */` |
|   692144 |  9467 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9468 | `		/* Call the foreign function */` |
|   692144 |  9469 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9470 | `		/* Release the call context */` |
|   692144 |  9471 | `		VmReleaseCallContext(&sCtx);` |
|   692144 |  9472 | `		if( rc == PH7_ABORT ){` |
|      497 |  9473 | `			goto Abort;` |
|   691648 |  9474 | `		}else if( rc == PH7_EXCEPTION ){` |
|      108 |  9475 | `			VmFrame *pFrm = pVm->pFrame;` |
|      108 |  9476 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      108 |  9477 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9478 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9479 | `				goto Exception;` |
|        - |  9480 | `			}` |
|        - |  9481 | `			/* Exception was caught: pop args and the result slot */` |
|      104 |  9482 | `			PH7_MemObjRelease(&sRet);` |
|      104 |  9483 | `			if( pInstr->iP1 > 0 ){` |
|       88 |  9484 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       43 |  9485 | `			}` |
|        - |  9486 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      104 |  9487 | `			VmPopOperand(&pTos,1);` |
|        - |  9488 | `			/* Jump past the try/catch block via the exception frame */` |
|      104 |  9489 | `			pFrm = pVm->pFrame;` |
|      104 |  9490 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      104 |  9491 | `				pc = pFrm->iExceptionJump - 1;` |
|       51 |  9492 | `			}` |
|      104 |  9493 | `			break;` |
|        - |  9494 | `		}` |
|   691542 |  9495 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9496 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9497 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9498 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9499 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9500 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9501 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9502 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9503 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9504 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9505 | `			}` |
|        - |  9506 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9507 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9508 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9509 | `			goto Suspend;` |
|        - |  9510 | `		}` |
|   691504 |  9511 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9512 | `			/* Pop function name and arguments */` |
|   669722 |  9513 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   334882 |  9514 | `		}` |
|        - |  9515 | `		/* Save foreign function return value */` |
|   691504 |  9516 | `		PH7_MemObjStore(&sRet,pTos);` |
|   691504 |  9517 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9518 | `	}` |
|   709762 |  9519 | `	break;` |
|        - |  9520 | `				  }` |
|        - |  9521 | `/*` |
|        - |  9522 | ` * OP_CONSUME: P1 * *` |
|        - |  9523 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9524 | ` */` |
|    15665 |  9525 | `case PH7_OP_CONSUME: {` |
|    31332 |  9526 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    31332 |  9527 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9528 |  |
|    31332 |  9529 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    31332 |  9530 | `	pCur = pOut;` |
|        - |  9531 | `	/* Start the consume process  */` |
|    62704 |  9532 | `	while( pOut <= pTos ){` |
|        - |  9533 | `		/* Force a string cast */` |
|    31374 |  9534 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1048 |  9535 | `			PH7_MemObjToString(pOut);` |
|      523 |  9536 | `		}` |
|    31374 |  9537 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9538 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9539 | `			/* Invoke the output consumer callback */` |
|    19054 |  9540 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19054 |  9541 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19054 |  9542 | `			SyBlobRelease(&pOut->sBlob);` |
|    19054 |  9543 | `			if( rc == SXERR_ABORT ){` |
|        - |  9544 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9545 | `				goto Abort;` |
|        - |  9546 | `			}` |
|     9526 |  9547 | `		}` |
|    31374 |  9548 | `		pOut++;` |
|        2 |  9549 | `	}` |
|    31332 |  9550 | `	pTos = &pCur[-1];` |
|    31330 |  9551 | `	break;` |
|        - |  9552 | `					 }` |
|        - |  9553 |  |
|        - |  9554 | `		} /* Switch() */` |
| 11713334 |  9555 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9556 | `	} /* For(;;) */` |
|    21941 |  9557 | `Done:` |
|    43884 |  9558 | `	SySetRelease(&aArg);` |
|    43884 |  9559 | `	return SXRET_OK;` |
|       72 |  9560 | `Suspend:` |
|      146 |  9561 | `	SySetRelease(&aArg);` |
|      146 |  9562 | `	return PH7_SUSPEND;` |
|      280 |  9563 | `Abort:` |
|      561 |  9564 | `	SySetRelease(&aArg);` |
|     1875 |  9565 | `	while( pTos >= pStack ){` |
|     1315 |  9566 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9567 | `		pTos--;` |
|        1 |  9568 | `	}` |
|      561 |  9569 | `	return PH7_ABORT;` |
|       27 |  9570 | `Exception:` |
|       56 |  9571 | `	SySetRelease(&aArg);` |
|      104 |  9572 | `	while( pTos >= pStack ){` |
|       50 |  9573 | `		PH7_MemObjRelease(pTos);` |
|       50 |  9574 | `		pTos--;` |
|        2 |  9575 | `	}` |
|       56 |  9576 | `	return PH7_EXCEPTION;` |
|    22322 |  9577 |  |
|        - |  9578 | `/*` |
|        - |  9579 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9580 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9581 | ` * See block-comment on that function for additional information.` |
|        - |  9582 | ` */` |
|    20336 |  9583 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9584 |  |
|        - |  9585 | `	ph7_value *pStack;` |
|        - |  9586 | `	sxi32 rc;` |
|        - |  9587 | `	/* Allocate a new operand stack */` |
|    20338 |  9588 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20338 |  9589 | `	if( pStack == 0 ){` |
|      ! 0 |  9590 | `		return SXERR_MEM;` |
|        - |  9591 | `	}` |
|        - |  9592 | `	/* Execute the program */` |
|    20338 |  9593 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9594 | `	/* Free the operand stack */` |
|    20338 |  9595 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9596 | `	/* Execution result */` |
|    20338 |  9597 | `	return rc;` |
|    10170 |  9598 |  |
|        - |  9599 | `/*` |
|        - |  9600 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9601 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9602 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9603 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9604 | ` * execution ends.` |
|        - |  9605 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9606 | ` * additional information.` |
|        - |  9607 | ` */` |
|     2820 |  9608 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9609 |  |
|        - |  9610 | `	VmShutdownCB *pEntry;` |
|        - |  9611 | `	ph7_value *apArg[10];` |
|        - |  9612 | `	sxu32 n,nEntry;` |
|        - |  9613 | `	int i;` |
|        - |  9614 | `	/* Point to the stack of registered callbacks */` |
|     2822 |  9615 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31022 |  9616 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28202 |  9617 | `		apArg[i] = 0;` |
|    14102 |  9618 | `	}` |
|        - |  9619 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - |  9620 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - |  9621 | `	 * callbacks, mirroring PHP.` |
|        - |  9622 | `	 */` |
|     2822 |  9623 | `	pVm->bHaltRequested = 0;` |
|     2832 |  9624 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 |  9625 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9626 | `		if( pEntry ){` |
|        - |  9627 | `			/* Prepare callback arguments if any */` |
|       12 |  9628 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9629 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9630 | `					break;` |
|        - |  9631 | `				}` |
|      ! 0 |  9632 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9633 | `			}` |
|        - |  9634 | `			/* Invoke the callback */` |
|       12 |  9635 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9636 | `			/*` |
|        - |  9637 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9638 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9639 | `			 */` |
|       12 |  9640 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9641 | `			if( pEntry ){` |
|       12 |  9642 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 |  9643 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9644 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9645 | `				}` |
|        5 |  9646 | `			}` |
|       12 |  9647 | `			if( pVm->bHaltRequested ){` |
|        - |  9648 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 |  9649 | `				break;` |
|        - |  9650 | `			}` |
|        5 |  9651 | `		}` |
|        7 |  9652 | `	}` |
|     2822 |  9653 | `	SySetReset(&pVm->aShutdown);` |
|     2822 |  9654 |  |
|        - |  9655 | `/*` |
|        - |  9656 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9657 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9658 | ` * See block-comment on that function for additional information.` |
|        - |  9659 | ` */` |
|     2820 |  9660 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9661 |  |
|        - |  9662 | `	/* Make sure we are ready to execute this program */` |
|     2822 |  9663 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9664 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9665 | `	}` |
|        - |  9666 | `	/* Set the execution magic number  */` |
|     2822 |  9667 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9668 | `	/* Execute the program */` |
|     2822 |  9669 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9670 | `	/* Invoke any shutdown callbacks */` |
|     2822 |  9671 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9672 | `	/*` |
|        - |  9673 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9674 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9675 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9676 | `	 */` |
|     2822 |  9677 | `	return SXRET_OK;` |
|     1412 |  9678 |  |
|        - |  9679 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9680 | `/*` |
|        - |  9681 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9682 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9683 | ` */` |
|       46 |  9684 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9685 |  |
|        - |  9686 | `	ph7_exec_ctx *pCtx;` |
|        - |  9687 | `	ph7_value *pStack;` |
|        - |  9688 | `	VmFrame *pFrame;` |
|       48 |  9689 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9690 | `	if( pCtx == 0 ){` |
|      ! 0 |  9691 | `		return 0;` |
|        - |  9692 | `	}` |
|       48 |  9693 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9694 | `	pCtx->pVm = pVm;` |
|       48 |  9695 | `	pCtx->pFunc = pFunc;` |
|       48 |  9696 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9697 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9698 | `	pCtx->pc = 0;` |
|       48 |  9699 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9700 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9701 | `	/* Allocate a private operand stack */` |
|       48 |  9702 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9703 | `	if( pStack == 0 ){` |
|      ! 0 |  9704 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9705 | `		return 0;` |
|        - |  9706 | `	}` |
|       48 |  9707 | `	pCtx->pStack = pStack;` |
|        - |  9708 | `	/* Create a detached frame for the fiber */` |
|       48 |  9709 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9710 | `	if( pFrame == 0 ){` |
|      ! 0 |  9711 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9712 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9713 | `		return 0;` |
|        - |  9714 | `	}` |
|       48 |  9715 | `	pCtx->pFrame = pFrame;` |
|       48 |  9716 | `	return pCtx;` |
|       25 |  9717 |  |
|        - |  9718 | `/*` |
|        - |  9719 | ` * Start executing a fiber context for the first time.` |
|        - |  9720 | ` */` |
|       46 |  9721 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9722 |  |
|        - |  9723 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9724 | `	sxi32 rc;` |
|       48 |  9725 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9726 | `		return SXERR_INVALID;` |
|        - |  9727 | `	}` |
|        - |  9728 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9729 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9730 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9731 | `	/* Save and set the active context */` |
|       48 |  9732 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9733 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9734 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9735 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9736 | `	pVm->nRecursionDepth++;` |
|        - |  9737 | `	/* Execute from the beginning */` |
|       48 |  9738 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9739 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9740 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9741 | `	pVm->nRecursionDepth--;` |
|        - |  9742 | `	/* Restore the previous context */` |
|       48 |  9743 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9744 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9745 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9746 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9747 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9748 | `		if( pResult ){` |
|       24 |  9749 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9750 | `		}` |
|       46 |  9751 | `		return SXRET_OK;` |
|        - |  9752 | `	}` |
|        - |  9753 | `	/* Detach frame */` |
|        3 |  9754 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9755 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9756 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9757 | `	}` |
|        3 |  9758 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9759 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9760 | `		return PH7_ABORT;` |
|        - |  9761 | `	}` |
|        3 |  9762 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9763 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9764 | `		return PH7_EXCEPTION;` |
|        - |  9765 | `	}` |
|        - |  9766 | `	/* Normal completion */` |
|        3 |  9767 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9768 | `	if( pResult ){` |
|        3 |  9769 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9770 | `	}` |
|        3 |  9771 | `	return SXRET_OK;` |
|       25 |  9772 |  |
|        - |  9773 | `/*` |
|        - |  9774 | ` * Resume a suspended fiber context.` |
|        - |  9775 | ` */` |
|       98 |  9776 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9777 |  |
|        - |  9778 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9779 | `	sxi32 rc;` |
|      100 |  9780 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9781 | `		return SXERR_INVALID;` |
|        - |  9782 | `	}` |
|        - |  9783 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9784 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9785 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9786 | `	if( pResumeValue ){` |
|       40 |  9787 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9788 | `	}else{` |
|       62 |  9789 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9790 | `	}` |
|      100 |  9791 | `	pCtx->nTos++;` |
|        - |  9792 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9793 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9794 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9795 | `	/* Save and set the active context */` |
|      100 |  9796 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9797 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9798 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9799 | `	pVm->nRecursionDepth++;` |
|        - |  9800 | `	/* Resume execution from saved PC */` |
|      100 |  9801 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9802 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9803 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9804 | `	pVm->nRecursionDepth--;` |
|        - |  9805 | `	/* Restore the previous context */` |
|      100 |  9806 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9807 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9808 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9809 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9810 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9811 | `		if( pResult ){` |
|       18 |  9812 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9813 | `		}` |
|       64 |  9814 | `		return SXRET_OK;` |
|        - |  9815 | `	}` |
|        - |  9816 | `	/* Detach frame */` |
|       38 |  9817 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9818 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9819 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9820 | `	}` |
|       38 |  9821 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9822 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9823 | `		return PH7_ABORT;` |
|        - |  9824 | `	}` |
|       38 |  9825 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9826 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9827 | `		return PH7_EXCEPTION;` |
|        - |  9828 | `	}` |
|        - |  9829 | `	/* Normal completion */` |
|       38 |  9830 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9831 | `	if( pResult ){` |
|       20 |  9832 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9833 | `	}` |
|       38 |  9834 | `	return SXRET_OK;` |
|       51 |  9835 |  |
|        - |  9836 | `/*` |
|        - |  9837 | ` * Release an execution context and all its resources.` |
|        - |  9838 | ` */` |
|        4 |  9839 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9840 |  |
|        5 |  9841 | `	if( pCtx == 0 ){` |
|      ! 0 |  9842 | `		return;` |
|        - |  9843 | `	}` |
|        5 |  9844 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9845 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9846 | `		return;` |
|        - |  9847 | `	}` |
|        5 |  9848 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9849 | `	/* Release values */` |
|        5 |  9850 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9851 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9852 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9853 | `	if( pCtx->pFrame ){` |
|        - |  9854 | `		VmSlot *aSlot;` |
|        - |  9855 | `		sxu32 n;` |
|        - |  9856 | `		/* Free local variables */` |
|        5 |  9857 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9858 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9859 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9860 | `		}` |
|        - |  9861 | `		/* Remove local references */` |
|        5 |  9862 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9863 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9864 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9865 | `		}` |
|        5 |  9866 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9867 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9868 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9869 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9870 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9871 | `		pCtx->pFrame = 0;` |
|        2 |  9872 | `	}` |
|        - |  9873 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9874 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9875 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9876 | `	if( pCtx->pStack ){` |
|        5 |  9877 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9878 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9879 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9880 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9881 | `				pTos--;` |
|        1 |  9882 | `			}` |
|        2 |  9883 | `		}` |
|        5 |  9884 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9885 | `		pCtx->pStack = 0;` |
|        2 |  9886 | `	}` |
|        - |  9887 | `	/* Free the context itself */` |
|        5 |  9888 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9889 |  |
|        - |  9890 | `/*` |
|        - |  9891 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9892 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9893 | ` */` |
|       90 |  9894 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9895 |  |
|        - |  9896 | `	ph7_class_instance *pThis;` |
|        - |  9897 | `	SyString sAttr;` |
|        - |  9898 | `	ph7_value *pAttr;` |
|       92 |  9899 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9900 | `		return 0;` |
|        - |  9901 | `	}` |
|       92 |  9902 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9903 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9904 | `		return 0;` |
|        - |  9905 | `	}` |
|       92 |  9906 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9907 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9908 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9909 | `		return 0;` |
|        - |  9910 | `	}` |
|       62 |  9911 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9912 |  |
|        - |  9913 | `/*` |
|        - |  9914 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9915 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9916 | ` */` |
|       38 |  9917 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9918 |  |
|       40 |  9919 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9920 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9921 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9922 | `			"Cannot suspend outside of a fiber");` |
|        - |  9923 | `	}` |
|       40 |  9924 | `	if( nArg > 0 ){` |
|       40 |  9925 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9926 | `	}else{` |
|      ! 0 |  9927 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9928 | `	}` |
|       40 |  9929 | `	return PH7_SUSPEND;` |
|       21 |  9930 |  |
|        - |  9931 | `/*` |
|        - |  9932 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9933 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9934 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9935 | ` */` |
|       24 |  9936 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9937 |  |
|        - |  9938 | `	ph7_class_instance *pThis;` |
|        - |  9939 | `	ph7_value *pAttr;` |
|        - |  9940 | `	SyString sAttrName;` |
|       26 |  9941 | `	if( nArg < 2 ){` |
|      ! 0 |  9942 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9943 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9944 | `	}` |
|       26 |  9945 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9946 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9947 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9948 | `	}` |
|       26 |  9949 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9950 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9951 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9952 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9953 | `	}` |
|        - |  9954 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9955 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9956 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9957 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9958 | `	}` |
|        - |  9959 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9960 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9961 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9962 | `	if( pAttr ){` |
|       26 |  9963 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9964 | `	}` |
|       26 |  9965 | `	return PH7_OK;` |
|       14 |  9966 |  |
|        - |  9967 | `/*` |
|        - |  9968 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9969 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9970 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9971 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9972 | ` */` |
|       24 |  9973 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9974 | `	ph7_class_instance **ppThis)` |
|        2 |  9975 |  |
|       26 |  9976 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9977 | `	ph7_value *pCallable;` |
|        - |  9978 | `	SyString sAttrName;` |
|       26 |  9979 | `	*ppThis = 0;` |
|       26 |  9980 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9981 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9982 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9983 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9984 | `		return 0;` |
|        - |  9985 | `	}` |
|       26 |  9986 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9987 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9988 | `		SyString sName;` |
|        - |  9989 | `		SyHashEntry *pEntry;` |
|        - |  9990 | `		ph7_vm_func *pFunc;` |
|       26 |  9991 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9992 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9993 | `		if( pEntry == 0 ){` |
|      ! 0 |  9994 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9995 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9996 | `			return 0;` |
|        - |  9997 | `		}` |
|       26 |  9998 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9999 | `		return pFunc;` |
|      ! 0 | 10000 | `	}else{` |
|        - | 10001 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10002 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10003 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10004 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10005 | `		if( pMethod == 0 ){` |
|      ! 0 | 10006 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10007 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10008 | `			return 0;` |
|        - | 10009 | `		}` |
|      ! 0 | 10010 | `		*ppThis = pClosure;` |
|      ! 0 | 10011 | `		return &pMethod->sFunc;` |
|        - | 10012 | `	}` |
|       14 | 10013 |  |
|        - | 10014 | `/*` |
|        - | 10015 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10016 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10017 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10018 | ` */` |
|       46 | 10019 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10020 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10021 |  |
|       48 | 10022 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10023 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10024 | `	sxu32 nFormal, n;` |
|        - | 10025 | `	VmSlot sSlot;` |
|        - | 10026 | `	sxi32 rc;` |
|        - | 10027 | `	/* Install $this for closure/method callables */` |
|       48 | 10028 | `	if( pClosureThis ){` |
|        - | 10029 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10030 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10031 | `		if( pObj ){` |
|      ! 0 | 10032 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10033 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10034 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10035 | `		}` |
|      ! 0 | 10036 | `	}` |
|        - | 10037 | `	/* Install static variables */` |
|       48 | 10038 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10039 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10040 | `		ph7_value *pVal;` |
|      ! 0 | 10041 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10042 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10043 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10044 | `			if( pVal ){` |
|      ! 0 | 10045 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10046 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10047 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10048 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10049 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10050 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10051 | `				}` |
|      ! 0 | 10052 | `			}` |
|      ! 0 | 10053 | `		}` |
|      ! 0 | 10054 | `	}` |
|        - | 10055 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10056 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10057 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10058 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10059 | `		ph7_value *pObj;` |
|       20 | 10060 | `		if( n < (sxu32)nArg ){` |
|        - | 10061 | `			/* Argument provided — install with type casting */` |
|       20 | 10062 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10063 | `			if( pObj ){` |
|       20 | 10064 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10065 | `				/* Type casting */` |
|       20 | 10066 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10067 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10068 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10069 | `						if( xCast ){` |
|      ! 0 | 10070 | `							xCast(pObj);` |
|      ! 0 | 10071 | `						}` |
|      ! 0 | 10072 | `					}` |
|      ! 0 | 10073 | `				}` |
|       20 | 10074 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10075 | `				sSlot.pUserData = 0;` |
|       20 | 10076 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10077 | `			}` |
|        9 | 10078 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10079 | `			/* Default value */` |
|      ! 0 | 10080 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10081 | `			if( pObj ){` |
|      ! 0 | 10082 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10083 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10084 | `					return rc;` |
|        - | 10085 | `				}` |
|      ! 0 | 10086 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10087 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10088 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10089 | `						if( xCast ){` |
|      ! 0 | 10090 | `							xCast(pObj);` |
|      ! 0 | 10091 | `						}` |
|      ! 0 | 10092 | `					}` |
|      ! 0 | 10093 | `				}` |
|      ! 0 | 10094 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10095 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10096 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10097 | `			}` |
|      ! 0 | 10098 | `		}` |
|       11 | 10099 | `	}` |
|        - | 10100 | `	/* Install closure environment (captured variables) */` |
|       48 | 10101 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10102 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10103 | `		ph7_value *pValue;` |
|        - | 10104 | `		sxu32 iEnv;` |
|        3 | 10105 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10106 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10107 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10108 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10109 | `				continue;` |
|        - | 10110 | `			}` |
|        5 | 10111 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10112 | `			if( pValue == 0 ){` |
|      ! 0 | 10113 | `				continue;` |
|        - | 10114 | `			}` |
|        5 | 10115 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10116 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10117 | `		}` |
|        1 | 10118 | `	}` |
|       48 | 10119 | `	return SXRET_OK;` |
|       25 | 10120 |  |
|        - | 10121 | `/*` |
|        - | 10122 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10123 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10124 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10125 | ` */` |
|       26 | 10126 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10127 |  |
|       28 | 10128 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10129 | `	ph7_class_instance *pThis;` |
|        - | 10130 | `	ph7_class_instance *pClosureThis;` |
|        - | 10131 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10132 | `	ph7_vm_func *pFunc;` |
|        - | 10133 | `	ph7_value sResult;` |
|        - | 10134 | `	ph7_value *pCtxAttr;` |
|        - | 10135 | `	SyString sAttrName;` |
|        - | 10136 | `	sxi32 rc;` |
|       28 | 10137 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10138 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10139 | `	}` |
|       28 | 10140 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10141 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10142 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10143 | `	if( pExecCtx != 0 ){` |
|        3 | 10144 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10145 | `			"Cannot start a fiber that has already been started");` |
|        - | 10146 | `	}` |
|        - | 10147 | `	/* Resolve callable */` |
|       26 | 10148 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10149 | `	if( pFunc == 0 ){` |
|      ! 0 | 10150 | `		return PH7_EXCEPTION;` |
|        - | 10151 | `	}` |
|        - | 10152 | `	/* Create execution context now that we know the function */` |
|       26 | 10153 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10154 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10155 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10156 | `			"Fiber::start(): out of memory");` |
|        - | 10157 | `	}` |
|        - | 10158 | `	/* Store context in $this->__ctx */` |
|       26 | 10159 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10160 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10161 | `	if( pCtxAttr ){` |
|       26 | 10162 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10163 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10164 | `	}` |
|        - | 10165 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10166 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10167 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10168 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10169 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10170 | `	/* Unpack the args array and install into the frame */` |
|        - | 10171 | `	{` |
|       26 | 10172 | `		ph7_value **apValues = 0;` |
|       26 | 10173 | `		int nActual = 0;` |
|       26 | 10174 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10175 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10176 | `			ph7_hashmap_node *pNode;` |
|       26 | 10177 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10178 | `			if( nCount > 0 ){` |
|        3 | 10179 | `				sxu32 idx = 0;` |
|        4 | 10180 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10181 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10182 | `				if( apValues ){` |
|        3 | 10183 | `					pNode = pMap->pFirst;` |
|        7 | 10184 | `					while( pNode && idx < nCount ){` |
|        5 | 10185 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10186 | `						idx++;` |
|        5 | 10187 | `						pNode = pNode->pPrev;` |
|        1 | 10188 | `					}` |
|        3 | 10189 | `					nActual = (int)idx;` |
|        1 | 10190 | `				}` |
|        1 | 10191 | `			}` |
|       12 | 10192 | `		}` |
|       26 | 10193 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10194 | `		if( apValues ){` |
|        3 | 10195 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10196 | `		}` |
|        - | 10197 | `	}` |
|        - | 10198 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10199 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10200 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10201 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10202 | `		return PH7_ABORT;` |
|        - | 10203 | `	}` |
|       26 | 10204 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10205 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10206 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10207 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10208 | `		return PH7_ABORT;` |
|        - | 10209 | `	}` |
|       26 | 10210 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10211 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10212 | `		return PH7_EXCEPTION;` |
|        - | 10213 | `	}` |
|       26 | 10214 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10215 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10216 | `	return PH7_OK;` |
|       15 | 10217 |  |
|        - | 10218 | `/*` |
|        - | 10219 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10220 | ` */` |
|       36 | 10221 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10222 |  |
|       38 | 10223 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10224 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10225 | `	ph7_value sResult;` |
|        - | 10226 | `	ph7_value *pResumeVal;` |
|        - | 10227 | `	sxi32 rc;` |
|       38 | 10228 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10229 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10230 | `		return PH7_OK;` |
|        - | 10231 | `	}` |
|       38 | 10232 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10233 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10234 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10235 | `		return PH7_OK;` |
|        - | 10236 | `	}` |
|       38 | 10237 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10238 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10239 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10240 | `	}` |
|       36 | 10241 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10242 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10243 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10244 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10245 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10246 | `		return PH7_ABORT;` |
|        - | 10247 | `	}` |
|       36 | 10248 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10249 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10250 | `		return PH7_EXCEPTION;` |
|        - | 10251 | `	}` |
|       36 | 10252 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10253 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10254 | `	return PH7_OK;` |
|       20 | 10255 |  |
|        - | 10256 | `/*` |
|        - | 10257 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10258 | ` */` |
|        6 | 10259 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10260 |  |
|        8 | 10261 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10262 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10263 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10264 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10265 | `		return PH7_OK;` |
|        - | 10266 | `	}` |
|        8 | 10267 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10268 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10269 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10270 | `		return PH7_OK;` |
|        - | 10271 | `	}` |
|        8 | 10272 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10273 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10274 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10275 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10276 | `		}` |
|      ! 0 | 10277 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10278 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10279 | `	}` |
|        8 | 10280 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10281 | `	return PH7_OK;` |
|        5 | 10282 |  |
|        - | 10283 | `/*` |
|        - | 10284 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10285 | ` */` |
|        6 | 10286 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10287 |  |
|        - | 10288 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10289 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10290 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10291 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10292 | `	return PH7_OK;` |
|        4 | 10293 |  |
|      ! 0 | 10294 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10295 |  |
|        - | 10296 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10297 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10298 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10299 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10300 | `	return PH7_OK;` |
|      ! 0 | 10301 |  |
|        6 | 10302 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10303 |  |
|        - | 10304 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10305 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10306 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10307 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10308 | `	return PH7_OK;` |
|        4 | 10309 |  |
|        6 | 10310 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10311 |  |
|        - | 10312 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10313 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10314 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10315 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10316 | `	return PH7_OK;` |
|        4 | 10317 |  |
|        - | 10318 | `/*` |
|        - | 10319 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10320 | ` */` |
|        4 | 10321 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10322 |  |
|        5 | 10323 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10324 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10325 | `	if( nArg < 1 ){` |
|      ! 0 | 10326 | `		return PH7_OK;` |
|        - | 10327 | `	}` |
|        5 | 10328 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10329 | `	if( pExecCtx ){` |
|        5 | 10330 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10331 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10332 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10333 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10334 | `			SyString sAttrName;` |
|        - | 10335 | `			ph7_value *pAttr;` |
|        5 | 10336 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10337 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10338 | `			if( pAttr ){` |
|        5 | 10339 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10340 | `			}` |
|        2 | 10341 | `		}` |
|        2 | 10342 | `	}` |
|        5 | 10343 | `	return PH7_OK;` |
|        3 | 10344 |  |
|        - | 10345 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10346 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10347 |  |
|        - | 10348 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10349 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10350 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10351 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10352 |  |
|      ! 0 | 10353 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10354 |  |
|        - | 10355 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10356 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10357 | `	ph7_exec_ctx *pCtx;` |
|        - | 10358 | `	ph7_vm_func *pFunc;` |
|        - | 10359 | `	ph7_value *pCallable;` |
|        - | 10360 | `	ph7_value *pCtxAttr;` |
|        - | 10361 | `	SyString sAttrName;` |
|        - | 10362 | `	/* Must not already be started */` |
|      ! 0 | 10363 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10364 | `	if( pCtx != 0 ){` |
|      ! 0 | 10365 | `		return SXERR_INVALID;` |
|        - | 10366 | `	}` |
|      ! 0 | 10367 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10368 | `		return SXERR_INVALID;` |
|        - | 10369 | `	}` |
|      ! 0 | 10370 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10371 | `	/* Get the callable */` |
|      ! 0 | 10372 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10373 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10374 | `	if( pCallable == 0 ){` |
|      ! 0 | 10375 | `		return SXERR_INVALID;` |
|        - | 10376 | `	}` |
|        - | 10377 | `	/* Resolve callable */` |
|      ! 0 | 10378 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10379 | `		SyString sName;` |
|        - | 10380 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10381 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10382 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10383 | `		if( pEntry == 0 ){` |
|      ! 0 | 10384 | `			return SXERR_NOTFOUND;` |
|        - | 10385 | `		}` |
|      ! 0 | 10386 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10387 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10388 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10389 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10390 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10391 | `		if( pMethod == 0 ){` |
|      ! 0 | 10392 | `			return SXERR_INVALID;` |
|        - | 10393 | `		}` |
|      ! 0 | 10394 | `		pClosureThis = pClosure;` |
|      ! 0 | 10395 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10396 | `	}else{` |
|      ! 0 | 10397 | `		return SXERR_INVALID;` |
|        - | 10398 | `	}` |
|        - | 10399 | `	/* Create context */` |
|      ! 0 | 10400 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10401 | `	if( pCtx == 0 ){` |
|      ! 0 | 10402 | `		return SXERR_MEM;` |
|        - | 10403 | `	}` |
|        - | 10404 | `	/* Store in __ctx */` |
|      ! 0 | 10405 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10406 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10407 | `	if( pCtxAttr ){` |
|      ! 0 | 10408 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10409 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10410 | `	}` |
|        - | 10411 | `	/* Set up frame with args */` |
|      ! 0 | 10412 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10413 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10414 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10415 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10416 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10417 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10418 |  |
|      ! 0 | 10419 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10420 |  |
|      ! 0 | 10421 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10422 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10423 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10424 |  |
|      ! 0 | 10425 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10426 |  |
|      ! 0 | 10427 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10428 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10429 |  |
|      ! 0 | 10430 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10431 |  |
|      ! 0 | 10432 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10433 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10434 |  |
|      ! 0 | 10435 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10436 |  |
|      ! 0 | 10437 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10438 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10439 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10440 |  |
|        - | 10441 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10442 | `/*` |
|        - | 10443 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10444 | ` */` |
|       22 | 10445 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10446 |  |
|        - | 10447 | `	ph7_generator *pGen;` |
|       24 | 10448 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10449 | `	if( pGen == 0 ){` |
|      ! 0 | 10450 | `		return 0;` |
|        - | 10451 | `	}` |
|       24 | 10452 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10453 | `	pGen->pCtx = pCtx;` |
|       24 | 10454 | `	pGen->iImplicitKey = 0;` |
|       24 | 10455 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10456 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10457 | `	/* Link the generator back to the exec context */` |
|       24 | 10458 | `	pCtx->pPrivate = pGen;` |
|       24 | 10459 | `	return pGen;` |
|       13 | 10460 |  |
|        - | 10461 | `/*` |
|        - | 10462 | ` * Release a generator and its execution context.` |
|        - | 10463 | ` */` |
|      ! 0 | 10464 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10465 |  |
|      ! 0 | 10466 | `	if( pGen == 0 ){` |
|      ! 0 | 10467 | `		return;` |
|        - | 10468 | `	}` |
|      ! 0 | 10469 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10470 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10471 | `	if( pGen->pCtx ){` |
|      ! 0 | 10472 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10473 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10474 | `		pGen->pCtx = 0;` |
|      ! 0 | 10475 | `	}` |
|      ! 0 | 10476 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10477 |  |
|        - | 10478 | `/*` |
|        - | 10479 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10480 | ` */` |
|      236 | 10481 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10482 |  |
|        - | 10483 | `	ph7_class_instance *pThis;` |
|        - | 10484 | `	SyString sAttr;` |
|        - | 10485 | `	ph7_value *pAttr;` |
|      238 | 10486 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10487 | `		return 0;` |
|        - | 10488 | `	}` |
|      238 | 10489 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10490 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10491 | `		return 0;` |
|        - | 10492 | `	}` |
|      238 | 10493 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10494 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10495 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10496 | `		return 0;` |
|        - | 10497 | `	}` |
|      238 | 10498 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10499 |  |
|        - | 10500 | `/*` |
|        - | 10501 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10502 | ` */` |
|       22 | 10503 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10504 |  |
|        - | 10505 | `	ph7_generator *pGen;` |
|        - | 10506 | `	sxi32 rc;` |
|       24 | 10507 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10508 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10509 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10510 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10511 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10512 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10513 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10514 | `	}` |
|       24 | 10515 | `	return PH7_OK;` |
|       13 | 10516 |  |
|        - | 10517 | `/*` |
|        - | 10518 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10519 | ` */` |
|       68 | 10520 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10521 |  |
|        - | 10522 | `	ph7_generator *pGen;` |
|       70 | 10523 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10524 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10525 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10526 | `	return PH7_OK;` |
|       36 | 10527 |  |
|        - | 10528 | `/*` |
|        - | 10529 | ` * Generator::current() — return the last yielded value.` |
|        - | 10530 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10531 | ` */` |
|       68 | 10532 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10533 |  |
|        - | 10534 | `	ph7_generator *pGen;` |
|        - | 10535 | `	sxi32 rc;` |
|       70 | 10536 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10537 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10538 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10539 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10540 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10541 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10542 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10543 | `	}` |
|       70 | 10544 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10545 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10546 | `	}else{` |
|      ! 0 | 10547 | `		ph7_result_null(pCtx);` |
|        - | 10548 | `	}` |
|       70 | 10549 | `	return PH7_OK;` |
|       36 | 10550 |  |
|        - | 10551 | `/*` |
|        - | 10552 | ` * Generator::key() — return the last yielded key.` |
|        - | 10553 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10554 | ` */` |
|       12 | 10555 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10556 |  |
|        - | 10557 | `	ph7_generator *pGen;` |
|        - | 10558 | `	sxi32 rc;` |
|       13 | 10559 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10560 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10561 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10562 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10563 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10564 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10565 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10566 | `	}` |
|       13 | 10567 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10568 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10569 | `	}else{` |
|      ! 0 | 10570 | `		ph7_result_null(pCtx);` |
|        - | 10571 | `	}` |
|       13 | 10572 | `	return PH7_OK;` |
|        7 | 10573 |  |
|        - | 10574 | `/*` |
|        - | 10575 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10576 | ` */` |
|       60 | 10577 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10578 |  |
|        - | 10579 | `	ph7_generator *pGen;` |
|        - | 10580 | `	sxi32 rc;` |
|       62 | 10581 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10582 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10583 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10584 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10585 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10586 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10587 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10588 | `	}else{` |
|      ! 0 | 10589 | `		return PH7_OK;` |
|        - | 10590 | `	}` |
|       62 | 10591 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10592 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10593 | `	return PH7_OK;` |
|       32 | 10594 |  |
|        - | 10595 | `/*` |
|        - | 10596 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10597 | ` */` |
|        4 | 10598 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10599 |  |
|        - | 10600 | `	ph7_generator *pGen;` |
|        - | 10601 | `	ph7_value *pSendVal;` |
|        - | 10602 | `	sxi32 rc;` |
|        5 | 10603 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10604 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10605 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10606 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10607 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10608 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10609 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10610 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10611 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10612 | `	}else{` |
|      ! 0 | 10613 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10614 | `		return PH7_OK;` |
|        - | 10615 | `	}` |
|        5 | 10616 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10617 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10618 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10619 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10620 | `	}else{` |
|        3 | 10621 | `		ph7_result_null(pCtx);` |
|        - | 10622 | `	}` |
|        5 | 10623 | `	return PH7_OK;` |
|        3 | 10624 |  |
|        - | 10625 | `/*` |
|        - | 10626 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10627 | ` *` |
|        - | 10628 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10629 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10630 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10631 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10632 | ` * the exception to the caller.` |
|        - | 10633 | ` */` |
|      ! 0 | 10634 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10635 |  |
|        - | 10636 | `	ph7_generator *pGen;` |
|        - | 10637 | `	const char *zMsg;` |
|        - | 10638 | `	int nLen;` |
|      ! 0 | 10639 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10640 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10641 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10642 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10643 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10644 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10645 | `			"Cannot throw into a closed generator");` |
|        - | 10646 | `	}` |
|        - | 10647 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10648 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10649 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10650 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10651 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10652 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10653 | `	nLen = 0;` |
|      ! 0 | 10654 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10655 | `		/* Try to get the exception's message */` |
|        - | 10656 | `		SyString sAttr;` |
|        - | 10657 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10658 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10659 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10660 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10661 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10662 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10663 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10664 | `		}` |
|      ! 0 | 10665 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10666 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10667 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10668 | `	}` |
|      ! 0 | 10669 | `	(void)nLen;` |
|      ! 0 | 10670 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10671 |  |
|        - | 10672 | `/*` |
|        - | 10673 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10674 | ` */` |
|        2 | 10675 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10676 |  |
|        - | 10677 | `	ph7_generator *pGen;` |
|        3 | 10678 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10679 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10680 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10681 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10682 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10683 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10684 | `	}` |
|        3 | 10685 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10686 | `	return PH7_OK;` |
|        2 | 10687 |  |
|        - | 10688 | `/*` |
|        - | 10689 | ` * Generator::__destruct() — clean up.` |
|        - | 10690 | ` */` |
|      ! 0 | 10691 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10692 |  |
|        - | 10693 | `	ph7_generator *pGen;` |
|      ! 0 | 10694 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10695 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10696 | `	if( pGen ){` |
|      ! 0 | 10697 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10698 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10699 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10700 | `			SyString sAttrName;` |
|        - | 10701 | `			ph7_value *pAttr;` |
|      ! 0 | 10702 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10703 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10704 | `			if( pAttr ){` |
|      ! 0 | 10705 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10706 | `			}` |
|      ! 0 | 10707 | `		}` |
|      ! 0 | 10708 | `	}` |
|      ! 0 | 10709 | `	return PH7_OK;` |
|      ! 0 | 10710 |  |
|        - | 10711 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10712 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10713 | `/*` |
|        - | 10714 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10715 | ` * the desired message.` |
|        - | 10716 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10717 | ` * in 'api.c' for additional information.` |
|        - | 10718 | ` */` |
|      370 | 10719 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10720 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10721 | `	SyString *pString /* Message to output */` |
|        - | 10722 | `	)` |
|        2 | 10723 |  |
|      372 | 10724 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10725 | `	sxi32 rc = SXRET_OK;` |
|        - | 10726 | `	/* Call the output consumer */` |
|      372 | 10727 | `	if( pString->nByte > 0 ){` |
|      372 | 10728 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10729 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10730 | `	}` |
|      372 | 10731 | `	return rc;` |
|        2 | 10732 |  |
|        - | 10733 | `/*` |
|        - | 10734 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10735 | ` * callback to consume the formatted message.` |
|        - | 10736 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10737 | ` * in 'api.c' for additional information.` |
|        - | 10738 | ` */` |
|        2 | 10739 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10740 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10741 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10742 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10743 | `	)` |
|        1 | 10744 |  |
|        3 | 10745 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10746 | `	sxi32 rc = SXRET_OK;` |
|        - | 10747 | `	SyBlob sWorker;` |
|        - | 10748 | `	/* Format the message and call the output consumer */` |
|        3 | 10749 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10750 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10751 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10752 | `		/* Consume the formatted message */` |
|        3 | 10753 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10754 | `	}` |
|        3 | 10755 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10756 | `	/* Release the working buffer */` |
|        3 | 10757 | `	SyBlobRelease(&sWorker);` |
|        3 | 10758 | `	return rc;` |
|        1 | 10759 |  |
|        - | 10760 | `/*` |
|        - | 10761 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10762 | ` * This function never fail and always return a pointer` |
|        - | 10763 | ` * to a null terminated string.` |
|        - | 10764 | ` */` |
|       12 | 10765 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10766 |  |
|       13 | 10767 | `	const char *zOp = "Unknown     ";` |
|       13 | 10768 | `	switch(nOp){` |
|        3 | 10769 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10770 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10771 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10772 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10773 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10774 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10775 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10776 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10777 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10778 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10779 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10780 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10781 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10782 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10783 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10784 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10785 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10786 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10787 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10788 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10789 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10790 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10791 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10792 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10793 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10794 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10795 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10796 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10797 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10798 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10799 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10800 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10801 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10802 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10803 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10804 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10805 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10806 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10807 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10808 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10809 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10810 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10811 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10812 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10813 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10814 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10815 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10816 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10817 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10818 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10819 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10820 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10821 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10822 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10823 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10824 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10825 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10826 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10827 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10828 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10829 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10830 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10831 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10832 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10833 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10834 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10835 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10836 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10837 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10838 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10839 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10840 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10841 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10842 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10843 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10844 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10845 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10846 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10847 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10848 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10849 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10850 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10851 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10852 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10853 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10854 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10855 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10856 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10857 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10858 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10859 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10860 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10861 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10862 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10863 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10864 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10865 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10866 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10867 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10868 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10869 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10870 | `	default:` |
|      ! 0 | 10871 | `		break;` |
|        - | 10872 | `	}` |
|       13 | 10873 | `	return zOp;` |
|        1 | 10874 |  |
|        - | 10875 | `/*` |
|        - | 10876 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10877 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10878 | ` * is responsible of consuming the generated dump.` |
|        - | 10879 | ` */` |
|        2 | 10880 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10881 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10882 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10883 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10884 | `	)` |
|        1 | 10885 |  |
|        - | 10886 | `	sxi32 rc;` |
|        3 | 10887 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10888 | `	return rc;` |
|        1 | 10889 |  |
|        - | 10890 | `/*` |
|        - | 10891 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10892 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10893 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10894 | ` * in 'compile.c' for additional information.` |
|        - | 10895 | ` */` |
|       14 | 10896 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10897 |  |
|       15 | 10898 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10899 | `	/* Evaluate and expand constant value */` |
|       15 | 10900 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10901 |  |
|        - | 10902 | `/*` |
|        - | 10903 | ` * Section:` |
|        - | 10904 | ` *  Function handling functions.` |
|        - | 10905 | ` * Status:` |
|        - | 10906 | ` *    Stable.` |
|        - | 10907 | ` */` |
|        - | 10908 | `/*` |
|        - | 10909 | ` * int func_num_args(void)` |
|        - | 10910 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10911 | ` * Parameters` |
|        - | 10912 | ` *   None.` |
|        - | 10913 | ` * Return` |
|        - | 10914 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10915 | ` *  or -1 if called from the globe scope.` |
|        - | 10916 | ` */` |
|      976 | 10917 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10918 |  |
|        - | 10919 | `	VmFrame *pFrame;` |
|        - | 10920 | `	ph7_vm *pVm;` |
|        - | 10921 | `	/* Point to the target VM */` |
|      978 | 10922 | `	pVm = pCtx->pVm;` |
|        - | 10923 | `	/* Current frame */` |
|      978 | 10924 | `	pFrame = pVm->pFrame;` |
|      978 | 10925 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      978 | 10926 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10927 | `		SXUNUSED(nArg);` |
|      ! 0 | 10928 | `		SXUNUSED(apArg);` |
|        - | 10929 | `		/* Global frame,return -1 */` |
|      ! 0 | 10930 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10931 | `		return SXRET_OK;` |
|        - | 10932 | `	}` |
|        - | 10933 | `	/* Total number of arguments passed to the enclosing function */` |
|      978 | 10934 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      978 | 10935 | `	ph7_result_int(pCtx,nArg);` |
|      978 | 10936 | `	return SXRET_OK;` |
|      490 | 10937 |  |
|        - | 10938 | `/*` |
|        - | 10939 | ` * value func_get_arg(int $arg_num)` |
|        - | 10940 | ` *   Return an item from the argument list.` |
|        - | 10941 | ` * Parameters` |
|        - | 10942 | ` *  Argument number(index start from zero).` |
|        - | 10943 | ` * Return` |
|        - | 10944 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10945 | ` */` |
|       22 | 10946 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10947 |  |
|       24 | 10948 | `	ph7_value *pObj = 0;` |
|       24 | 10949 | `	VmSlot *pSlot = 0;` |
|        - | 10950 | `	VmFrame *pFrame;` |
|        - | 10951 | `	ph7_vm *pVm;` |
|        - | 10952 | `	/* Point to the target VM */` |
|       24 | 10953 | `	pVm = pCtx->pVm;` |
|        - | 10954 | `	/* Current frame */` |
|       24 | 10955 | `	pFrame = pVm->pFrame;` |
|       24 | 10956 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10957 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10958 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10959 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10960 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10961 | `		return SXRET_OK;` |
|        - | 10962 | `	}` |
|        - | 10963 | `	/* Extract the desired index */` |
|       21 | 10964 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10965 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10966 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10967 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10968 | `		return SXRET_OK;` |
|        - | 10969 | `	}` |
|        - | 10970 | `	/* Extract the desired argument */` |
|       21 | 10971 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10972 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10973 | `			/* Return the desired argument */` |
|       21 | 10974 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10975 | `		}else{` |
|        - | 10976 | `			/* No such argument,return false */` |
|      ! 0 | 10977 | `			ph7_result_bool(pCtx,0);` |
|        - | 10978 | `		}` |
|       11 | 10979 | `	}else{` |
|        - | 10980 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10981 | `		ph7_result_bool(pCtx,0);` |
|        - | 10982 | `	}` |
|       21 | 10983 | `	return SXRET_OK;` |
|       13 | 10984 |  |
|        - | 10985 | `/*` |
|        - | 10986 | ` * array func_get_args_byref(void)` |
|        - | 10987 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10988 | ` * Parameters` |
|        - | 10989 | ` *  None.` |
|        - | 10990 | ` * Return` |
|        - | 10991 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10992 | ` *  member of the current user-defined function's argument list.` |
|        - | 10993 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10994 | ` * NOTE:` |
|        - | 10995 | ` *  Arguments are returned to the array by reference.` |
|        - | 10996 | ` */` |
|        2 | 10997 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10998 |  |
|        - | 10999 | `	ph7_value *pArray;` |
|        - | 11000 | `	VmFrame *pFrame;` |
|        - | 11001 | `	VmSlot *aSlot;` |
|        - | 11002 | `	sxu32 n;` |
|        - | 11003 | `	/* Point to the current frame */` |
|        3 | 11004 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11005 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11006 | `	if( pFrame->pParent == 0 ){` |
|        - | 11007 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11008 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11009 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11010 | `		return SXRET_OK;` |
|        - | 11011 | `	}` |
|        - | 11012 | `	/* Create a new array */` |
|        3 | 11013 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11014 | `	if( pArray == 0 ){` |
|      ! 0 | 11015 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11016 | `		SXUNUSED(apArg);` |
|      ! 0 | 11017 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11018 | `		return SXRET_OK;` |
|        - | 11019 | `	}` |
|        - | 11020 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11021 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11022 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11023 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11024 | `	}` |
|        - | 11025 | `	/* Return the freshly created array */` |
|        3 | 11026 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11027 | `	return SXRET_OK;` |
|        2 | 11028 |  |
|        - | 11029 | `/*` |
|        - | 11030 | ` * array func_get_args(void)` |
|        - | 11031 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11032 | ` * Parameters` |
|        - | 11033 | ` *  None.` |
|        - | 11034 | ` * Return` |
|        - | 11035 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11036 | ` *  member of the current user-defined function's argument list.` |
|        - | 11037 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11038 | ` */` |
|       88 | 11039 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11040 |  |
|       90 | 11041 | `	ph7_value *pObj = 0;` |
|        - | 11042 | `	ph7_value *pArray;` |
|        - | 11043 | `	VmFrame *pFrame;` |
|        - | 11044 | `	VmSlot *aSlot;` |
|        - | 11045 | `	sxu32 n;` |
|        - | 11046 | `	/* Point to the current frame */` |
|       90 | 11047 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11048 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11049 | `	if( pFrame->pParent == 0 ){` |
|        - | 11050 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11051 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11052 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11053 | `		return SXRET_OK;` |
|        - | 11054 | `	}` |
|        - | 11055 | `	/* Create a new array */` |
|       90 | 11056 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11057 | `	if( pArray == 0 ){` |
|      ! 0 | 11058 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11059 | `		SXUNUSED(apArg);` |
|      ! 0 | 11060 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11061 | `		return SXRET_OK;` |
|        - | 11062 | `	}` |
|        - | 11063 | `	/* Start filling the array with the given arguments */` |
|       90 | 11064 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11065 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11066 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11067 | `		if( pObj ){` |
|      134 | 11068 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11069 | `		}` |
|       68 | 11070 | `	}` |
|        - | 11071 | `	/* Return the freshly created array */` |
|       90 | 11072 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11073 | `	return SXRET_OK;` |
|       46 | 11074 |  |
|        - | 11075 | `/*` |
|        - | 11076 | ` * bool function_exists(string $name)` |
|        - | 11077 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11078 | ` * Parameters` |
|        - | 11079 | ` *  The name of the desired function.` |
|        - | 11080 | ` * Return` |
|        - | 11081 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11082 | ` */` |
|     1726 | 11083 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11084 |  |
|        - | 11085 | `	const char *zName;` |
|        - | 11086 | `	ph7_vm *pVm;` |
|        - | 11087 | `	int nLen;` |
|        - | 11088 | `	int res;` |
|     1728 | 11089 | `	if( nArg < 1 ){` |
|        - | 11090 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11091 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11092 | `		return SXRET_OK;` |
|        - | 11093 | `	}` |
|        - | 11094 | `	/* Point to the target VM */` |
|     1728 | 11095 | `	pVm = pCtx->pVm;` |
|        - | 11096 | `	/* Extract the function name */` |
|     1728 | 11097 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11098 | `	/* Assume the function is not defined */` |
|     1728 | 11099 | `	res = 0;` |
|        - | 11100 | `	/* Perform the lookup */` |
|     2589 | 11101 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1722 | 11102 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11103 | `			/* Function is defined */` |
|      250 | 11104 | `			res = 1;` |
|      124 | 11105 | `	}` |
|     1728 | 11106 | `	ph7_result_bool(pCtx,res);` |
|     1728 | 11107 | `	return SXRET_OK;` |
|      865 | 11108 |  |
|        - | 11109 | `/*` |
|        - | 11110 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11111 | ` * [i.e: Whether it is callable or not].` |
|        - | 11112 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11113 | ` */` |
|    23242 | 11114 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11115 |  |
|    23244 | 11116 | `	int res = 0;` |
|    23244 | 11117 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11118 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11119 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11120 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11121 | `		 * standard PHP behavior. */` |
|       20 | 11122 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11123 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11124 | `			res = 1;` |
|       10 | 11125 | `		}` |
|        9 | 11126 | `		(void)CallInvoke;` |
|    23235 | 11127 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11128 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11129 | `		if( pMap->nEntry == 2 ){` |
|        - | 11130 | `			ph7_class *pClass;` |
|        - | 11131 | `			ph7_value *pV;` |
|        - | 11132 | `			/* Extract the target class */` |
|       12 | 11133 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11134 | `			if( pV ){` |
|       12 | 11135 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11136 | `				if( pClass ){` |
|        - | 11137 | `					ph7_class_method *pMethod;` |
|        - | 11138 | `					/* Extract the target method */` |
|       10 | 11139 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11140 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11141 | `						/* Perform the lookup */` |
|       10 | 11142 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11143 | `						if( pMethod ){` |
|        - | 11144 | `							/* Method is callable */` |
|        5 | 11145 | `							res = 1;` |
|        2 | 11146 | `						}` |
|        4 | 11147 | `					}` |
|        4 | 11148 | `				}` |
|        5 | 11149 | `			}` |
|        7 | 11150 | `		}` |
|    23213 | 11151 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11152 | `		const char *zName;` |
|        - | 11153 | `		int nLen;` |
|        - | 11154 | `		/* Extract the name */` |
|     5820 | 11155 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11156 | `		/* Perform the lookup */` |
|     5835 | 11157 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11158 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11159 | `				/* Function is callable */` |
|     5802 | 11160 | `				res = 1;` |
|     2900 | 11161 | `		}` |
|     2909 | 11162 | `	}` |
|    23244 | 11163 | `	return res;` |
|        2 | 11164 |  |
|        - | 11165 | `/*` |
|        - | 11166 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11167 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11168 | ` * Parameters` |
|        - | 11169 | ` * $name` |
|        - | 11170 | ` *    The callback function to check` |
|        - | 11171 | ` * $syntax_only` |
|        - | 11172 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11173 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11174 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11175 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11176 | ` *    a string.` |
|        - | 11177 | ` * Return` |
|        - | 11178 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11179 | ` */` |
|       20 | 11180 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11181 |  |
|        - | 11182 | `	ph7_vm *pVm;` |
|        - | 11183 | `	int res;` |
|       21 | 11184 | `	if( nArg < 1 ){` |
|        - | 11185 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11186 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11187 | `		return SXRET_OK;` |
|        - | 11188 | `	}` |
|        - | 11189 | `	/* Point to the target VM */` |
|       21 | 11190 | `	pVm = pCtx->pVm;` |
|        - | 11191 | `	/* Perform the requested operation */` |
|       21 | 11192 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11193 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11194 | `	return SXRET_OK;` |
|       11 | 11195 |  |
|        - | 11196 | `/*` |
|        - | 11197 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11198 | ` * defined below.` |
|        - | 11199 | ` */` |
|     1302 | 11200 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11201 |  |
|     1303 | 11202 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11203 | `	ph7_value sName;` |
|        - | 11204 | `	sxi32 rc;` |
|        - | 11205 | `	/* Prepare the function name for insertion */` |
|     1303 | 11206 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1303 | 11207 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11208 | `	/* Perform the insertion */` |
|     1303 | 11209 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1303 | 11210 | `	PH7_MemObjRelease(&sName);` |
|     1303 | 11211 | `	return rc;` |
|        1 | 11212 |  |
|        - | 11213 | `/*` |
|        - | 11214 | ` * array get_defined_functions(void)` |
|        - | 11215 | ` *  Returns an array of all defined functions.` |
|        - | 11216 | ` * Parameter` |
|        - | 11217 | ` *  None.` |
|        - | 11218 | ` * Return` |
|        - | 11219 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11220 | ` *  both built-in (internal) and user-defined.` |
|        - | 11221 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11222 | ` *  defined ones using $arr["user"].` |
|        - | 11223 | ` * Note:` |
|        - | 11224 | ` *  NULL is returned on failure.` |
|        - | 11225 | ` */` |
|        2 | 11226 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11227 |  |
|        - | 11228 | `	ph7_value *pArray,*pEntry;` |
|        - | 11229 | `	/* NOTE:` |
|        - | 11230 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11231 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11232 | `	 */` |
|        3 | 11233 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11234 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11235 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11236 | `		SXUNUSED(apArg);` |
|        - | 11237 | `		/* Return NULL */` |
|      ! 0 | 11238 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11239 | `		return SXRET_OK;` |
|        - | 11240 | `	}` |
|        3 | 11241 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11242 | `	if( pEntry == 0 ){` |
|        - | 11243 | `		/* Return NULL */` |
|      ! 0 | 11244 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11245 | `		return SXRET_OK;` |
|        - | 11246 | `	}` |
|        - | 11247 | `	/* Fill with the appropriate information */` |
|        3 | 11248 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11249 | `	/* Create the 'internal' index */` |
|        3 | 11250 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11251 | `	/* Create the user-func array */` |
|        3 | 11252 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11253 | `	if( pEntry == 0 ){` |
|        - | 11254 | `		/* Return NULL */` |
|      ! 0 | 11255 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11256 | `		return SXRET_OK;` |
|        - | 11257 | `	}` |
|        - | 11258 | `	/* Fill with the appropriate information */` |
|        3 | 11259 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11260 | `	/* Create the 'user' index */` |
|        3 | 11261 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11262 | `	/* Return the multi-dimensional array */` |
|        3 | 11263 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11264 | `	return SXRET_OK;` |
|        2 | 11265 |  |
|        - | 11266 | `/*` |
|        - | 11267 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11268 | ` *  Register a function for execution on shutdown.` |
|        - | 11269 | ` * Note` |
|        - | 11270 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11271 | ` *  be called in the same order as they were registered.` |
|        - | 11272 | ` * Parameters` |
|        - | 11273 | ` *  $callback` |
|        - | 11274 | ` *   The shutdown callback to register.` |
|        - | 11275 | ` * $param` |
|        - | 11276 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11277 | ` * Return` |
|        - | 11278 | ` *  Nothing.` |
|        - | 11279 | ` */` |
|       10 | 11280 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11281 |  |
|        - | 11282 | `	VmShutdownCB sEntry;` |
|        - | 11283 | `	int i,j;` |
|       12 | 11284 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11285 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11286 | `		return PH7_OK;` |
|        - | 11287 | `	}` |
|        - | 11288 | `	/* Zero the Entry */` |
|       12 | 11289 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11290 | `	/* Initialize fields */` |
|       12 | 11291 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11292 | `	/* Save the callback name for later invocation name */` |
|       12 | 11293 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11294 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11295 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11296 | `	}` |
|        - | 11297 | `	/* Copy arguments */` |
|       12 | 11298 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11299 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11300 | `			/* Limit reached */` |
|      ! 0 | 11301 | `			break;` |
|        - | 11302 | `		}` |
|      ! 0 | 11303 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11304 | `	}` |
|       12 | 11305 | `	sEntry.nArg = j;` |
|        - | 11306 | `	/* Install the callback */` |
|       12 | 11307 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11308 | `	return PH7_OK;` |
|        7 | 11309 |  |
|        - | 11310 | `/*` |
|        - | 11311 | ` * Section:` |
|        - | 11312 | ` *  Class handling functions.` |
|        - | 11313 | ` * Status:` |
|        - | 11314 | ` *    Stable.` |
|        - | 11315 | ` */` |
|        - | 11316 | `/*` |
|        - | 11317 | ` * Extract the top active class. NULL is returned` |
|        - | 11318 | ` * if the class stack is empty.` |
|        - | 11319 | ` */` |
|      956 | 11320 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11321 |  |
|      958 | 11322 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11323 | `	ph7_class **apClass;` |
|      958 | 11324 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11325 | `		/* Empty stack,return NULL */` |
|       15 | 11326 | `		return 0;` |
|        - | 11327 | `	}` |
|        - | 11328 | `	/* Peek the last entry */` |
|      944 | 11329 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      944 | 11330 | `	return apClass[pSet->nUsed - 1];` |
|      480 | 11331 |  |
|        - | 11332 | `/*` |
|        - | 11333 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11334 | ` *   Get the class that declared the currently executing method.` |
|        - | 11335 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11336 | ` *` |
|        - | 11337 | ` * Parameters` |
|        - | 11338 | ` *   pVm: Target VM` |
|        - | 11339 | ` *` |
|        - | 11340 | ` * Return` |
|        - | 11341 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11342 | ` *   - Not executing within a class method` |
|        - | 11343 | ` *` |
|        - | 11344 | ` * Note` |
|        - | 11345 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11346 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11347 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11348 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11349 | ` *   declaring class.` |
|        - | 11350 | ` */` |
|       98 | 11351 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11352 |  |
|      100 | 11353 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11354 | `	ph7_vm_func *pVmFunc;` |
|        - | 11355 |  |
|        - | 11356 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11357 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11358 |  |
|        - | 11359 | `	/* Check if we're in a method context */` |
|      100 | 11360 | `	if( pFrame->pParent ){` |
|       96 | 11361 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11362 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11363 | `			/* Return the declaring class */` |
|       96 | 11364 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11365 | `		}` |
|      ! 0 | 11366 | `	}` |
|        - | 11367 |  |
|        5 | 11368 | `	return 0;` |
|       51 | 11369 |  |
|        - | 11370 |  |
|        - | 11371 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11372 | `/*` |
|        - | 11373 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11374 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11375 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11376 | ` * return value indicates failure.` |
|        - | 11377 | ` */` |
|        - | 11378 | `/*` |
|        - | 11379 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11380 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11381 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11382 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11383 | ` */` |
|     2430 | 11384 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11385 | `	ph7_vm *pVm,` |
|        - | 11386 | `	ph7_class_instance *pThis,` |
|        - | 11387 | `	ph7_class_method *pMethod,` |
|        - | 11388 | `	ph7_value *pResult,` |
|        - | 11389 | `	int nArg,` |
|        - | 11390 | `	ph7_value **apArg,` |
|        - | 11391 | `	VmCallArgMap *pMap` |
|        - | 11392 | `	)` |
|        2 | 11393 |  |
|        - | 11394 | `	ph7_value *aStack;` |
|        - | 11395 | `	VmInstr aInstr[2];` |
|        - | 11396 | `	int iCursor;` |
|        - | 11397 | `	int i;` |
|        - | 11398 | `	sxi32 rc;` |
|     2432 | 11399 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2432 | 11400 | `	if( aStack == 0 ){` |
|      ! 0 | 11401 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11402 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11403 | `		return SXERR_MEM;` |
|        - | 11404 | `	}` |
|     3946 | 11405 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1516 | 11406 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1516 | 11407 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      759 | 11408 | `	}` |
|     2432 | 11409 | `	iCursor = nArg + 1;` |
|     2432 | 11410 | `	if( pThis ){` |
|     2426 | 11411 | `		pThis->iRef++;` |
|     2426 | 11412 | `		aStack[i].x.pOther = pThis;` |
|     2426 | 11413 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1212 | 11414 | `	}` |
|     2432 | 11415 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2432 | 11416 | `	i++;` |
|     2432 | 11417 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2432 | 11418 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2432 | 11419 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2432 | 11420 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2432 | 11421 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2432 | 11422 | `	aInstr[0].iP1 = nArg;` |
|     2432 | 11423 | `	aInstr[0].iP2 = 0;` |
|     2432 | 11424 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2432 | 11425 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2432 | 11426 | `	aInstr[1].iP1 = 1;` |
|     2432 | 11427 | `	aInstr[1].iP2 = 0;` |
|     2432 | 11428 | `	aInstr[1].p3  = 0;` |
|     2432 | 11429 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2432 | 11430 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11431 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11432 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2432 | 11433 | `	return rc;` |
|     1217 | 11434 |  |
|     1908 | 11435 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11436 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11437 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11438 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11439 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11440 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11441 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11442 | `	)` |
|        2 | 11443 |  |
|     1910 | 11444 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11445 |  |
|        - | 11446 | `/*` |
|        - | 11447 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11448 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11449 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11450 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11451 | ` *` |
|        - | 11452 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11453 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11454 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11455 | ` *` |
|        - | 11456 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11457 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11458 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11459 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11460 | ` *` |
|        - | 11461 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11462 | ` */` |
|      174 | 11463 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11464 | `	ph7_vm *pVm,` |
|        - | 11465 | `	ph7_class_instance *pThis,` |
|        - | 11466 | `	int nArg,` |
|        - | 11467 | `	ph7_value **apArg,` |
|        - | 11468 | `	ph7_value *pResult,` |
|        - | 11469 | `	VmCallArgMap *pMap` |
|        - | 11470 | `	)` |
|        2 | 11471 |  |
|        - | 11472 | `	ph7_class_method *pMethod;` |
|      176 | 11473 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11474 | `	if( pMethod == 0 ){` |
|       13 | 11475 | `		if( pResult ){` |
|       13 | 11476 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11477 | `		}` |
|       13 | 11478 | `		return SXERR_INVALID;` |
|        - | 11479 | `	}` |
|      164 | 11480 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11481 |  |
|        - | 11482 | `/*` |
|        - | 11483 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11484 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11485 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11486 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11487 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11488 | ` * lookup or 'goto Exception').` |
|        - | 11489 | ` *` |
|        - | 11490 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11491 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11492 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11493 | ` * reported.` |
|        - | 11494 | ` */` |
|       12 | 11495 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11496 |  |
|        - | 11497 | `	ph7_class *pErrorClass;` |
|       13 | 11498 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11499 | `	ph7_class_method *pCons;` |
|        - | 11500 | `	VmFrame *pThrowFrame;` |
|        - | 11501 | `	char zMsg[256];` |
|        - | 11502 | `	int nMsg;` |
|        - | 11503 | `	sxi32 rc;` |
|       25 | 11504 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11505 | `		"Object of type %.*s is not callable",` |
|       12 | 11506 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11507 | `		pThis->pClass->sName.zString);` |
|       13 | 11508 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11509 | `	if( pErrorClass ){` |
|       13 | 11510 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11511 | `	}` |
|       13 | 11512 | `	if( pErrInst == 0 ){` |
|        - | 11513 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11514 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11515 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11516 | `		 * visible to the user. */` |
|      ! 0 | 11517 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11518 | `		return SXERR_ABORT;` |
|        - | 11519 | `	}` |
|       13 | 11520 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11521 | `	if( pCons ){` |
|        - | 11522 | `		ph7_value sArg;` |
|        - | 11523 | `		ph7_value *apMsg[1];` |
|        - | 11524 | `		SyString sMsgStr;` |
|       13 | 11525 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11526 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11527 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11528 | `		apMsg[0] = &sArg;` |
|       13 | 11529 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11530 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11531 | `	}` |
|        - | 11532 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11533 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11534 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11535 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11536 | `	if( pThrowFrame ){` |
|       13 | 11537 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11538 | `	}` |
|       13 | 11539 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11540 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11541 | `	return rc;` |
|        7 | 11542 |  |
|        - | 11543 | `/*` |
|        - | 11544 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11545 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11546 | ` * in the apArg[] array.` |
|        - | 11547 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11548 | ` * return value indicates failure.` |
|        - | 11549 | ` */` |
|     1212 | 11550 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11551 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11552 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11553 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11554 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11555 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11556 | `	)` |
|        2 | 11557 |  |
|        - | 11558 | `	ph7_value *aStack;` |
|        - | 11559 | `	VmInstr aInstr[2];` |
|        - | 11560 | `	int i;` |
|     1214 | 11561 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11562 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11563 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11564 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 11565 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 11566 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 11567 | `			nArg,apArg,pResult,0);` |
|        - | 11568 | `	}` |
|     1122 | 11569 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11570 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11571 | `		if( pResult ){` |
|        - | 11572 | `			/* Assume a null return value */` |
|      ! 0 | 11573 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11574 | `		}` |
|      511 | 11575 | `		return SXERR_INVALID;` |
|        - | 11576 | `	}` |
|      612 | 11577 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11578 | `		/* Class method */` |
|       15 | 11579 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 11580 | `		ph7_class_method *pMethod = 0;` |
|       15 | 11581 | `		ph7_class_instance *pThis = 0;` |
|       15 | 11582 | `		ph7_class *pClass = 0;` |
|        - | 11583 | `		ph7_value *pValue;` |
|        - | 11584 | `		sxi32 rc;` |
|       15 | 11585 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11586 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11587 | `			if( pResult ){` |
|        - | 11588 | `				/* Assume a null return value */` |
|      ! 0 | 11589 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11590 | `			}` |
|      ! 0 | 11591 | `			return SXRET_OK;` |
|        - | 11592 | `		}` |
|        - | 11593 | `		/* Extract the class name or an instance of it */` |
|       15 | 11594 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 11595 | `		if( pValue ){` |
|       15 | 11596 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 11597 | `		}` |
|       15 | 11598 | `		if( pClass == 0 ){` |
|        - | 11599 | `			/* No such class,return NULL */` |
|      ! 0 | 11600 | `			if( pResult ){` |
|      ! 0 | 11601 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11602 | `			}` |
|      ! 0 | 11603 | `			return SXRET_OK;` |
|        - | 11604 | `		}` |
|       15 | 11605 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11606 | `			/* Point to the class instance */` |
|        9 | 11607 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 11608 | `		}` |
|        - | 11609 | `		/* Try to extract the method */` |
|       15 | 11610 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 11611 | `		if( pValue ){` |
|       15 | 11612 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 11613 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 11614 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 11615 | `			}` |
|        7 | 11616 | `		}` |
|       15 | 11617 | `		if( pMethod == 0 ){` |
|        - | 11618 | `			/* No such method,return NULL */` |
|      ! 0 | 11619 | `			if( pResult ){` |
|      ! 0 | 11620 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11621 | `			}` |
|      ! 0 | 11622 | `			return SXRET_OK;` |
|        - | 11623 | `		}` |
|        - | 11624 | `		/* Call the class method */` |
|       15 | 11625 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 11626 | `		return rc;` |
|        - | 11627 | `	}` |
|        - | 11628 | `	/* Create a new operand stack */` |
|      598 | 11629 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 11630 | `	if( aStack == 0 ){` |
|      ! 0 | 11631 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11632 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11633 | `		if( pResult ){` |
|        - | 11634 | `			/* Assume a null return value */` |
|      ! 0 | 11635 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11636 | `		}` |
|      ! 0 | 11637 | `		return SXERR_MEM;` |
|        - | 11638 | `	}` |
|        - | 11639 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 11640 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 11641 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11642 | `		/*` |
|        - | 11643 | `		 * Symisc eXtension:` |
|        - | 11644 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11645 | `		 */` |
|     1304 | 11646 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 11647 | `	}` |
|        - | 11648 | `	/* Push the function name */` |
|      598 | 11649 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 11650 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11651 | `	/* Emit the CALL istruction */` |
|      598 | 11652 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 11653 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 11654 | `	aInstr[0].iP2 = 0;` |
|      598 | 11655 | `	aInstr[0].p3  = 0;` |
|        - | 11656 | `	/* Emit the DONE instruction */` |
|      598 | 11657 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 11658 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 11659 | `	aInstr[1].iP2 = 0;` |
|      598 | 11660 | `	aInstr[1].p3  = 0;` |
|        - | 11661 | `	/* Execute the function body (if available) */` |
|        - | 11662 | `	{` |
|        - | 11663 | `		sxi32 rcExec;` |
|      598 | 11664 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11665 | `		/* Clean up the mess left behind */` |
|      598 | 11666 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11667 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 11668 | `		return rcExec;` |
|        - | 11669 | `	}` |
|      608 | 11670 |  |
|        - | 11671 | `/*` |
|        - | 11672 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11673 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11674 | ` * parameter.` |
|        - | 11675 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11676 | ` * return value indicates failure.` |
|        - | 11677 | ` */` |
|      240 | 11678 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11679 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11680 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11681 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11682 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11683 | `	)` |
|        1 | 11684 |  |
|        - | 11685 | `	ph7_value *pArg;` |
|        - | 11686 | `	SySet aArg;` |
|        - | 11687 | `	va_list ap;` |
|        - | 11688 | `	sxi32 rc;` |
|      241 | 11689 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11690 | `	/* Copy arguments one after one */` |
|      241 | 11691 | `	va_start(ap,pResult);` |
|      399 | 11692 | `	for(;;){` |
|      799 | 11693 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 11694 | `		if( pArg == 0 ){` |
|      241 | 11695 | `			break;` |
|        - | 11696 | `		}` |
|      559 | 11697 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11698 | `	}` |
|        - | 11699 | `	/* Call the core routine */` |
|      241 | 11700 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11701 | `	/* Cleanup */` |
|      241 | 11702 | `	SySetRelease(&aArg);` |
|      241 | 11703 | `	return rc;` |
|        1 | 11704 |  |
|        - | 11705 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11706 | `/*` |
|        - | 11707 | ` * bool defined(string $name)` |
|        - | 11708 | ` *  Checks whether a given named constant exists.` |
|        - | 11709 | ` * Parameter:` |
|        - | 11710 | ` *  Name of the desired constant.` |
|        - | 11711 | ` * Return` |
|        - | 11712 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11713 | ` */` |
|       20 | 11714 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11715 |  |
|        - | 11716 | `	const char *zName;` |
|       22 | 11717 | `	int nLen = 0;` |
|       22 | 11718 | `	int res = 0;` |
|       22 | 11719 | `	if( nArg < 1 ){` |
|        - | 11720 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11721 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11722 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11723 | `		return SXRET_OK;` |
|        - | 11724 | `	}` |
|        - | 11725 | `	/* Extract constant name */` |
|       22 | 11726 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11727 | `	/* Perform the lookup */` |
|       22 | 11728 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11729 | `		/* Already defined */` |
|       20 | 11730 | `		res = 1;` |
|        9 | 11731 | `	}` |
|       22 | 11732 | `	ph7_result_bool(pCtx,res);` |
|       22 | 11733 | `	return SXRET_OK;` |
|       12 | 11734 |  |
|        - | 11735 | `/*` |
|        - | 11736 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11737 | ` * below.` |
|        - | 11738 | ` */` |
|       10 | 11739 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11740 |  |
|       12 | 11741 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11742 | `	/* Expand constant value */` |
|       12 | 11743 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11744 |  |
|        - | 11745 | `/*` |
|        - | 11746 | ` * bool define(string $constant_name,expression value)` |
|        - | 11747 | ` *  Defines a named constant at runtime.` |
|        - | 11748 | ` * Parameter:` |
|        - | 11749 | ` *  $constant_name` |
|        - | 11750 | ` *   The name of the constant` |
|        - | 11751 | ` *  $value` |
|        - | 11752 | ` *   Constant value` |
|        - | 11753 | ` * Return:` |
|        - | 11754 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11755 | ` */` |
|       12 | 11756 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11757 |  |
|        - | 11758 | `	const char *zName;  /* Constant name */` |
|        - | 11759 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11760 | `	int nLen = 0;       /* Name length */` |
|        - | 11761 | `	sxi32 rc;` |
|       14 | 11762 | `	if( nArg < 2 ){` |
|        - | 11763 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11764 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11765 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11766 | `		return SXRET_OK;` |
|        - | 11767 | `	}` |
|       14 | 11768 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11769 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11770 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11771 | `		return SXRET_OK;` |
|        - | 11772 | `	}` |
|        - | 11773 | `	/* Extract constant name */` |
|       14 | 11774 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11775 | `	if( nLen < 1 ){` |
|      ! 0 | 11776 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11777 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11778 | `		return SXRET_OK;` |
|        - | 11779 | `	}` |
|        - | 11780 | `	/* Duplicate constant value */` |
|       14 | 11781 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11782 | `	if( pValue == 0 ){` |
|      ! 0 | 11783 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11784 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11785 | `		return SXRET_OK;` |
|        - | 11786 | `	}` |
|        - | 11787 | `	/* Initialize the memory object */` |
|       14 | 11788 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11789 | `	/* Register the constant */` |
|       14 | 11790 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11791 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11792 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11793 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11794 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11795 | `		return SXRET_OK;` |
|        - | 11796 | `	}` |
|        - | 11797 | `	/* Duplicate constant value */` |
|       14 | 11798 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11799 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11800 | `		/* Lower case the constant name */` |
|      ! 0 | 11801 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11802 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11803 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11804 | `				/* UTF-8 stream */` |
|      ! 0 | 11805 | `				zCur++;` |
|      ! 0 | 11806 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11807 | `					zCur++;` |
|      ! 0 | 11808 | `				}` |
|      ! 0 | 11809 | `				continue;` |
|        - | 11810 | `			}` |
|      ! 0 | 11811 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11812 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11813 | `				zCur[0] = (char)c;` |
|      ! 0 | 11814 | `			}` |
|      ! 0 | 11815 | `			zCur++;` |
|      ! 0 | 11816 | `		}` |
|        - | 11817 | `		/* Finally,register the constant */` |
|      ! 0 | 11818 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11819 | `	}` |
|        - | 11820 | `	/* All done,return TRUE */` |
|       14 | 11821 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11822 | `	return SXRET_OK;` |
|        8 | 11823 |  |
|        - | 11824 | `/*` |
|        - | 11825 | ` * value constant(string $name)` |
|        - | 11826 | ` *  Returns the value of a constant` |
|        - | 11827 | ` * Parameter` |
|        - | 11828 | ` *  $name` |
|        - | 11829 | ` *    Name of the constant.` |
|        - | 11830 | ` * Return` |
|        - | 11831 | ` *  Constant value or NULL if not defined.` |
|        - | 11832 | ` */` |
|        8 | 11833 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11834 |  |
|        - | 11835 | `	SyHashEntry *pEntry;` |
|        - | 11836 | `	ph7_constant *pCons;` |
|        - | 11837 | `	const char *zName; /* Constant name */` |
|        - | 11838 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11839 | `	int nLen;` |
|       10 | 11840 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11841 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11842 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11843 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11844 | `		return SXRET_OK;` |
|        - | 11845 | `	}` |
|        - | 11846 | `	/* Extract the constant name */` |
|       10 | 11847 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11848 | `	/* Perform the query */` |
|       10 | 11849 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11850 | `	if( pEntry == 0 ){` |
|        3 | 11851 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11852 | `		ph7_result_null(pCtx);` |
|        3 | 11853 | `		return SXRET_OK;` |
|        - | 11854 | `	}` |
|        8 | 11855 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11856 | `	/* Point to the structure that describe the constant */` |
|        8 | 11857 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11858 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11859 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11860 | `	/* Return that value */` |
|        8 | 11861 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11862 | `	/* Cleanup */` |
|        8 | 11863 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11864 | `	return SXRET_OK;` |
|        6 | 11865 |  |
|        - | 11866 | `/*` |
|        - | 11867 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11868 | ` * defined below.` |
|        - | 11869 | ` */` |
|      466 | 11870 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11871 |  |
|      467 | 11872 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11873 | `	ph7_value sName;` |
|        - | 11874 | `	sxi32 rc;` |
|        - | 11875 | `	/* Prepare the constant name for insertion */` |
|      467 | 11876 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 11877 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11878 | `	/* Perform the insertion */` |
|      467 | 11879 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 11880 | `	PH7_MemObjRelease(&sName);` |
|      467 | 11881 | `	return rc;` |
|        1 | 11882 |  |
|        - | 11883 | `/*` |
|        - | 11884 | ` * array get_defined_constants(void)` |
|        - | 11885 | ` *  Returns an associative array with the names of all defined` |
|        - | 11886 | ` *  constants.` |
|        - | 11887 | ` * Parameters` |
|        - | 11888 | ` *  NONE.` |
|        - | 11889 | ` * Returns` |
|        - | 11890 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11891 | ` */` |
|        2 | 11892 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11893 |  |
|        - | 11894 | `	ph7_value *pArray;` |
|        - | 11895 | `	/* Create the array first*/` |
|        3 | 11896 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11897 | `	if( pArray == 0 ){` |
|      ! 0 | 11898 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11899 | `		SXUNUSED(apArg);` |
|        - | 11900 | `		/* Return NULL */` |
|      ! 0 | 11901 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11902 | `		return SXRET_OK;` |
|        - | 11903 | `	}` |
|        - | 11904 | `	/* Fill the array with the defined constants */` |
|        3 | 11905 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11906 | `	/* Return the created array */` |
|        3 | 11907 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11908 | `	return SXRET_OK;` |
|        2 | 11909 |  |
|        - | 11910 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11911 | `/*` |
|        - | 11912 | ` * Section:` |
|        - | 11913 | ` *  Random numbers/string generators.` |
|        - | 11914 | ` * Status:` |
|        - | 11915 | ` *    Stable.` |
|        - | 11916 | ` */` |
|        - | 11917 | `/*` |
|        - | 11918 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11919 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11920 | ` * used by te SQLite3 library.` |
|        - | 11921 | ` */` |
|     2895 | 11922 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11923 |  |
|        - | 11924 | `	sxu32 iNum;` |
|     2897 | 11925 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2897 | 11926 | `	return iNum;` |
|        2 | 11927 |  |
|        - | 11928 | `/*` |
|        - | 11929 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11930 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11931 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11932 | ` * by te SQLite3 library.` |
|        - | 11933 | ` */` |
|   232888 | 11934 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11935 |  |
|        - | 11936 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11937 | `	int i;` |
|        - | 11938 | `	/* Generate a binary string first */` |
|   232890 | 11939 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11940 | `	/* Turn the binary string into english based alphabet */` |
|  2561938 | 11941 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2329050 | 11942 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1164526 | 11943 | `	 }` |
|   232890 | 11944 |  |
|        - | 11945 | `/*` |
|        - | 11946 | ` * int rand()` |
|        - | 11947 | ` * int mt_rand()` |
|        - | 11948 | ` * int rand(int $min,int $max)` |
|        - | 11949 | ` * int mt_rand(int $min,int $max)` |
|        - | 11950 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11951 | ` * Parameter` |
|        - | 11952 | ` *  $min` |
|        - | 11953 | ` *    The lowest value to return (default: 0)` |
|        - | 11954 | ` *  $max` |
|        - | 11955 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11956 | ` * Return` |
|        - | 11957 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11958 | ` * Note:` |
|        - | 11959 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11960 | ` *  by te SQLite3 library.` |
|        - | 11961 | ` */` |
|       20 | 11962 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11963 |  |
|        - | 11964 | `	sxu32 iNum;` |
|        - | 11965 | `	/* Generate the random number */` |
|       21 | 11966 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11967 | `	if( nArg > 1 ){` |
|        - | 11968 | `		sxu32 iMin,iMax;` |
|        3 | 11969 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11970 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11971 | `		if( iMin < iMax ){` |
|        3 | 11972 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11973 | `			if( iDiv > 0 ){` |
|        3 | 11974 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11975 | `			}` |
|        1 | 11976 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11977 | `			iNum %= iMax;` |
|      ! 0 | 11978 | `		}` |
|        1 | 11979 | `	}` |
|        - | 11980 | `	/* Return the number */` |
|       21 | 11981 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11982 | `	return SXRET_OK;` |
|        1 | 11983 |  |
|        - | 11984 | `/*` |
|        - | 11985 | ` * int getrandmax(void)` |
|        - | 11986 | ` * int mt_getrandmax(void)` |
|        - | 11987 | ` * int rc4_getrandmax(void)` |
|        - | 11988 | ` *   Show largest possible random value` |
|        - | 11989 | ` * Return` |
|        - | 11990 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11991 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11992 | ` * Note:` |
|        - | 11993 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11994 | ` *  by te SQLite3 library.` |
|        - | 11995 | ` */` |
|        4 | 11996 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11997 |  |
|        2 | 11998 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11999 | `	SXUNUSED(apArg);` |
|        5 | 12000 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12001 | `	return SXRET_OK;` |
|        1 | 12002 |  |
|        - | 12003 | `/*` |
|        - | 12004 | ` * string rand_str()` |
|        - | 12005 | ` * string rand_str(int $len)` |
|        - | 12006 | ` *  Generate a random string (English alphabet).` |
|        - | 12007 | ` * Parameter` |
|        - | 12008 | ` *  $len` |
|        - | 12009 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12010 | ` * Return` |
|        - | 12011 | ` *   A pseudo random string.` |
|        - | 12012 | ` * Note:` |
|        - | 12013 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12014 | ` *  by te SQLite3 library.` |
|        - | 12015 | ` *  This function is a symisc extension.` |
|        - | 12016 | ` */` |
|      120 | 12017 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12018 |  |
|        - | 12019 | `	char zString[1024];` |
|      122 | 12020 | `	int iLen = 0x10;` |
|      122 | 12021 | `	if( nArg > 0 ){` |
|        - | 12022 | `		/* Get the desired length */` |
|      122 | 12023 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12024 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12025 | `			/* Default length */` |
|        3 | 12026 | `			iLen = 0x10;` |
|        1 | 12027 | `		}` |
|       60 | 12028 | `	}` |
|        - | 12029 | `	/* Generate the random string */` |
|      122 | 12030 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12031 | `	/* Return the generated string */` |
|      122 | 12032 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12033 | `	return SXRET_OK;` |
|        2 | 12034 |  |
|        - | 12035 | `/*` |
|        - | 12036 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12037 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12038 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12039 | ` */` |
|      488 | 12040 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12041 |  |
|      488 | 12042 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12043 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12044 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12045 | `			"TypeError",` |
|        - | 12046 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12047 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12048 | `			ph7_type_name(pArg)` |
|        - | 12049 | `			);` |
|        - | 12050 | `	}` |
|      483 | 12051 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12052 | `		int len;` |
|        9 | 12053 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12054 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12055 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12056 | `				"TypeError",` |
|        - | 12057 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12058 | `				zFunc,iArgPos,zParamName` |
|        - | 12059 | `				);` |
|        - | 12060 | `		}` |
|        2 | 12061 | `	}` |
|      479 | 12062 | `	return SXRET_OK;` |
|      245 | 12063 |  |
|        - | 12064 | `/*` |
|        - | 12065 | ` * int random_int(int $min, int $max)` |
|        - | 12066 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12067 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12068 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12069 | ` *  power-of-two mask covering the range.` |
|        - | 12070 | ` */` |
|      242 | 12071 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12072 |  |
|        - | 12073 | `	sxi64 iMin,iMax;` |
|        - | 12074 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12075 | `	unsigned int nAttempt;` |
|        - | 12076 | `	int rc;` |
|      243 | 12077 | `	if( nArg != 2 ){` |
|       10 | 12078 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12079 | `			"ArgumentCountError",` |
|        - | 12080 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12081 | `			nArg` |
|        - | 12082 | `			);` |
|        - | 12083 | `	}` |
|      237 | 12084 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12085 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12086 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12087 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12088 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12089 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12090 | `	if( iMin > iMax ){` |
|        3 | 12091 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12092 | `			"ValueError",` |
|        - | 12093 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12094 | `			);` |
|        - | 12095 | `	}` |
|      229 | 12096 | `	if( iMin == iMax ){` |
|        5 | 12097 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12098 | `		return SXRET_OK;` |
|        - | 12099 | `	}` |
|      225 | 12100 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12101 | `	uMask = uRange;` |
|      225 | 12102 | `	uMask \|= uMask >> 1;` |
|      225 | 12103 | `	uMask \|= uMask >> 2;` |
|      225 | 12104 | `	uMask \|= uMask >> 4;` |
|      225 | 12105 | `	uMask \|= uMask >> 8;` |
|      225 | 12106 | `	uMask \|= uMask >> 16;` |
|      225 | 12107 | `	uMask \|= uMask >> 32;` |
|      225 | 12108 | `	uResult = 0;` |
|      353 | 12109 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12110 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12111 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12112 | `		 * and the low-half mask would always read 0). */` |
|        - | 12113 | `		sxu64 uDraw;` |
|      353 | 12114 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12115 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12116 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12117 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12118 | `				"Exception",` |
|        - | 12119 | `				"Cannot gather sufficient random data"` |
|        - | 12120 | `				);` |
|        - | 12121 | `		}` |
|      353 | 12122 | `		uDraw &= uMask;` |
|      353 | 12123 | `		if( uDraw <= uRange ){` |
|      225 | 12124 | `			uResult = uDraw;` |
|      225 | 12125 | `			break;` |
|        - | 12126 | `		}` |
|       62 | 12127 | `	}` |
|      225 | 12128 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12129 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12130 | `			"Exception",` |
|        - | 12131 | `			"Cannot gather sufficient random data"` |
|        - | 12132 | `			);` |
|        - | 12133 | `	}` |
|      225 | 12134 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12135 | `	return SXRET_OK;` |
|      122 | 12136 |  |
|        - | 12137 | `/*` |
|        - | 12138 | ` * string random_bytes(int $length)` |
|        - | 12139 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12140 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12141 | ` */` |
|       24 | 12142 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12143 |  |
|        - | 12144 | `	sxi64 iLen;` |
|        - | 12145 | `	unsigned char zStack[256];` |
|        - | 12146 | `	void *pBuf;` |
|        - | 12147 | `	int rc;` |
|       25 | 12148 | `	int bHeap = 0;` |
|       25 | 12149 | `	if( nArg != 1 ){` |
|        7 | 12150 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12151 | `			"ArgumentCountError",` |
|        - | 12152 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12153 | `			nArg` |
|        - | 12154 | `			);` |
|        - | 12155 | `	}` |
|       21 | 12156 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12157 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12158 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12159 | `	if( iLen < 1 ){` |
|        5 | 12160 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12161 | `			"ValueError",` |
|        - | 12162 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12163 | `			);` |
|        - | 12164 | `	}` |
|        - | 12165 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12166 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12167 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12168 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12169 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12170 | `			"ValueError",` |
|        - | 12171 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12172 | `			);` |
|        - | 12173 | `	}` |
|       13 | 12174 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12175 | `		pBuf = zStack;` |
|        7 | 12176 | `	}else{` |
|      ! 0 | 12177 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12178 | `		if( pBuf == 0 ){` |
|      ! 0 | 12179 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12180 | `				"Exception",` |
|        - | 12181 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12182 | `				iLen` |
|        - | 12183 | `				);` |
|        - | 12184 | `		}` |
|      ! 0 | 12185 | `		bHeap = 1;` |
|        - | 12186 | `	}` |
|       13 | 12187 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12188 | `		if( bHeap ){` |
|      ! 0 | 12189 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12190 | `		}` |
|      ! 0 | 12191 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12192 | `			"Exception",` |
|        - | 12193 | `			"Cannot gather sufficient random data"` |
|        - | 12194 | `			);` |
|        - | 12195 | `	}` |
|       13 | 12196 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12197 | `	if( bHeap ){` |
|      ! 0 | 12198 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12199 | `	}` |
|       13 | 12200 | `	return SXRET_OK;` |
|       13 | 12201 |  |
|        - | 12202 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12203 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12204 | `/* Unique ID private data */` |
|        - | 12205 | `struct unique_id_data` |
|        - | 12206 |  |
|        - | 12207 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12208 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12209 | `};` |
|        - | 12210 | `/*` |
|        - | 12211 | ` * Binary to hex consumer callback.` |
|        - | 12212 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12213 | ` * defined below.` |
|        - | 12214 | ` */` |
|      192 | 12215 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12216 |  |
|      193 | 12217 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12218 | `	sxu32 nBuflen;` |
|        - | 12219 | `	/* Extract result buffer length */` |
|      193 | 12220 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12221 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12222 | `			/*` |
|        - | 12223 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12224 | `			 * string will be 13 characters long` |
|        - | 12225 | `			 */` |
|       25 | 12226 | `		return SXERR_ABORT;` |
|        - | 12227 | `	}` |
|      169 | 12228 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12229 | `		return SXERR_ABORT;` |
|        - | 12230 | `	}` |
|        - | 12231 | `	/* Safely Consume the hex stream */` |
|      169 | 12232 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12233 | `	return SXRET_OK;` |
|       97 | 12234 |  |
|        - | 12235 | `/*` |
|        - | 12236 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12237 | ` *  Generate a unique ID` |
|        - | 12238 | ` * Parameter` |
|        - | 12239 | ` * $prefix` |
|        - | 12240 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12241 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12242 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12243 | ` * $more_entropy` |
|        - | 12244 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12245 | ` *  that the result will be unique.` |
|        - | 12246 | ` * Return` |
|        - | 12247 | ` *  Returns the unique identifier, as a string.` |
|        - | 12248 | ` */` |
|       24 | 12249 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12250 |  |
|        - | 12251 | `	struct unique_id_data sUniq;` |
|        - | 12252 | `	unsigned char zDigest[20];` |
|       25 | 12253 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12254 | `	const char *zPrefix;` |
|        - | 12255 | `	SHA1Context sCtx;` |
|        - | 12256 | `	char zRandom[7];` |
|        - | 12257 | `	int nPrefix;` |
|        - | 12258 | `	int entropy;` |
|        - | 12259 | `	/* Generate a random string first */` |
|       25 | 12260 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12261 | `	/* Initialize fields */` |
|       25 | 12262 | `	zPrefix = 0;` |
|       25 | 12263 | `	nPrefix = 0;` |
|       25 | 12264 | `	entropy = 0;` |
|       25 | 12265 | `	if( nArg > 0 ){` |
|        - | 12266 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12267 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12268 | `		if( nArg > 1 ){` |
|      ! 0 | 12269 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12270 | `		}` |
|      ! 0 | 12271 | `	}` |
|       25 | 12272 | `	SHA1Init(&sCtx);` |
|        - | 12273 | `	/* Generate the random ID */` |
|       25 | 12274 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12275 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12276 | `	}` |
|        - | 12277 | `	/* Append the random ID */` |
|       25 | 12278 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12279 | `	/* Append the random string */` |
|       25 | 12280 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12281 | `	/* Increment the number */` |
|       25 | 12282 | `	pVm->unique_id++;` |
|       25 | 12283 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12284 | `	/* Hexify the digest */` |
|       25 | 12285 | `	sUniq.pCtx = pCtx;` |
|       25 | 12286 | `	sUniq.entropy = entropy;` |
|       25 | 12287 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12288 | `	/* All done */` |
|       25 | 12289 | `	return PH7_OK;` |
|        1 | 12290 |  |
|        - | 12291 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12292 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12293 | `/*` |
|        - | 12294 | ` * Section:` |
|        - | 12295 | ` *  Language construct implementation as foreign functions.` |
|        - | 12296 | ` * Status:` |
|        - | 12297 | ` *    Stable.` |
|        - | 12298 | ` */` |
|        - | 12299 | `/*` |
|        - | 12300 | ` * void echo($string...)` |
|        - | 12301 | ` *  Output one or more messages.` |
|        - | 12302 | ` * Parameters` |
|        - | 12303 | ` *  $string` |
|        - | 12304 | ` *   Message to output.` |
|        - | 12305 | ` * Return` |
|        - | 12306 | ` *  NULL.` |
|        - | 12307 | ` */` |
|      ! 0 | 12308 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12309 |  |
|        - | 12310 | `	const char *zData;` |
|      ! 0 | 12311 | `	int nDataLen = 0;` |
|        - | 12312 | `	ph7_vm *pVm;` |
|        - | 12313 | `	int i,rc;` |
|        - | 12314 | `	/* Point to the target VM */` |
|      ! 0 | 12315 | `	pVm = pCtx->pVm;` |
|        - | 12316 | `	/* Output */` |
|      ! 0 | 12317 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12318 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12319 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12320 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12321 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12322 | `			if( rc == SXERR_ABORT ){` |
|        - | 12323 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12324 | `				return PH7_ABORT;` |
|        - | 12325 | `			}` |
|      ! 0 | 12326 | `		}` |
|      ! 0 | 12327 | `	}` |
|      ! 0 | 12328 | `	return SXRET_OK;` |
|      ! 0 | 12329 |  |
|        - | 12330 | `/*` |
|        - | 12331 | ` * int print($string...)` |
|        - | 12332 | ` *  Output one or more messages.` |
|        - | 12333 | ` * Parameters` |
|        - | 12334 | ` *  $string` |
|        - | 12335 | ` *   Message to output.` |
|        - | 12336 | ` * Return` |
|        - | 12337 | ` *  1 always.` |
|        - | 12338 | ` */` |
|        2 | 12339 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12340 |  |
|        - | 12341 | `	const char *zData;` |
|        3 | 12342 | `	int nDataLen = 0;` |
|        - | 12343 | `	ph7_vm *pVm;` |
|        - | 12344 | `	int i,rc;` |
|        - | 12345 | `	/* Point to the target VM */` |
|        3 | 12346 | `	pVm = pCtx->pVm;` |
|        - | 12347 | `	/* Output */` |
|        5 | 12348 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12349 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12350 | `		if( nDataLen > 0 ){` |
|        3 | 12351 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12352 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12353 | `			if( rc == SXERR_ABORT ){` |
|        - | 12354 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12355 | `				return PH7_ABORT;` |
|        - | 12356 | `			}` |
|        1 | 12357 | `		}` |
|        2 | 12358 | `	}` |
|        - | 12359 | `	/* Return 1 */` |
|        3 | 12360 | `	ph7_result_int(pCtx,1);` |
|        3 | 12361 | `	return SXRET_OK;` |
|        2 | 12362 |  |
|        - | 12363 | `/*` |
|        - | 12364 | ` * void exit(string $msg)` |
|        - | 12365 | ` * void exit(int $status)` |
|        - | 12366 | ` * void die(string $ms)` |
|        - | 12367 | ` * void die(int $status)` |
|        - | 12368 | ` *   Output a message and terminate program execution.` |
|        - | 12369 | ` * Parameter` |
|        - | 12370 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12371 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12372 | ` *  and not printed` |
|        - | 12373 | ` * Return` |
|        - | 12374 | ` *  NULL` |
|        - | 12375 | ` */` |
|      ! 0 | 12376 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12377 |  |
|      ! 0 | 12378 | `	if( nArg > 0 ){` |
|      ! 0 | 12379 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12380 | `			const char *zData;` |
|      ! 0 | 12381 | `			int iLen = 0;` |
|        - | 12382 | `			/* Print exit message */` |
|      ! 0 | 12383 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12384 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12385 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12386 | `			sxi32 iExitStatus;` |
|        - | 12387 | `			/* Record exit status code */` |
|      ! 0 | 12388 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12389 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12390 | `		}` |
|      ! 0 | 12391 | `	}` |
|        - | 12392 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12393 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12394 | `	 */` |
|      ! 0 | 12395 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12396 | `	return PH7_ABORT;` |
|      ! 0 | 12397 |  |
|        - | 12398 | `/*` |
|        - | 12399 | ` * bool isset($var,...)` |
|        - | 12400 | ` *  Finds out whether a variable is set.` |
|        - | 12401 | ` * Parameters` |
|        - | 12402 | ` *  One or more variable to check.` |
|        - | 12403 | ` * Return` |
|        - | 12404 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12405 | ` */` |
|    92044 | 12406 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12407 |  |
|        - | 12408 | `	ph7_value *pObj;` |
|    92046 | 12409 | `	int res = 0;` |
|        - | 12410 | `	int i;` |
|    92046 | 12411 | `	if( nArg < 1 ){` |
|        - | 12412 | `		/* Missing arguments,return false */` |
|      ! 0 | 12413 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12414 | `		return SXRET_OK;` |
|        - | 12415 | `	}` |
|        - | 12416 | `	/* Iterate over available arguments */` |
|   120334 | 12417 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92056 | 12418 | `		pObj = apArg[i];` |
|    92056 | 12419 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12420 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12421 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12422 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    62840 | 12423 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12424 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12425 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12426 | `			}` |
|    31419 | 12427 | `		}` |
|    92056 | 12428 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92056 | 12429 | `		if( !res ){` |
|        - | 12430 | `			/* Variable not set,return FALSE */` |
|    63768 | 12431 | `			ph7_result_bool(pCtx,0);` |
|    63768 | 12432 | `			return SXRET_OK;` |
|        - | 12433 | `		}` |
|    14146 | 12434 | `	}` |
|        - | 12435 | `	/* All given variable are set,return TRUE */` |
|    28280 | 12436 | `	ph7_result_bool(pCtx,1);` |
|    28280 | 12437 | `	return SXRET_OK;` |
|    46024 | 12438 |  |
|        - | 12439 | `/*` |
|        - | 12440 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12441 | ` * frame,the reference table and discard it's contents.` |
|        - | 12442 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12443 | ` */` |
|  3151104 | 12444 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12445 |  |
|        - | 12446 | `	ph7_value *pObj;` |
|        - | 12447 | `	VmRefObj *pRef;` |
|  3151106 | 12448 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3151106 | 12449 | `	if( pObj ){` |
|        - | 12450 | `		/* Release the object */` |
|  3151106 | 12451 | `		PH7_MemObjRelease(pObj);` |
|  1575552 | 12452 | `	}` |
|        - | 12453 | `	/* Remove old reference links */` |
|  3151106 | 12454 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3151106 | 12455 | `	if( pRef ){` |
|  3151100 | 12456 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12457 | `		/* Unlink from the reference table */` |
|  3151100 | 12458 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3151100 | 12459 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12460 | `			VmSlot sFree;` |
|        - | 12461 | `			/* Restore to the free list */` |
|  3151092 | 12462 | `			sFree.nIdx = nObjIdx;` |
|  3151092 | 12463 | `			sFree.pUserData = 0;` |
|  3151092 | 12464 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1575545 | 12465 | `		}` |
|  1575549 | 12466 | `	}` |
|  3151106 | 12467 | `	return SXRET_OK;` |
|        2 | 12468 |  |
|        - | 12469 | `/*` |
|        - | 12470 | ` * void unset($var,...)` |
|        - | 12471 | ` *   Unset one or more given variable.` |
|        - | 12472 | ` * Parameters` |
|        - | 12473 | ` *  One or more variable to unset.` |
|        - | 12474 | ` * Return` |
|        - | 12475 | ` *  Nothing.` |
|        - | 12476 | ` */` |
|     7500 | 12477 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12478 |  |
|        - | 12479 | `	ph7_value *pObj;` |
|        - | 12480 | `	ph7_vm *pVm;` |
|        - | 12481 | `	int i;` |
|        - | 12482 | `	/* Point to the target VM */` |
|     7502 | 12483 | `	pVm = pCtx->pVm;` |
|        - | 12484 | `	/* Iterate and unset */` |
|    15002 | 12485 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7502 | 12486 | `		pObj = apArg[i];` |
|     7502 | 12487 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      818 | 12488 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12489 | `				/* Throw an error */` |
|      ! 0 | 12490 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12491 | `			}` |
|      410 | 12492 | `		}else{` |
|     6686 | 12493 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12494 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6686 | 12495 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6680 | 12496 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3339 | 12497 | `			}` |
|        - | 12498 | `		}` |
|     3752 | 12499 | `	}` |
|     7502 | 12500 | `	return SXRET_OK;` |
|        2 | 12501 |  |
|        - | 12502 | `/*` |
|        - | 12503 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12504 | ` */` |
|      116 | 12505 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12506 |  |
|      117 | 12507 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12508 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12509 | `	ph7_value *pObj;` |
|        - | 12510 | `	sxu32 nIdx;` |
|        - | 12511 | `	/* Extract the memory object */` |
|      117 | 12512 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12513 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12514 | `	if( pObj ){` |
|      117 | 12515 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12516 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12517 | `				SyString sName;` |
|        - | 12518 | `				ph7_value sKey;` |
|        - | 12519 | `				/* Perform the insertion */` |
|      115 | 12520 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12521 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12522 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12523 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12524 | `			}` |
|       57 | 12525 | `		}` |
|       58 | 12526 | `	}` |
|      117 | 12527 | `	return SXRET_OK;` |
|        1 | 12528 |  |
|        - | 12529 | `/*` |
|        - | 12530 | ` * array get_defined_vars(void)` |
|        - | 12531 | ` *  Returns an array of all defined variables.` |
|        - | 12532 | ` * Parameter` |
|        - | 12533 | ` *  None` |
|        - | 12534 | ` * Return` |
|        - | 12535 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12536 | ` */` |
|        2 | 12537 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12538 |  |
|        3 | 12539 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12540 | `	ph7_value *pArray;` |
|        - | 12541 | `	/* Create a new array */` |
|        3 | 12542 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12543 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12544 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12545 | `		SXUNUSED(apArg);` |
|        - | 12546 | `		/* Return NULL */` |
|      ! 0 | 12547 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12548 | `		return SXRET_OK;` |
|        - | 12549 | `	}` |
|        - | 12550 | `	/* Superglobals first */` |
|        3 | 12551 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12552 | `	/* Then variable defined in the current frame */` |
|        3 | 12553 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12554 | `	/* Finally,return the created array */` |
|        3 | 12555 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12556 | `	return SXRET_OK;` |
|        2 | 12557 |  |
|        - | 12558 | `/*` |
|        - | 12559 | ` * bool gettype($var)` |
|        - | 12560 | ` *  Get the type of a variable` |
|        - | 12561 | ` * Parameters` |
|        - | 12562 | ` *   $var` |
|        - | 12563 | ` *    The variable being type checked.` |
|        - | 12564 | ` * Return` |
|        - | 12565 | ` *   String representation of the given variable type.` |
|        - | 12566 | ` */` |
|       32 | 12567 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12568 |  |
|       34 | 12569 | `	const char *zType = "Empty";` |
|       34 | 12570 | `	if( nArg > 0 ){` |
|       34 | 12571 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12572 | `	}` |
|        - | 12573 | `	/* Return the variable type */` |
|       34 | 12574 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12575 | `	return SXRET_OK;` |
|        2 | 12576 |  |
|        - | 12577 | `/*` |
|        - | 12578 | ` * string get_resource_type(resource $handle)` |
|        - | 12579 | ` *  This function gets the type of the given resource.` |
|        - | 12580 | ` * Parameters` |
|        - | 12581 | ` *  $handle` |
|        - | 12582 | ` *  The evaluated resource handle.` |
|        - | 12583 | ` * Return` |
|        - | 12584 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12585 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12586 | ` *  the return value will be the string Unknown.` |
|        - | 12587 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12588 | ` *  is not a resource.` |
|        - | 12589 | ` */` |
|        2 | 12590 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12591 |  |
|        3 | 12592 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12593 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12594 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12595 | `		return PH7_OK;` |
|        - | 12596 | `	}` |
|        3 | 12597 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12598 | `	return SXRET_OK;` |
|        2 | 12599 |  |
|        - | 12600 | `/*` |
|        - | 12601 | ` * void var_dump(expression,....)` |
|        - | 12602 | ` *   var_dump � Dumps information about a variable` |
|        - | 12603 | ` * Parameters` |
|        - | 12604 | ` *   One or more expression to dump.` |
|        - | 12605 | ` * Returns` |
|        - | 12606 | ` *  Nothing.` |
|        - | 12607 | ` */` |
|      218 | 12608 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12609 |  |
|        - | 12610 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12611 | `	int i;` |
|      220 | 12612 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12613 | `	/* Dump one or more expressions */` |
|      444 | 12614 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12615 | `		ph7_value *pObj = apArg[i];` |
|        - | 12616 | `		/* Reset the working buffer */` |
|      226 | 12617 | `		SyBlobReset(&sDump);` |
|        - | 12618 | `		/* Dump the given expression */` |
|      226 | 12619 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12620 | `		/* Output */` |
|      226 | 12621 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12622 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12623 | `		}` |
|      114 | 12624 | `	}` |
|        - | 12625 | `	/* Release the working buffer */` |
|      220 | 12626 | `	SyBlobRelease(&sDump);` |
|      220 | 12627 | `	return SXRET_OK;` |
|        2 | 12628 |  |
|        - | 12629 | `/*` |
|        - | 12630 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12631 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12632 | ` * Parameters` |
|        - | 12633 | ` *   expression: Expression to dump` |
|        - | 12634 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12635 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12636 | ` *            print_r() will return the information rather than print it.` |
|        - | 12637 | ` * Return` |
|        - | 12638 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12639 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12640 | ` */` |
|       16 | 12641 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12642 |  |
|       17 | 12643 | `	int ret_string = 0;` |
|        - | 12644 | `	SyBlob sDump;` |
|       17 | 12645 | `	if( nArg < 1 ){` |
|        - | 12646 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12647 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12648 | `		return SXRET_OK;` |
|        - | 12649 | `	}` |
|       17 | 12650 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12651 | `	if ( nArg > 1 ){` |
|        - | 12652 | `		/* Where to redirect output */` |
|       11 | 12653 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12654 | `	}` |
|        - | 12655 | `	/* Generate dump */` |
|       17 | 12656 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12657 | `	if( !ret_string ){` |
|        - | 12658 | `		/* Output dump */` |
|        7 | 12659 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12660 | `		/* Return true */` |
|        7 | 12661 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12662 | `	}else{` |
|        - | 12663 | `		/* Generated dump as return value */` |
|       11 | 12664 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12665 | `	}` |
|        - | 12666 | `	/* Release the working buffer */` |
|       17 | 12667 | `	SyBlobRelease(&sDump);` |
|       17 | 12668 | `	return SXRET_OK;` |
|        9 | 12669 |  |
|        - | 12670 | `/*` |
|        - | 12671 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12672 | ` * Same job as print_r. (see coment above)` |
|        - | 12673 | ` */` |
|        2 | 12674 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12675 |  |
|        3 | 12676 | `	int ret_string = 0;` |
|        - | 12677 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12678 | `	if( nArg < 1 ){` |
|        - | 12679 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12680 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12681 | `		return SXRET_OK;` |
|        - | 12682 | `	}` |
|        3 | 12683 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12684 | `	if ( nArg > 1 ){` |
|        - | 12685 | `		/* Where to redirect output */` |
|        3 | 12686 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12687 | `	}` |
|        - | 12688 | `	/* Generate dump */` |
|        3 | 12689 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12690 | `	if( !ret_string ){` |
|        - | 12691 | `		/* Output dump */` |
|      ! 0 | 12692 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12693 | `		/* Return NULL */` |
|      ! 0 | 12694 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12695 | `	}else{` |
|        - | 12696 | `		/* Generated dump as return value */` |
|        3 | 12697 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12698 | `	}` |
|        - | 12699 | `	/* Release the working buffer */` |
|        3 | 12700 | `	SyBlobRelease(&sDump);` |
|        3 | 12701 | `	return SXRET_OK;` |
|        2 | 12702 |  |
|        - | 12703 | `/*` |
|        - | 12704 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12705 | ` *  Set/get the various assert flags.` |
|        - | 12706 | ` * Parameter` |
|        - | 12707 | ` * $what` |
|        - | 12708 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12709 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12710 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12711 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12712 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12713 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12714 | ` * $value` |
|        - | 12715 | ` *   An optional new value for the option.` |
|        - | 12716 | ` * Return` |
|        - | 12717 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12718 | ` */` |
|       28 | 12719 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12720 |  |
|       30 | 12721 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12722 | `	int iOption;` |
|        - | 12723 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12724 | `	if( nArg < 1 ){` |
|        3 | 12725 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12726 | `			"ArgumentCountError",` |
|        - | 12727 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12728 | `			);` |
|        - | 12729 | `	}` |
|        - | 12730 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12731 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12732 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12733 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12734 | `			"TypeError",` |
|        - | 12735 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12736 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12737 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12738 | `			);` |
|        - | 12739 | `	}` |
|       28 | 12740 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12741 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12742 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12743 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12744 | `	switch( iOption ){` |
|        5 | 12745 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12746 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12747 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12748 | `		if( nArg > 1 ){` |
|        5 | 12749 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12750 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12751 | `			}else{` |
|        3 | 12752 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12753 | `			}` |
|        2 | 12754 | `		}` |
|       12 | 12755 | `		break;` |
|        1 | 12756 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12757 | `		/* Return old callback or null */` |
|        3 | 12758 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12759 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12760 | `		}else{` |
|        3 | 12761 | `			ph7_result_null(pCtx);` |
|        - | 12762 | `		}` |
|        3 | 12763 | `		if( nArg > 1 ){` |
|      ! 0 | 12764 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12765 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12766 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12767 | `			}else{` |
|      ! 0 | 12768 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12769 | `			}` |
|      ! 0 | 12770 | `		}` |
|        3 | 12771 | `		break;` |
|        5 | 12772 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12773 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12774 | `		if( nArg > 1 ){` |
|        5 | 12775 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12776 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12777 | `			}else{` |
|        3 | 12778 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12779 | `			}` |
|        2 | 12780 | `		}` |
|       11 | 12781 | `		break;` |
|      ! 0 | 12782 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12783 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12784 | `		break;` |
|        1 | 12785 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12786 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12787 | `		break;` |
|      ! 0 | 12788 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12789 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12790 | `		break;` |
|        1 | 12791 | `	default:` |
|        - | 12792 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12793 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12794 | `			"ValueError",` |
|        - | 12795 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12796 | `			);` |
|        - | 12797 | `	}` |
|       26 | 12798 | `	return PH7_OK;` |
|       16 | 12799 |  |
|        - | 12800 | `/*` |
|        - | 12801 | ` * bool assert(mixed $assertion)` |
|        - | 12802 | ` *  Checks if assertion is FALSE.` |
|        - | 12803 | ` * Parameter` |
|        - | 12804 | ` *  $assertion` |
|        - | 12805 | ` *    The assertion to test.` |
|        - | 12806 | ` * Return` |
|        - | 12807 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12808 | ` */` |
|       24 | 12809 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12810 |  |
|       26 | 12811 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12812 | `	int iFlags,iResult;` |
|        - | 12813 | `	const char *zDesc;` |
|        - | 12814 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12815 | `	if( nArg < 1 ){` |
|        3 | 12816 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12817 | `			"ArgumentCountError",` |
|        - | 12818 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12819 | `			);` |
|        - | 12820 | `	}` |
|       24 | 12821 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12822 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12823 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12824 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12825 | `		return PH7_OK;` |
|        - | 12826 | `	}` |
|        - | 12827 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12828 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12829 | `	if( !iResult ){` |
|        - | 12830 | `		/* Assertion failed */` |
|        - | 12831 | `		/* Extract optional description */` |
|       13 | 12832 | `		zDesc = 0;` |
|       13 | 12833 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12834 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12835 | `		}` |
|       13 | 12836 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12837 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12838 | `			ph7_value sFile,sLine;` |
|        - | 12839 | `			ph7_value *apCbArg[3];` |
|        - | 12840 | `			SyString *pFile;` |
|        - | 12841 | `			/* Extract the processed script */` |
|      ! 0 | 12842 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12843 | `			if( pFile == 0 ){` |
|      ! 0 | 12844 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12845 | `			}` |
|        - | 12846 | `			/* Invoke the callback */` |
|      ! 0 | 12847 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12848 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12849 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12850 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12851 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12852 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12853 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12854 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12855 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12856 | `		}` |
|       13 | 12857 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12858 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12859 | `			return PH7_ABORT;` |
|        - | 12860 | `		}` |
|        - | 12861 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12862 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12863 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12864 | `				"AssertionError",` |
|        - | 12865 | `				"%s",` |
|        1 | 12866 | `				zDesc` |
|        - | 12867 | `				);` |
|      ! 0 | 12868 | `		}else{` |
|       11 | 12869 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12870 | `				"AssertionError",` |
|        - | 12871 | `				"assert(false)"` |
|        - | 12872 | `				);` |
|        - | 12873 | `		}` |
|        - | 12874 | `	}` |
|        - | 12875 | `	/* Assertion passed */` |
|       11 | 12876 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12877 | `	return PH7_OK;` |
|       14 | 12878 |  |
|        - | 12879 | `/*` |
|        - | 12880 | ` * Section:` |
|        - | 12881 | ` *  Error reporting functions.` |
|        - | 12882 | ` * Status:` |
|        - | 12883 | ` *    Stable.` |
|        - | 12884 | ` */` |
|        - | 12885 | `/*` |
|        - | 12886 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12887 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12888 | ` * Parameters` |
|        - | 12889 | ` *  $error_msg` |
|        - | 12890 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12891 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12892 | ` * $error_type` |
|        - | 12893 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12894 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12895 | ` * Return` |
|        - | 12896 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12897 | ` */` |
|       12 | 12898 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12899 |  |
|       14 | 12900 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12901 | `	int rc = PH7_OK;` |
|       14 | 12902 | `	if( nArg > 0 ){` |
|        - | 12903 | `		const char *zErr;` |
|        - | 12904 | `		int nLen;` |
|        - | 12905 | `		/* Extract the error message */` |
|       12 | 12906 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12907 | `		if( nArg > 1 ){` |
|        - | 12908 | `			/* Extract the error type */` |
|       12 | 12909 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12910 | `			switch( nErr ){` |
|        1 | 12911 | `			case 1:   /* E_ERROR */` |
|        - | 12912 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12913 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12914 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12915 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12916 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12917 | `				break;` |
|        1 | 12918 | `			case 2:   /* E_WARNING */` |
|        - | 12919 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12920 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12921 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12922 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12923 | `				break;` |
|        3 | 12924 | `			default:` |
|        8 | 12925 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12926 | `				break;` |
|        - | 12927 | `			}` |
|        5 | 12928 | `		}` |
|        - | 12929 | `		/* Report error */` |
|       12 | 12930 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12931 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12932 | `			return rc;` |
|        - | 12933 | `		}` |
|        - | 12934 | `		/* Return true */` |
|       12 | 12935 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12936 | `	}else{` |
|        - | 12937 | `		/* Missing arguments,return FALSE */` |
|        3 | 12938 | `		ph7_result_bool(pCtx,0);` |
|        - | 12939 | `	}` |
|       14 | 12940 | `	return rc;` |
|        8 | 12941 |  |
|        - | 12942 | `/*` |
|        - | 12943 | ` * int error_reporting([int $level])` |
|        - | 12944 | ` *  Sets which PHP errors are reported.` |
|        - | 12945 | ` * Parameters` |
|        - | 12946 | ` *  $level` |
|        - | 12947 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12948 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12949 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12950 | ` *   levels will not always behave as expected.` |
|        - | 12951 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12952 | ` *   in the predefined constants.` |
|        - | 12953 | ` * Return` |
|        - | 12954 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12955 | ` *   parameter is given.` |
|        - | 12956 | ` */` |
|       32 | 12957 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12958 |  |
|       34 | 12959 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12960 | `	int nOld;` |
|        - | 12961 | `	/* Extract the old reporting level */` |
|       34 | 12962 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 12963 | `	if( nArg > 0 ){` |
|        - | 12964 | `		int nNew;` |
|        - | 12965 | `		/* Extract the desired error reporting level */` |
|       28 | 12966 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 12967 | `		if( !nNew ){` |
|        - | 12968 | `			/* Do not report errors at all */` |
|        5 | 12969 | `			pVm->bErrReport = 0;` |
|        3 | 12970 | `		}else{` |
|        - | 12971 | `			/* Report all errors */` |
|       24 | 12972 | `			pVm->bErrReport = 1;` |
|        - | 12973 | `		}` |
|       13 | 12974 | `	}` |
|        - | 12975 | `	/* Return the old level */` |
|       34 | 12976 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 12977 | `	return PH7_OK;` |
|        2 | 12978 |  |
|        - | 12979 | `/*` |
|        - | 12980 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12981 | ` *  Send an error message somewhere.` |
|        - | 12982 | ` * Parameter` |
|        - | 12983 | ` *  $message` |
|        - | 12984 | ` *   The error message that should be logged.` |
|        - | 12985 | ` *  $message_type` |
|        - | 12986 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12987 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12988 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12989 | ` *       This is the default option.` |
|        - | 12990 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12991 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12992 | ` *    2  No longer an option.` |
|        - | 12993 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12994 | ` *       to the end of the message string.` |
|        - | 12995 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12996 | ` *  $destination` |
|        - | 12997 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12998 | ` *  $extra_headers` |
|        - | 12999 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13000 | ` * Return` |
|        - | 13001 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13002 | ` * NOTE:` |
|        - | 13003 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13004 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13005 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13006 | ` *  Otherwise this function is no-op.` |
|        - | 13007 | ` */` |
|        4 | 13008 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13009 |  |
|        - | 13010 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13011 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13012 | `	int iType = 0;` |
|        5 | 13013 | `	if( nArg < 1 ){` |
|        - | 13014 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13015 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13016 | `		return PH7_OK;` |
|        - | 13017 | `	}` |
|        5 | 13018 | `	if( pVm->xErrLog  ){` |
|        - | 13019 | `		/* Invoke the user callback */` |
|      ! 0 | 13020 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13021 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13022 | `		if( nArg > 1 ){` |
|      ! 0 | 13023 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13024 | `			if( nArg > 2 ){` |
|      ! 0 | 13025 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13026 | `				if( nArg > 3 ){` |
|      ! 0 | 13027 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13028 | `				}` |
|      ! 0 | 13029 | `			}` |
|      ! 0 | 13030 | `		}` |
|      ! 0 | 13031 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13032 | `	}` |
|        - | 13033 | `	/* Retun TRUE */` |
|        5 | 13034 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13035 | `	return PH7_OK;` |
|        3 | 13036 |  |
|        - | 13037 | `/*` |
|        - | 13038 | ` * bool restore_exception_handler(void)` |
|        - | 13039 | ` *  Restores the previously defined exception handler function.` |
|        - | 13040 | ` * Parameter` |
|        - | 13041 | ` *  None` |
|        - | 13042 | ` * Return` |
|        - | 13043 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13044 | ` */` |
|        4 | 13045 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13046 |  |
|        5 | 13047 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13048 | `	ph7_value *pOld,*pNew;` |
|        - | 13049 | `	/* Point to the old and the new handler */` |
|        5 | 13050 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13051 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13052 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13053 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13054 | `		SXUNUSED(apArg);` |
|        - | 13055 | `		/* No installed handler,return FALSE */` |
|        5 | 13056 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13057 | `		return PH7_OK;` |
|        - | 13058 | `	}` |
|        - | 13059 | `	/* Copy the old handler */` |
|      ! 0 | 13060 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13061 | `	PH7_MemObjRelease(pOld);` |
|        - | 13062 | `	/* Return TRUE */` |
|      ! 0 | 13063 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13064 | `	return PH7_OK;` |
|        3 | 13065 |  |
|        - | 13066 | `/*` |
|        - | 13067 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13068 | ` *  Sets a user-defined exception handler function.` |
|        - | 13069 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13070 | ` * NOTE` |
|        - | 13071 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13072 | ` *  the satndard PHP engine.` |
|        - | 13073 | ` * Parameters` |
|        - | 13074 | ` *  $exception_handler` |
|        - | 13075 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13076 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13077 | ` *   that was thrown.` |
|        - | 13078 | ` *  Note:` |
|        - | 13079 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13080 | ` * Return` |
|        - | 13081 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13082 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13083 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13084 | ` */` |
|        4 | 13085 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13086 |  |
|        6 | 13087 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13088 | `	ph7_value *pOld,*pNew;` |
|        - | 13089 | `	/* Point to the old and the new handler */` |
|        6 | 13090 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13091 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13092 | `	/* Return the old handler */` |
|        6 | 13093 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13094 | `	if( nArg > 0 ){` |
|        6 | 13095 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13096 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13097 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13098 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13099 | `		}else{` |
|        6 | 13100 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13101 | `			/* Install the new handler */` |
|        6 | 13102 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13103 | `		}` |
|        2 | 13104 | `	}` |
|        6 | 13105 | `	return PH7_OK;` |
|        2 | 13106 |  |
|        - | 13107 | `/*` |
|        - | 13108 | ` * bool restore_error_handler(void)` |
|        - | 13109 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13110 | ` * Parameters:` |
|        - | 13111 | ` *  None.` |
|        - | 13112 | ` * Return` |
|        - | 13113 | ` *  Always TRUE.` |
|        - | 13114 | ` */` |
|        6 | 13115 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13116 |  |
|        7 | 13117 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13118 | `	ph7_value *pOld,*pNew;` |
|        - | 13119 | `	/* Point to the old and the new handler */` |
|        7 | 13120 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13121 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13122 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13123 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13124 | `		SXUNUSED(apArg);` |
|        - | 13125 | `		/* No installed callback,return FALSE */` |
|        7 | 13126 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13127 | `		return PH7_OK;` |
|        - | 13128 | `	}` |
|        - | 13129 | `	/* Copy the old callback */` |
|      ! 0 | 13130 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13131 | `	PH7_MemObjRelease(pOld);` |
|        - | 13132 | `	/* Return TRUE */` |
|      ! 0 | 13133 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13134 | `	return PH7_OK;` |
|        4 | 13135 |  |
|        - | 13136 | `/*` |
|        - | 13137 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13138 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13139 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13140 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13141 | ` *  Sets a user-defined error handler function.` |
|        - | 13142 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13143 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13144 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13145 | ` *  conditions (using trigger_error()).` |
|        - | 13146 | ` * Parameters` |
|        - | 13147 | ` *  $error_handler` |
|        - | 13148 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13149 | ` *   describing the error.` |
|        - | 13150 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13151 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13152 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13153 | ` *   The function can be shown as:` |
|        - | 13154 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13155 | ` *     errno` |
|        - | 13156 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13157 | ` *   errstr` |
|        - | 13158 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13159 | ` *   errfile` |
|        - | 13160 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13161 | ` *     was raised in, as a string.` |
|        - | 13162 | ` *  Note:` |
|        - | 13163 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13164 | ` * Return` |
|        - | 13165 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13166 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13167 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13168 | ` */` |
|    10756 | 13169 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13170 |  |
|    10758 | 13171 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13172 | `	ph7_value *pOld,*pNew;` |
|        - | 13173 | `	/* Point to the old and the new handler */` |
|    10758 | 13174 | `	pOld = &pVm->aErrCB[0];` |
|    10758 | 13175 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13176 | `	/* Return the old handler */` |
|    10758 | 13177 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10758 | 13178 | `	if( nArg > 0 ){` |
|    10758 | 13179 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13180 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5373 | 13181 | `			PH7_MemObjRelease(pNew);` |
|     5373 | 13182 | `			ph7_result_bool(pCtx,1);` |
|     2687 | 13183 | `		}else{` |
|     5386 | 13184 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13185 | `			/* Install the new handler */` |
|     5386 | 13186 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13187 | `		}` |
|     5378 | 13188 | `	}` |
|    10758 | 13189 | `	return PH7_OK;` |
|        2 | 13190 |  |
|        - | 13191 | `/*` |
|        - | 13192 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13193 | ` *  Generates a backtrace.` |
|        - | 13194 | ` * Paramaeter` |
|        - | 13195 | ` *  $options` |
|        - | 13196 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13197 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13198 | ` *   all the function/method arguments, to save memory.` |
|        - | 13199 | ` * $limit` |
|        - | 13200 | ` *   (Not Used)` |
|        - | 13201 | ` * Return` |
|        - | 13202 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13203 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13204 | ` *          Name        Type      Description` |
|        - | 13205 | ` *          ------      ------     -----------` |
|        - | 13206 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13207 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13208 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13209 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13210 | ` *          object      object    The current object.` |
|        - | 13211 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13212 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13213 | ` */` |
|      898 | 13214 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13215 |  |
|      900 | 13216 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13217 | `	ph7_value *pArray;` |
|        - | 13218 | `	ph7_class *pClass;` |
|        - | 13219 | `	ph7_value *pValue;` |
|        - | 13220 | `	SyString *pFile;` |
|        - | 13221 | `	/* Create a new array */` |
|      900 | 13222 | `	pArray = ph7_context_new_array(pCtx);` |
|      900 | 13223 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      900 | 13224 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13225 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13226 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13227 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13228 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13229 | `		SXUNUSED(apArg);` |
|      ! 0 | 13230 | `		return PH7_OK;` |
|        - | 13231 | `	}` |
|        - | 13232 | `	/* Dump running function name and it's arguments  */` |
|      900 | 13233 | `	if( pVm->pFrame->pParent ){` |
|      900 | 13234 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13235 | `		ph7_vm_func *pFunc;` |
|        - | 13236 | `		ph7_value *pArg;` |
|      900 | 13237 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      900 | 13238 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      900 | 13239 | `		if( pFrame->pParent && pFunc ){` |
|      900 | 13240 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      900 | 13241 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      900 | 13242 | `			ph7_value_reset_string_cursor(pValue);` |
|      449 | 13243 | `		}` |
|        - | 13244 | `		/* Function arguments */` |
|      900 | 13245 | `		pArg = ph7_context_new_array(pCtx);` |
|      900 | 13246 | `		if( pArg  ){` |
|        - | 13247 | `			ph7_value *pObj;` |
|        - | 13248 | `			VmSlot *aSlot;` |
|        - | 13249 | `			sxu32 n;` |
|        - | 13250 | `			/* Start filling the array with the given arguments */` |
|      900 | 13251 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3598 | 13252 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2700 | 13253 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2700 | 13254 | `				if( pObj ){` |
|     2700 | 13255 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1349 | 13256 | `				}` |
|     1351 | 13257 | `			}` |
|        - | 13258 | `			/* Save the array */` |
|      900 | 13259 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      449 | 13260 | `		}` |
|      449 | 13261 | `	}` |
|      900 | 13262 | `	ph7_value_int(pValue,1);` |
|        - | 13263 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13264 | `	 * line numbers at run-time. )` |
|        - | 13265 | `	 */` |
|      900 | 13266 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13267 | `	/* Current processed script */` |
|      900 | 13268 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      900 | 13269 | `	if( pFile ){` |
|      900 | 13270 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      900 | 13271 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      900 | 13272 | `		ph7_value_reset_string_cursor(pValue);` |
|      449 | 13273 | `	}` |
|        - | 13274 | `	/* Top class */` |
|      900 | 13275 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      900 | 13276 | `	if( pClass ){` |
|      896 | 13277 | `		ph7_value_reset_string_cursor(pValue);` |
|      896 | 13278 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      896 | 13279 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      447 | 13280 | `	}` |
|        - | 13281 | `	/* Return the freshly created array */` |
|      900 | 13282 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13283 | `	/*` |
|        - | 13284 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13285 | `	 * as soon we return from this function.` |
|        - | 13286 | `	 */` |
|      900 | 13287 | `	return PH7_OK;` |
|      451 | 13288 |  |
|        - | 13289 | `/*` |
|        - | 13290 | ` * Generate a small backtrace.` |
|        - | 13291 | ` * Store the generated dump in the given BLOB` |
|        - | 13292 | ` */` |
|        4 | 13293 | `static int VmMiniBacktrace(` |
|        - | 13294 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13295 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13296 | `	)` |
|        1 | 13297 |  |
|        5 | 13298 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13299 | `	ph7_vm_func *pFunc;` |
|        - | 13300 | `	ph7_class *pClass;` |
|        - | 13301 | `	SyString *pFile;` |
|        - | 13302 | `	/* Called function */` |
|        5 | 13303 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13304 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13305 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13306 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13307 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13308 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13309 | `	}else{` |
|      ! 0 | 13310 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13311 | `	}` |
|        5 | 13312 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13313 | `	/* Current processed script */` |
|        5 | 13314 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13315 | `	if( pFile ){` |
|        5 | 13316 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13317 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13318 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13319 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13320 | `	}` |
|        - | 13321 | `	/* Top class */` |
|        5 | 13322 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13323 | `	if( pClass ){` |
|      ! 0 | 13324 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13325 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13326 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13327 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13328 | `	}` |
|        5 | 13329 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13330 | `	/* All done */` |
|        5 | 13331 | `	return SXRET_OK;` |
|        1 | 13332 |  |
|        - | 13333 | `/*` |
|        - | 13334 | ` * void debug_print_backtrace()` |
|        - | 13335 | ` *  Prints a backtrace` |
|        - | 13336 | ` * Parameters` |
|        - | 13337 | ` * None` |
|        - | 13338 | ` * Return` |
|        - | 13339 | ` * NULL` |
|        - | 13340 | ` */` |
|        2 | 13341 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13342 |  |
|        3 | 13343 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13344 | `	SyBlob sDump;` |
|        3 | 13345 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13346 | `	/* Generate the backtrace */` |
|        3 | 13347 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13348 | `	/* Output backtrace */` |
|        3 | 13349 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13350 | `	/* All done,cleanup */` |
|        3 | 13351 | `	SyBlobRelease(&sDump);` |
|        1 | 13352 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13353 | `	SXUNUSED(apArg);` |
|        3 | 13354 | `	return PH7_OK;` |
|        1 | 13355 |  |
|        - | 13356 | `/*` |
|        - | 13357 | ` * string debug_string_backtrace()` |
|        - | 13358 | ` *  Generate a backtrace` |
|        - | 13359 | ` * Parameters` |
|        - | 13360 | ` * None` |
|        - | 13361 | ` * Return` |
|        - | 13362 | ` *  A mini backtrace().` |
|        - | 13363 | ` * Note that this is a symisc extension.` |
|        - | 13364 | ` */` |
|        2 | 13365 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13366 |  |
|        3 | 13367 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13368 | `	SyBlob sDump;` |
|        3 | 13369 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13370 | `	/* Generate the backtrace */` |
|        3 | 13371 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13372 | `	/* Return the backtrace */` |
|        3 | 13373 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13374 | `	/* All done,cleanup */` |
|        3 | 13375 | `	SyBlobRelease(&sDump);` |
|        1 | 13376 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13377 | `	SXUNUSED(apArg);` |
|        3 | 13378 | `	return PH7_OK;` |
|        1 | 13379 |  |
|        - | 13380 | `/*` |
|        - | 13381 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13382 | ` * exception is triggered.` |
|        - | 13383 | ` */` |
|      512 | 13384 | `static sxi32 VmUncaughtException(` |
|        - | 13385 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13386 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13387 | `	)` |
|        1 | 13388 |  |
|        - | 13389 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13390 | `	int nArg = 1;` |
|        - | 13391 | `	sxi32 rc;` |
|      513 | 13392 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13393 | `		/* Nesting limit reached */` |
|      ! 0 | 13394 | `		return SXRET_OK;` |
|        - | 13395 | `	}` |
|        - | 13396 | `	/* Call any exception handler if available */` |
|      513 | 13397 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13398 | `	if( pThis ){` |
|        - | 13399 | `		/* Load the exception instance */` |
|      513 | 13400 | `		sArg.x.pOther = pThis;` |
|      513 | 13401 | `		pThis->iRef++;` |
|      513 | 13402 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13403 | `	}else{` |
|      ! 0 | 13404 | `		nArg = 0;` |
|        - | 13405 | `	}` |
|      513 | 13406 | `	apArg[0] = &sArg;` |
|        - | 13407 | `	/* Call the exception handler if available */` |
|      513 | 13408 | `	pVm->nExceptDepth++;` |
|      513 | 13409 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13410 | `	pVm->nExceptDepth--;` |
|      513 | 13411 | `	if( rc != SXRET_OK ){` |
|        - | 13412 | `		SyBlob sMsgBuf;` |
|      511 | 13413 | `		const char *zClass = "Exception";` |
|      511 | 13414 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13415 | `		const char *zMsg;` |
|        - | 13416 | `		sxu32 nMsg;` |
|        - | 13417 | `		const char *zFuncName;` |
|        - | 13418 | `		int nFuncLen;` |
|      511 | 13419 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13420 | `		if( pThis ){` |
|        - | 13421 | `			ph7_class_method *pGetMessage;` |
|        - | 13422 | `			ph7_value sMsg;` |
|        - | 13423 | `			const char *zTmp;` |
|        - | 13424 | `			int nTmp;` |
|      511 | 13425 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13426 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13427 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13428 | `			if( pGetMessage ){` |
|      511 | 13429 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13430 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13431 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13432 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13433 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13434 | `					}` |
|      255 | 13435 | `				}` |
|      511 | 13436 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13437 | `			}` |
|      255 | 13438 | `		}` |
|      511 | 13439 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13440 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13441 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13442 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13443 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13444 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13445 | `		rc = SXERR_ABORT;` |
|      255 | 13446 | `	}` |
|      513 | 13447 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13448 | `	return rc;` |
|      257 | 13449 |  |
|        - | 13450 | `/*` |
|        - | 13451 | ` * Throw a user exception.` |
|        - | 13452 | ` *` |
|        - | 13453 | ` * Exception dispatch follows this sequence:` |
|        - | 13454 | ` *` |
|        - | 13455 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13456 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13457 | ` *` |
|        - | 13458 | ` * 2. If NO catch matches:` |
|        - | 13459 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13460 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13461 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13462 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13463 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13464 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13465 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13466 | ` *` |
|        - | 13467 | ` * 3. If a catch DOES match:` |
|        - | 13468 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13469 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13470 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13471 | ` *       finally block.` |
|        - | 13472 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13473 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13474 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13475 | ` *       in pPendingException (step 2c).` |
|        - | 13476 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13477 | ` *    d. Run finally (if present).` |
|        - | 13478 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13479 | ` *       that handlers are restored and finally has run.` |
|        - | 13480 | ` */` |
|      846 | 13481 | `static sxi32 VmThrowException(` |
|        - | 13482 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13483 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13484 | `	)` |
|        2 | 13485 |  |
|        - | 13486 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13487 | `	ph7_exception **apException;` |
|        - | 13488 | `	ph7_exception *pException;` |
|        - | 13489 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13490 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13491 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      848 | 13492 | `	VmCoalesceDisarm(pVm);` |
|        - | 13493 | `	/* Point to the stack of loaded exceptions */` |
|      848 | 13494 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      848 | 13495 | `	pException = 0;` |
|      848 | 13496 | `	pCatch = 0;` |
|      848 | 13497 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13498 | `		ph7_exception_block *aCatch;` |
|        - | 13499 | `		ph7_class *pClass;` |
|        - | 13500 | `		SyString *aNames;` |
|        - | 13501 | `		sxu32 nNames;` |
|        - | 13502 | `		int matched;` |
|        - | 13503 | `		sxu32 j,k;` |
|        - | 13504 | `		/* Locate the appropriate block to execute */` |
|      328 | 13505 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      328 | 13506 | `		(void)SySetPop(&pVm->aException);` |
|      328 | 13507 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      336 | 13508 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13509 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      334 | 13510 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      334 | 13511 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      334 | 13512 | `			matched = 0;` |
|      360 | 13513 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13514 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13515 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13516 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      352 | 13517 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      352 | 13518 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13519 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13520 | `					continue;` |
|        - | 13521 | `				}` |
|      352 | 13522 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      326 | 13523 | `					matched = 1;` |
|      326 | 13524 | `					break;` |
|        - | 13525 | `				}` |
|       14 | 13526 | `			}` |
|      334 | 13527 | `			if( matched ){` |
|        - | 13528 | `				/* Catch block found,break immediately */` |
|      326 | 13529 | `				pCatch = &aCatch[j];` |
|      326 | 13530 | `				break;` |
|        - | 13531 | `			}` |
|        5 | 13532 | `		}` |
|      163 | 13533 | `	}` |
|        - | 13534 | `	/* Execute the cached block if available */` |
|      848 | 13535 | `	if( pCatch == 0 ){` |
|        - | 13536 | `		sxi32 rc;` |
|        - | 13537 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13538 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13539 | `			pException->iFinallyDone = 1;` |
|        3 | 13540 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13541 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13542 | `				return SXERR_ABORT;` |
|        - | 13543 | `			}` |
|        1 | 13544 | `		}` |
|        - | 13545 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13546 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13547 | `			/* Re-throw to the outer handler */` |
|        3 | 13548 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13549 | `		}` |
|        - | 13550 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13551 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13552 | `		 * exception instead of reporting it uncaught.` |
|        - | 13553 | `		 */` |
|      522 | 13554 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13555 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13556 | `			 * by looking for a catch frame on the stack.` |
|        - | 13557 | `			 */` |
|      522 | 13558 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13559 | `			int inCatch = 0;` |
|     1050 | 13560 | `			while( pF ){` |
|      538 | 13561 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13562 | `					inCatch = 1;` |
|        9 | 13563 | `					break;` |
|        - | 13564 | `				}` |
|      529 | 13565 | `				pF = pF->pParent;` |
|        1 | 13566 | `			}` |
|      522 | 13567 | `			if( inCatch ){` |
|        - | 13568 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13569 | `				pThis->iRef++;` |
|        9 | 13570 | `				pVm->pPendingException = pThis;` |
|        9 | 13571 | `				return SXRET_OK;` |
|        - | 13572 | `			}` |
|      256 | 13573 | `		}` |
|        - | 13574 | `		/* Truly uncaught */` |
|      513 | 13575 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13576 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13577 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13578 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13579 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13580 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13581 | `			}` |
|      ! 0 | 13582 | `		}` |
|      513 | 13583 | `		return rc;` |
|      ! 0 | 13584 | `	}else{` |
|      326 | 13585 | `		VmFrame *pFrame = pVm->pFrame;` |
|      326 | 13586 | `		ph7_exception **apSaved = 0;` |
|        - | 13587 | `		sxu32 nSavedCount;` |
|        - | 13588 | `		sxi32 rc;` |
|      326 | 13589 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      326 | 13590 | `		if( pException->pFrame == pFrame ){` |
|      230 | 13591 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      114 | 13592 | `		}` |
|        - | 13593 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13594 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13595 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13596 | `		 */` |
|      326 | 13597 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      326 | 13598 | `		if( nSavedCount > 0 ){` |
|       16 | 13599 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13600 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13601 | `			if( apSaved ){` |
|       16 | 13602 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13603 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13604 | `				SySetReset(&pVm->aException);` |
|        5 | 13605 | `			}` |
|        5 | 13606 | `		}` |
|        - | 13607 | `		/* Create a private frame first */` |
|      326 | 13608 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      326 | 13609 | `		if( rc == SXRET_OK ){` |
|      326 | 13610 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      326 | 13611 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      326 | 13612 | `			if( pObj ){` |
|      326 | 13613 | `				pThis->iRef++;` |
|      326 | 13614 | `				pObj->x.pOther = pThis;` |
|      326 | 13615 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      162 | 13616 | `			}` |
|        - | 13617 | `			/* Execute the catch block */` |
|      326 | 13618 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13619 | `			/* Leave the frame */` |
|      326 | 13620 | `			VmLeaveFrame(&(*pVm));` |
|      162 | 13621 | `		}` |
|        - | 13622 | `		/* Restore the outer exception handlers */` |
|      326 | 13623 | `		if( apSaved ){` |
|        - | 13624 | `			sxu32 k;` |
|        - | 13625 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13626 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13627 | `			 * Restore the original outer entries.` |
|        - | 13628 | `			 */` |
|       11 | 13629 | `			SySetReset(&pVm->aException);` |
|       21 | 13630 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13631 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13632 | `			}` |
|       11 | 13633 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13634 | `		}` |
|        - | 13635 | `		/* Execute the finally block after catch */` |
|      326 | 13636 | `		if( pException->iHasFinally ){` |
|       16 | 13637 | `			pException->iFinallyDone = 1;` |
|        - | 13638 | `			{` |
|       16 | 13639 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13640 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13641 | `					return SXERR_ABORT;` |
|        - | 13642 | `				}` |
|        - | 13643 | `			}` |
|        7 | 13644 | `		}` |
|      326 | 13645 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13646 | `			return SXERR_ABORT;` |
|        - | 13647 | `		}` |
|        - | 13648 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13649 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13650 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13651 | `		 */` |
|      326 | 13652 | `		if( pVm->pPendingException ){` |
|        9 | 13653 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13654 | `			pVm->pPendingException = 0;` |
|        9 | 13655 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13656 | `		}` |
|        - | 13657 | `	}` |
|        - | 13658 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13659 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13660 | `	 */` |
|      318 | 13661 | `	return SXRET_OK;` |
|      425 | 13662 |  |
|        - | 13663 | `/*` |
|        - | 13664 | ` * Section:` |
|        - | 13665 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13666 | ` * Status:` |
|        - | 13667 | ` *    Stable.` |
|        - | 13668 | ` */` |
|        - | 13669 | `/*` |
|        - | 13670 | ` * string ph7version(void)` |
|        - | 13671 | ` *  Returns the running version of the PH7 version.` |
|        - | 13672 | ` * Parameters` |
|        - | 13673 | ` *  None` |
|        - | 13674 | ` * Return` |
|        - | 13675 | ` * Current PH7 version.` |
|        - | 13676 | ` */` |
|        2 | 13677 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13678 |  |
|        1 | 13679 | `	SXUNUSED(nArg);` |
|        1 | 13680 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13681 | `	/* Current engine version */` |
|        3 | 13682 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13683 | `	return PH7_OK;` |
|        1 | 13684 |  |
|        - | 13685 | `/*` |
|        - | 13686 | ` * string phpversion([ string $extension ])` |
|        - | 13687 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 13688 | ` * Parameters` |
|        - | 13689 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 13690 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 13691 | ` * Return` |
|        - | 13692 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 13693 | ` */` |
|        4 | 13694 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13695 |  |
|        2 | 13696 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 13697 | `	if( nArg > 0 ){` |
|      ! 0 | 13698 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13699 | `		return PH7_OK;` |
|        - | 13700 | `	}` |
|        5 | 13701 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 13702 | `	return PH7_OK;` |
|        3 | 13703 |  |
|        - | 13704 | `/*` |
|        - | 13705 | ` * string php_sapi_name(void)` |
|        - | 13706 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 13707 | ` * Parameters` |
|        - | 13708 | ` *  None` |
|        - | 13709 | ` * Return` |
|        - | 13710 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 13711 | ` */` |
|        2 | 13712 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13713 |  |
|        3 | 13714 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 13715 | `	SXUNUSED(nArg);` |
|        1 | 13716 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 13717 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 13718 | `	return PH7_OK;` |
|        1 | 13719 |  |
|        - | 13720 | `/*` |
|        - | 13721 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13722 | ` */` |
|        - | 13723 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13724 | ` "<html><head>"\` |
|        - | 13725 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13726 | ` "<style type=\"text/css\">"\` |
|        - | 13727 | ` "div {"\` |
|        - | 13728 | `     "border: 1px solid #cccccc;"\` |
|        - | 13729 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13730 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13731 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13732 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13733 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13734 | `     "-o-border-radius: 10px;"\` |
|        - | 13735 | `     "border-radius: 10px;"\` |
|        - | 13736 | `     "padding-left: 2em;"\` |
|        - | 13737 | `     "background-color: white;"\` |
|        - | 13738 | `     "margin-left: auto;"\` |
|        - | 13739 | `     "font-family: verdana;"\` |
|        - | 13740 | `     "padding-right: 2em;"\` |
|        - | 13741 | `     "margin-right: auto;"\` |
|        - | 13742 | `     "}"\` |
|        - | 13743 | `     "body {"\` |
|        - | 13744 | `     "padding: 0.2em;"\` |
|        - | 13745 | `     "font-style: normal;"\` |
|        - | 13746 | `     "font-size: medium;"\` |
|        - | 13747 | `     "background-color: #f2f2f2;"\` |
|        - | 13748 | `     "}"\` |
|        - | 13749 | `     "hr {"\` |
|        - | 13750 | `     "border-style: solid none none;"\` |
|        - | 13751 | `     "border-width: 1px medium medium;"\` |
|        - | 13752 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13753 | `     "height: 1px;"\` |
|        - | 13754 | `     "}"\` |
|        - | 13755 | `     "a {"\` |
|        - | 13756 | `     "color: #3366cc;"\` |
|        - | 13757 | `     "text-decoration: none;"\` |
|        - | 13758 | `     "}"\` |
|        - | 13759 | `     "a:hover {"\` |
|        - | 13760 | `     "color: #999999;"\` |
|        - | 13761 | `     "}"\` |
|        - | 13762 | `     "a:active {"\` |
|        - | 13763 | `     "color: #663399;"\` |
|        - | 13764 | `     "}"\` |
|        - | 13765 | `     "h1 {"\` |
|        - | 13766 | `     "margin: 0;"\` |
|        - | 13767 | `     "padding: 0;"\` |
|        - | 13768 | `     "font-family: Verdana;"\` |
|        - | 13769 | `     "font-weight: bold;"\` |
|        - | 13770 | `     "font-style: normal;"\` |
|        - | 13771 | `     "font-size: medium;"\` |
|        - | 13772 | `     "text-transform: capitalize;"\` |
|        - | 13773 | `     "color: #0a328c;"\` |
|        - | 13774 | `     "}"\` |
|        - | 13775 | `     "p {"\` |
|        - | 13776 | `     "margin: 0 auto;"\` |
|        - | 13777 | `     "font-size: medium;"\` |
|        - | 13778 | `     "font-style: normal;"\` |
|        - | 13779 | `     "font-family: verdana;"\` |
|        - | 13780 | `     "}"\` |
|        - | 13781 | `"</style></head><body>"\` |
|        - | 13782 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13783 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13784 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13785 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13786 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13787 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13788 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13789 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13790 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13791 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13792 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13793 |  |
|        - | 13794 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13795 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13796 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13797 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13798 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13799 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13800 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13801 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13802 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13803 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13804 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13805 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13806 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13807 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13808 |  |
|        - | 13809 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13810 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13811 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13812 | `"&nbsp;*<br>"\` |
|        - | 13813 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13814 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13815 | `"&nbsp;* are met:<br>"\` |
|        - | 13816 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13817 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13818 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13819 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13820 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13821 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13822 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13823 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13824 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13825 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13826 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13827 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13828 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13829 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13830 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13831 | `"&nbsp;*<br>"\` |
|        - | 13832 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13833 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13834 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13835 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13836 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13837 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13838 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13839 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13840 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13841 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13842 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13843 | `"&nbsp;*/<br>"\` |
|        - | 13844 | `"</span></small></small></p>"\` |
|        - | 13845 | `"</div></body></html>"` |
|        - | 13846 | `/*` |
|        - | 13847 | ` * bool ph7credits(void)` |
|        - | 13848 | ` * bool ph7info(void)` |
|        - | 13849 | ` * bool ph7copyright(void)` |
|        - | 13850 | ` *  Prints out the credits for PH7 engine` |
|        - | 13851 | ` * Parameters` |
|        - | 13852 | ` *  None` |
|        - | 13853 | ` * Return` |
|        - | 13854 | ` *  Always TRUE` |
|        - | 13855 | ` */` |
|        2 | 13856 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13857 |  |
|        3 | 13858 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13859 | `	/* Expand the HTML page above*/` |
|        3 | 13860 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13861 | `	ph7_context_output_format(` |
|        1 | 13862 | `		pCtx,` |
|        - | 13863 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13864 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13865 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13866 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13867 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13868 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13869 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13870 | `#ifdef __WINNT__` |
|        - | 13871 | `		"Windows NT"` |
|        - | 13872 | `#elif defined(__UNIXES__)` |
|        - | 13873 | `		"UNIX-Like"` |
|        - | 13874 | `#else` |
|        - | 13875 | `		"Other OS"` |
|        - | 13876 | `#endif` |
|        - | 13877 | `		);` |
|        3 | 13878 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13879 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13880 | `	SXUNUSED(apArg);` |
|        - | 13881 | `	/* Return TRUE */` |
|        - | 13882 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13883 | `	return PH7_OK;` |
|        1 | 13884 |  |
|        - | 13885 | `/*` |
|        - | 13886 | ` * Section:` |
|        - | 13887 | ` *    URL related routines.` |
|        - | 13888 | ` * Status:` |
|        - | 13889 | ` *    Stable.` |
|        - | 13890 | ` */` |
|        - | 13891 | `/*` |
|        - | 13892 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13893 | ` *  Parse a URL and return its fields.` |
|        - | 13894 | ` * Parameters` |
|        - | 13895 | ` *  $url` |
|        - | 13896 | ` *   The URL to parse.` |
|        - | 13897 | ` * $component` |
|        - | 13898 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13899 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13900 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13901 | ` *  in which case the return value will be an integer).` |
|        - | 13902 | ` * Return` |
|        - | 13903 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13904 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13905 | ` *  this array are:` |
|        - | 13906 | ` *   scheme - e.g. http` |
|        - | 13907 | ` *   host` |
|        - | 13908 | ` *   port` |
|        - | 13909 | ` *   user` |
|        - | 13910 | ` *   pass` |
|        - | 13911 | ` *   path` |
|        - | 13912 | ` *   query - after the question mark ?` |
|        - | 13913 | ` *   fragment - after the hashmark #` |
|        - | 13914 | ` * Note:` |
|        - | 13915 | ` *  FALSE is returned on failure.` |
|        - | 13916 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13917 | ` *  with the standard PHP engine.` |
|        - | 13918 | ` */` |
|       28 | 13919 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13920 |  |
|        - | 13921 | `	const char *zStr; /* Input string */` |
|        - | 13922 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13923 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13924 | `	int nLen;` |
|        - | 13925 | `	sxi32 rc;` |
|       29 | 13926 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13927 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13928 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13929 | `		return PH7_OK;` |
|        - | 13930 | `	}` |
|        - | 13931 | `	/* Extract the given URI */` |
|       29 | 13932 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13933 | `	if( nLen < 1 ){` |
|        - | 13934 | `		/* Nothing to process,return FALSE */` |
|        3 | 13935 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13936 | `		return PH7_OK;` |
|        - | 13937 | `	}` |
|        - | 13938 | `	/* Get a parse */` |
|       27 | 13939 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13940 | `	if( rc != SXRET_OK ){` |
|        - | 13941 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13942 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13943 | `		return PH7_OK;` |
|        - | 13944 | `	}` |
|       27 | 13945 | `	if( nArg > 1 ){` |
|      ! 0 | 13946 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13947 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13948 | `		switch(nComponent){` |
|      ! 0 | 13949 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13950 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13951 | `			if( pComp->nByte < 1 ){` |
|        - | 13952 | `				/* No available value,return NULL */` |
|      ! 0 | 13953 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13954 | `			}else{` |
|      ! 0 | 13955 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13956 | `			}` |
|      ! 0 | 13957 | `			break;` |
|      ! 0 | 13958 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13959 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13960 | `			if( pComp->nByte < 1 ){` |
|        - | 13961 | `				/* No available value,return NULL */` |
|      ! 0 | 13962 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13963 | `			}else{` |
|      ! 0 | 13964 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13965 | `			}` |
|      ! 0 | 13966 | `			break;` |
|      ! 0 | 13967 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13968 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13969 | `			if( pComp->nByte < 1 ){` |
|        - | 13970 | `				/* No available value,return NULL */` |
|      ! 0 | 13971 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13972 | `			}else{` |
|      ! 0 | 13973 | `				int iPort = 0;` |
|        - | 13974 | `				/* Cast the value to integer */` |
|      ! 0 | 13975 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13976 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13977 | `			}` |
|      ! 0 | 13978 | `			break;` |
|      ! 0 | 13979 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 13980 | `			pComp = &sURI.sUser;` |
|      ! 0 | 13981 | `			if( pComp->nByte < 1 ){` |
|        - | 13982 | `				/* No available value,return NULL */` |
|      ! 0 | 13983 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13984 | `			}else{` |
|      ! 0 | 13985 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13986 | `			}` |
|      ! 0 | 13987 | `			break;` |
|      ! 0 | 13988 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 13989 | `			pComp = &sURI.sPass;` |
|      ! 0 | 13990 | `			if( pComp->nByte < 1 ){` |
|        - | 13991 | `				/* No available value,return NULL */` |
|      ! 0 | 13992 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13993 | `			}else{` |
|      ! 0 | 13994 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13995 | `			}` |
|      ! 0 | 13996 | `			break;` |
|      ! 0 | 13997 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 13998 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 13999 | `			if( pComp->nByte < 1 ){` |
|        - | 14000 | `				/* No available value,return NULL */` |
|      ! 0 | 14001 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14002 | `			}else{` |
|      ! 0 | 14003 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14004 | `			}` |
|      ! 0 | 14005 | `			break;` |
|      ! 0 | 14006 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14007 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14008 | `			if( pComp->nByte < 1 ){` |
|        - | 14009 | `				/* No available value,return NULL */` |
|      ! 0 | 14010 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14011 | `			}else{` |
|      ! 0 | 14012 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14013 | `			}` |
|      ! 0 | 14014 | `			break;` |
|      ! 0 | 14015 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14016 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14017 | `			if( pComp->nByte < 1 ){` |
|        - | 14018 | `				/* No available value,return NULL */` |
|      ! 0 | 14019 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14020 | `			}else{` |
|      ! 0 | 14021 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14022 | `			}` |
|      ! 0 | 14023 | `			break;` |
|      ! 0 | 14024 | `		default:` |
|        - | 14025 | `			/* No such entry,return NULL */` |
|      ! 0 | 14026 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14027 | `			break;` |
|        - | 14028 | `		}` |
|      ! 0 | 14029 | `	}else{` |
|        - | 14030 | `		ph7_value *pArray,*pValue;` |
|        - | 14031 | `		/* Return an associative array */` |
|       27 | 14032 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14033 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14034 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14035 | `			/* Out of memory */` |
|      ! 0 | 14036 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14037 | `			/* Return false */` |
|      ! 0 | 14038 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14039 | `			return PH7_OK;` |
|        - | 14040 | `		}` |
|        - | 14041 | `		/* Fill the array */` |
|       27 | 14042 | `		pComp = &sURI.sScheme;` |
|       27 | 14043 | `		if( pComp->nByte > 0 ){` |
|       19 | 14044 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14045 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14046 | `		}` |
|        - | 14047 | `		/* Reset the string cursor */` |
|       27 | 14048 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14049 | `		pComp = &sURI.sHost;` |
|       27 | 14050 | `		if( pComp->nByte > 0 ){` |
|       25 | 14051 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14052 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14053 | `		}` |
|        - | 14054 | `		/* Reset the string cursor */` |
|       27 | 14055 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14056 | `		pComp = &sURI.sPort;` |
|       27 | 14057 | `		if( pComp->nByte > 0 ){` |
|       11 | 14058 | `			int iPort = 0;/* cc warning */` |
|        - | 14059 | `			/* Convert to integer */` |
|       11 | 14060 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14061 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14062 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14063 | `		}` |
|        - | 14064 | `		/* Reset the string cursor */` |
|       27 | 14065 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14066 | `		pComp = &sURI.sUser;` |
|       27 | 14067 | `		if( pComp->nByte > 0 ){` |
|        7 | 14068 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14069 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14070 | `		}` |
|        - | 14071 | `		/* Reset the string cursor */` |
|       27 | 14072 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14073 | `		pComp = &sURI.sPass;` |
|       27 | 14074 | `		if( pComp->nByte > 0 ){` |
|        7 | 14075 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14076 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14077 | `		}` |
|        - | 14078 | `		/* Reset the string cursor */` |
|       27 | 14079 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14080 | `		pComp = &sURI.sPath;` |
|       27 | 14081 | `		if( pComp->nByte > 0 ){` |
|       17 | 14082 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14083 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14084 | `		}` |
|        - | 14085 | `		/* Reset the string cursor */` |
|       27 | 14086 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14087 | `		pComp = &sURI.sQuery;` |
|       27 | 14088 | `		if( pComp->nByte > 0 ){` |
|        5 | 14089 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14090 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14091 | `		}` |
|        - | 14092 | `		/* Reset the string cursor */` |
|       27 | 14093 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14094 | `		pComp = &sURI.sFragment;` |
|       27 | 14095 | `		if( pComp->nByte > 0 ){` |
|        5 | 14096 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14097 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14098 | `		}` |
|        - | 14099 | `		/* Return the created array */` |
|       27 | 14100 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14101 | `		/* NOTE:` |
|        - | 14102 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14103 | `		 * automatically as soon we return from this function.` |
|        - | 14104 | `		 */` |
|        - | 14105 | `	}` |
|        - | 14106 | `	/* All done */` |
|       27 | 14107 | `	return PH7_OK;` |
|       15 | 14108 |  |
|        - | 14109 | `/*` |
|        - | 14110 | ` * Section:` |
|        - | 14111 | ` *   Array related routines.` |
|        - | 14112 | ` * Status:` |
|        - | 14113 | ` *    Stable.` |
|        - | 14114 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14115 | ` *  Array related functions that need access to the underlying` |
|        - | 14116 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14117 | ` */` |
|        - | 14118 | `/*` |
|        - | 14119 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14120 | ` * of the following structure.` |
|        - | 14121 | ` */` |
|        - | 14122 | `struct compact_data` |
|        - | 14123 |  |
|        - | 14124 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14125 | `	int nRecCount;      /* Recursion count */` |
|        - | 14126 | `};` |
|        - | 14127 | `/*` |
|        - | 14128 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14129 | ` */` |
|      ! 0 | 14130 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14131 |  |
|      ! 0 | 14132 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14133 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14134 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14135 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14136 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14137 | `		SyString sVar;` |
|      ! 0 | 14138 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14139 | `		if( sVar.nByte > 0 ){` |
|        - | 14140 | `			/* Query the current frame */` |
|      ! 0 | 14141 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14142 | `			/* ^` |
|        - | 14143 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14144 | `			 */` |
|      ! 0 | 14145 | `			if( pKey ){` |
|        - | 14146 | `				/* Perform the insertion */` |
|      ! 0 | 14147 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14148 | `			}` |
|      ! 0 | 14149 | `		}` |
|      ! 0 | 14150 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14151 | `		int rc;` |
|        - | 14152 | `		/* Recursively traverse this array */` |
|      ! 0 | 14153 | `		pData->nRecCount++;` |
|      ! 0 | 14154 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14155 | `		pData->nRecCount--;` |
|      ! 0 | 14156 | `		return rc;` |
|        - | 14157 | `	}` |
|      ! 0 | 14158 | `	return SXRET_OK;` |
|      ! 0 | 14159 |  |
|        - | 14160 | `/*` |
|        - | 14161 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14162 | ` *  Create array containing variables and their values.` |
|        - | 14163 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14164 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14165 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14166 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14167 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14168 | ` * Parameters` |
|        - | 14169 | ` *  $varname` |
|        - | 14170 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14171 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14172 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14173 | ` *   it recursively.` |
|        - | 14174 | ` * Return` |
|        - | 14175 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14176 | ` */` |
|        2 | 14177 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14178 |  |
|        - | 14179 | `	ph7_value *pArray,*pObj;` |
|        3 | 14180 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14181 | `	const char *zName;` |
|        - | 14182 | `	SyString sVar;` |
|        - | 14183 | `	int i,nLen;` |
|        3 | 14184 | `	if( nArg < 1 ){` |
|        - | 14185 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14186 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14187 | `		return PH7_OK;` |
|        - | 14188 | `	}` |
|        - | 14189 | `	/* Create the array */` |
|        3 | 14190 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14191 | `	if( pArray == 0 ){` |
|        - | 14192 | `		/* Out of memory */` |
|      ! 0 | 14193 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14194 | `		/* Return NULL */` |
|      ! 0 | 14195 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14196 | `		return PH7_OK;` |
|        - | 14197 | `	}` |
|        - | 14198 | `	/* Perform the requested operation */` |
|        7 | 14199 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14200 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14201 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14202 | `				struct compact_data sData;` |
|      ! 0 | 14203 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14204 | `				/* Recursively walk the array */` |
|      ! 0 | 14205 | `				sData.nRecCount = 0;` |
|      ! 0 | 14206 | `				sData.pArray = pArray;` |
|      ! 0 | 14207 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14208 | `			}` |
|      ! 0 | 14209 | `		}else{` |
|        - | 14210 | `			/* Extract variable name */` |
|        5 | 14211 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14212 | `			if( nLen > 0 ){` |
|        5 | 14213 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14214 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14215 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14216 | `				if( pObj ){` |
|        5 | 14217 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14218 | `				}` |
|        2 | 14219 | `			}` |
|        - | 14220 | `		}` |
|        3 | 14221 | `	}` |
|        - | 14222 | `	/* Return the array */` |
|        3 | 14223 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14224 | `	return PH7_OK;` |
|        2 | 14225 |  |
|        - | 14226 | `/*` |
|        - | 14227 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14228 | ` * of the following structure.` |
|        - | 14229 | ` */` |
|        - | 14230 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14231 | `struct extract_aux_data` |
|        - | 14232 |  |
|        - | 14233 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14234 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14235 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14236 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14237 | `	int iFlags;           /* Control flags */` |
|        - | 14238 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14239 | `};` |
|        - | 14240 | `/* Forward declaration */` |
|        - | 14241 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14242 | `/*` |
|        - | 14243 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14244 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14245 | ` * Parameters` |
|        - | 14246 | ` * $var_array` |
|        - | 14247 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14248 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14249 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14250 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14251 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14252 | ` * $extract_type` |
|        - | 14253 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14254 | ` *  It can be one of the following values:` |
|        - | 14255 | ` *   EXTR_OVERWRITE` |
|        - | 14256 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14257 | ` *   EXTR_SKIP` |
|        - | 14258 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14259 | ` *   EXTR_PREFIX_SAME` |
|        - | 14260 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14261 | ` *   EXTR_PREFIX_ALL` |
|        - | 14262 | ` *       Prefix all variable names with prefix.` |
|        - | 14263 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14264 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14265 | ` *   EXTR_IF_EXISTS` |
|        - | 14266 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14267 | ` *       otherwise do nothing.` |
|        - | 14268 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14269 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14270 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14271 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14272 | ` *      the current symbol table.` |
|        - | 14273 | ` * $prefix` |
|        - | 14274 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14275 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14276 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14277 | ` *  underscore character.` |
|        - | 14278 | ` * Return` |
|        - | 14279 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14280 | ` */` |
|        4 | 14281 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14282 |  |
|        - | 14283 | `	extract_aux_data sAux;` |
|        - | 14284 | `	ph7_hashmap *pMap;` |
|        5 | 14285 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14286 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14287 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14288 | `		return PH7_OK;` |
|        - | 14289 | `	}` |
|        - | 14290 | `	/* Point to the target hashmap */` |
|        5 | 14291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14292 | `	if( pMap->nEntry < 1 ){` |
|        - | 14293 | `		/* Empty map,return  0 */` |
|      ! 0 | 14294 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14295 | `		return PH7_OK;` |
|        - | 14296 | `	}` |
|        - | 14297 | `	/* Prepare the aux data */` |
|        5 | 14298 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14299 | `	if( nArg > 1 ){` |
|        3 | 14300 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14301 | `		if( nArg > 2 ){` |
|      ! 0 | 14302 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14303 | `		}` |
|        1 | 14304 | `	}` |
|        5 | 14305 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14306 | `	/* Invoke the worker callback */` |
|        5 | 14307 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14308 | `	/* Number of variables successfully imported */` |
|        5 | 14309 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14310 | `	return PH7_OK;` |
|        3 | 14311 |  |
|        - | 14312 | `/*` |
|        - | 14313 | ` * Worker callback for the [extract()] function defined` |
|        - | 14314 | ` * below.` |
|        - | 14315 | ` */` |
|        8 | 14316 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14317 |  |
|        9 | 14318 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14319 | `	int iFlags = pAux->iFlags;` |
|        9 | 14320 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14321 | `	ph7_value *pObj;` |
|        - | 14322 | `	SyString sVar;` |
|        9 | 14323 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14324 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14325 | `	}` |
|        - | 14326 | `	/* Perform a string cast */` |
|        9 | 14327 | `	PH7_MemObjToString(pKey);` |
|        9 | 14328 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14329 | `		/* Unavailable variable name */` |
|      ! 0 | 14330 | `		return SXRET_OK;` |
|        - | 14331 | `	}` |
|        9 | 14332 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14333 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14334 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14335 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14336 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14337 | `			);` |
|      ! 0 | 14338 | `	}else{` |
|       13 | 14339 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14340 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14341 | `	}` |
|        9 | 14342 | `	sVar.zString = pAux->zWorker;` |
|        - | 14343 | `	/* Try to extract the variable */` |
|        9 | 14344 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14345 | `	if( pObj ){` |
|        - | 14346 | `		/* Collision */` |
|        5 | 14347 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14348 | `			return SXRET_OK;` |
|        - | 14349 | `		}` |
|        5 | 14350 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14351 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14352 | `				/* Already prefixed */` |
|      ! 0 | 14353 | `				return SXRET_OK;` |
|        - | 14354 | `			}` |
|      ! 0 | 14355 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14356 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14357 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14358 | `				);` |
|      ! 0 | 14359 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14360 | `		}` |
|        3 | 14361 | `	}else{` |
|        - | 14362 | `		/* Create the variable */` |
|        5 | 14363 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14364 | `	}` |
|        9 | 14365 | `	if( pObj ){` |
|        - | 14366 | `		/* Overwrite the old value */` |
|        9 | 14367 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14368 | `		/* Increment counter */` |
|        9 | 14369 | `		pAux->iCount++;` |
|        4 | 14370 | `	}` |
|        9 | 14371 | `	return SXRET_OK;` |
|        5 | 14372 |  |
|        - | 14373 | `/*` |
|        - | 14374 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14375 | ` * defined below.` |
|        - | 14376 | ` */` |
|        2 | 14377 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14378 |  |
|        3 | 14379 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14380 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14381 | `	ph7_value *pObj;` |
|        - | 14382 | `	SyString sVar;` |
|        - | 14383 | `	/* Perform a string cast */` |
|        3 | 14384 | `	PH7_MemObjToString(pKey);` |
|        3 | 14385 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14386 | `		/* Unavailable variable name */` |
|      ! 0 | 14387 | `		return SXRET_OK;` |
|        - | 14388 | `	}` |
|        3 | 14389 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14390 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14391 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14392 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14393 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14394 | `			);` |
|        2 | 14395 | `	}else{` |
|      ! 0 | 14396 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14397 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14398 | `	}` |
|        3 | 14399 | `	sVar.zString = pAux->zWorker;` |
|        - | 14400 | `	/* Extract the variable */` |
|        3 | 14401 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14402 | `	if( pObj ){` |
|        3 | 14403 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14404 | `	}` |
|        3 | 14405 | `	return SXRET_OK;` |
|        2 | 14406 |  |
|        - | 14407 | `/*` |
|        - | 14408 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14409 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14410 | ` * Parameters` |
|        - | 14411 | ` * $types` |
|        - | 14412 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14413 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14414 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14415 | ` *  POST includes the POST uploaded file information.` |
|        - | 14416 | ` *  Note:` |
|        - | 14417 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14418 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14419 | ` * $prefix` |
|        - | 14420 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14421 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14422 | ` *  variable named $pref_userid.` |
|        - | 14423 | ` * Return` |
|        - | 14424 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14425 | ` */` |
|        2 | 14426 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14427 |  |
|        - | 14428 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14429 | `	extract_aux_data sAux;` |
|        - | 14430 | `	int nLen,nPrefixLen;` |
|        - | 14431 | `	ph7_value *pSuper;` |
|        - | 14432 | `	ph7_vm *pVm;` |
|        - | 14433 | `	/* By default import only $_GET variables  */` |
|        3 | 14434 | `	zImport = "G";` |
|        3 | 14435 | `	nLen = (int)sizeof(char);` |
|        3 | 14436 | `	zPrefix = 0;` |
|        3 | 14437 | `	nPrefixLen = 0;` |
|        3 | 14438 | `	if( nArg > 0 ){` |
|        3 | 14439 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14440 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14441 | `		}` |
|        3 | 14442 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14443 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14444 | `		}` |
|        1 | 14445 | `	}` |
|        - | 14446 | `	/* Point to the underlying VM */` |
|        3 | 14447 | `	pVm = pCtx->pVm;` |
|        - | 14448 | `	/* Initialize the aux data */` |
|        3 | 14449 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14450 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14451 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14452 | `	sAux.pVm = pVm;` |
|        - | 14453 | `	/* Extract */` |
|        3 | 14454 | `	zEnd = &zImport[nLen];` |
|        5 | 14455 | `	while( zImport < zEnd ){` |
|        3 | 14456 | `		int c = zImport[0];` |
|        3 | 14457 | `		pSuper = 0;` |
|        3 | 14458 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14459 | `			/* Import $_GET variables */` |
|        3 | 14460 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14461 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14462 | `			/* Import $_POST variables */` |
|      ! 0 | 14463 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14464 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14465 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14466 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14467 | `		}` |
|        3 | 14468 | `		if( pSuper ){` |
|        - | 14469 | `			/* Iterate throw array entries */` |
|        3 | 14470 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14471 | `		}` |
|        - | 14472 | `		/* Advance the cursor */` |
|        3 | 14473 | `		zImport++;` |
|        1 | 14474 | `	}` |
|        - | 14475 | `	/* All done,return TRUE*/` |
|        3 | 14476 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14477 | `	return PH7_OK;` |
|        1 | 14478 |  |
|        - | 14479 | `/*` |
|        - | 14480 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14481 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14482 | ` * information.` |
|        - | 14483 | ` */` |
|    12638 | 14484 | `static sxi32 VmEvalChunk(` |
|        - | 14485 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14486 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14487 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14488 | `	int iFlags,         /* Compile flag */` |
|        - | 14489 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14490 | `	)` |
|        2 | 14491 |  |
|        - | 14492 | `	SySet *pByteCode,aByteCode;` |
|        - | 14493 | `	SyBlob sSavedNs;` |
|    12640 | 14494 | `	ProcConsumer xErr = 0;` |
|    12640 | 14495 | `	void *pErrData = 0;` |
|        - | 14496 | `	/* Initialize bytecode container */` |
|    12640 | 14497 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12640 | 14498 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14499 | `	/* Reset the code generator */` |
|    12640 | 14500 | `	if( bTrueReturn ){` |
|        - | 14501 | `		/* Included file,log compile-time errors */` |
|     9484 | 14502 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9484 | 14503 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4741 | 14504 | `	}` |
|    12640 | 14505 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14506 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14507 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14508 | `	 * the caller's namespace is restored. */` |
|    12640 | 14509 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12640 | 14510 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12640 | 14511 | `	if( bTrueReturn ){` |
|        - | 14512 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9484 | 14513 | `		SyBlobReset(&pVm->sNamespace);` |
|     4741 | 14514 | `	}` |
|        - | 14515 | `	/* Swap bytecode container */` |
|    12640 | 14516 | `	pByteCode = pVm->pByteContainer;` |
|    12640 | 14517 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14518 | `	/* Compile the chunk */` |
|    12640 | 14519 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    18959 | 14520 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14521 | `		/* Compilation error,return false */` |
|        3 | 14522 | `		if( pCtx ){` |
|        3 | 14523 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14524 | `		}` |
|        2 | 14525 | `	}else{` |
|        - | 14526 | `		/* Mount any newly defined classes */` |
|        - | 14527 | `		SyHashEntry *pEntry;` |
|        - | 14528 | `		ph7_class *pClass;` |
|        - | 14529 | `		ph7_value sResult; /* Return value */` |
|        - | 14530 | `		sxi32 rc;` |
|    12638 | 14531 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   747404 | 14532 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   728450 | 14533 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14534 | `			/* Only mount classes that haven't been mounted yet */` |
|   728450 | 14535 | `			if( !pClass->bMounted ){` |
|   191196 | 14536 | `				rc = VmMountUserClass(pVm,pClass);` |
|   191196 | 14537 | `				if( rc != SXRET_OK ){` |
|        - | 14538 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14539 | `					if( pCtx ){` |
|      ! 0 | 14540 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14541 | `					}` |
|      ! 0 | 14542 | `					goto Cleanup;` |
|        - | 14543 | `				}` |
|    95597 | 14544 | `			}` |
|        2 | 14545 | `		}` |
|    12638 | 14546 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14547 | `			/* Out of memory */` |
|      ! 0 | 14548 | `			if( pCtx ){` |
|      ! 0 | 14549 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14550 | `			}` |
|      ! 0 | 14551 | `			goto Cleanup;` |
|        - | 14552 | `		}` |
|    12638 | 14553 | `		if( bTrueReturn ){` |
|        - | 14554 | `			/* Assume a boolean true return value */` |
|     9484 | 14555 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4743 | 14556 | `		}else{` |
|        - | 14557 | `			/* Assume a null return value */` |
|     3156 | 14558 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14559 | `		}` |
|        - | 14560 | `		/* Execute the compiled chunk */` |
|    12638 | 14561 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12638 | 14562 | `		if( pCtx ){` |
|        - | 14563 | `			/* Set the execution result */` |
|     9504 | 14564 | `			ph7_result_value(pCtx,&sResult);` |
|     4751 | 14565 | `		}` |
|    12638 | 14566 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14567 | `	}` |
|     6319 | 14568 | `Cleanup:` |
|        - | 14569 | `	/* Cleanup the mess left behind */` |
|    12640 | 14570 | `	pVm->pByteContainer = pByteCode;` |
|    12640 | 14571 | `	SySetRelease(&aByteCode);` |
|        - | 14572 | `	/* Restore caller's namespace state */` |
|    12640 | 14573 | `	SyBlobReset(&pVm->sNamespace);` |
|    12640 | 14574 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12640 | 14575 | `	SyBlobRelease(&sSavedNs);` |
|    12640 | 14576 | `	return SXRET_OK;` |
|        2 | 14577 |  |
|        - | 14578 | `/*` |
|        - | 14579 | ` * value eval(string $code)` |
|        - | 14580 | ` *   Evaluate a string as PHP code.` |
|        - | 14581 | ` * Parameter` |
|        - | 14582 | ` *  code: PHP code to evaluate.` |
|        - | 14583 | ` * Return` |
|        - | 14584 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14585 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14586 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14587 | ` */` |
|       24 | 14588 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14589 |  |
|        - | 14590 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 14591 | `	if( nArg < 1 ){` |
|        - | 14592 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14593 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14594 | `		return SXRET_OK;` |
|        - | 14595 | `	}` |
|        - | 14596 | `	/* Chunk to evaluate */` |
|       26 | 14597 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 14598 | `	if( sChunk.nByte < 1 ){` |
|        - | 14599 | `		/* Empty string,return NULL */` |
|        3 | 14600 | `		ph7_result_null(pCtx);` |
|        3 | 14601 | `		return SXRET_OK;` |
|        - | 14602 | `	}` |
|        - | 14603 | `	/* Eval the chunk */` |
|       24 | 14604 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 14605 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14606 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 14607 | `		return PH7_ABORT;` |
|        - | 14608 | `	}` |
|       22 | 14609 | `	return SXRET_OK;` |
|       14 | 14610 |  |
|        - | 14611 | `/*` |
|        - | 14612 | ` * Check if a file path is already included.` |
|        - | 14613 | ` */` |
|    18960 | 14614 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14615 |  |
|        - | 14616 | `	SyString *aEntries;` |
|        - | 14617 | `	sxu32 n;` |
|    18962 | 14618 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14619 | `	/* Perform a linear search */` |
| 89738640 | 14620 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 89719686 | 14621 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14622 | `			/* Already included */` |
|        7 | 14623 | `			return TRUE;` |
|        - | 14624 | `		}` |
| 44859841 | 14625 | `	}` |
|    18956 | 14626 | `	return FALSE;` |
|     9482 | 14627 |  |
|        - | 14628 | `/*` |
|        - | 14629 | ` * Push a file path in the appropriate VM container.` |
|        - | 14630 | ` */` |
|    22086 | 14631 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14632 |  |
|        - | 14633 | `	SyString sPath;` |
|        - | 14634 | `	char *zDup;` |
|        - | 14635 | `#ifdef __WINNT__` |
|        - | 14636 | `	char *zCur;` |
|        - | 14637 | `#endif` |
|        - | 14638 | `	sxi32 rc;` |
|    22088 | 14639 | `	if( nLen < 0 ){` |
|     3128 | 14640 | `		nLen = SyStrlen(zPath);` |
|     1563 | 14641 | `	}` |
|        - | 14642 | `	/* Duplicate the file path first */` |
|    22088 | 14643 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22088 | 14644 | `	if( zDup == 0 ){` |
|      ! 0 | 14645 | `		return SXERR_MEM;` |
|        - | 14646 | `	}` |
|        - | 14647 | `#ifdef __WINNT__` |
|        - | 14648 | `	/* Normalize path on windows` |
|        - | 14649 | `	 * Example:` |
|        - | 14650 | `	 *    Path/To/File.php` |
|        - | 14651 | `	 * becomes` |
|        - | 14652 | `	 *   path\to\file.php` |
|        - | 14653 | `	 */` |
|        2 | 14654 | `	zCur = zDup;` |
|        2 | 14655 | `	while( zCur[0] != 0 ){` |
|        2 | 14656 | `		if( zCur[0] == '/' ){` |
|        2 | 14657 | `			zCur[0] = '\\';` |
|        2 | 14658 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14659 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14660 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14661 | `		}` |
|        2 | 14662 | `		zCur++;` |
|        2 | 14663 | `	}` |
|        - | 14664 | `#endif` |
|        - | 14665 | `	/* Install the file path */` |
|    22088 | 14666 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22088 | 14667 | `	if( !bMain ){` |
|    18962 | 14668 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14669 | `			/* Already included */` |
|        7 | 14670 | `			*pNew = 0;` |
|        4 | 14671 | `		}else{` |
|        - | 14672 | `			/* Insert in the corresponding container */` |
|    18956 | 14673 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    18956 | 14674 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14675 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14676 | `				return rc;` |
|        - | 14677 | `			}` |
|    18956 | 14678 | `			*pNew = 1;` |
|        - | 14679 | `		}` |
|     9480 | 14680 | `	}` |
|    22088 | 14681 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22088 | 14682 | `	return SXRET_OK;` |
|    11045 | 14683 |  |
|        - | 14684 | `/*` |
|        - | 14685 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14686 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14687 | ` * indicates failure.` |
|        - | 14688 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14689 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14690 | ` * operations.` |
|        - | 14691 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14692 | ` * this function is a no-op.` |
|        - | 14693 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14694 | ` * constructs for more information.` |
|        - | 14695 | ` */` |
|     9492 | 14696 | `static sxi32 VmExecIncludedFile(` |
|        - | 14697 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14698 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14699 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14700 | `	 )` |
|        2 | 14701 |  |
|        - | 14702 | `	sxi32 rc;` |
|        - | 14703 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14704 | `	const ph7_io_stream *pStream;` |
|        - | 14705 | `	SyBlob sContents;` |
|        - | 14706 | `	void *pHandle;` |
|        - | 14707 | `	ph7_vm *pVm;` |
|        - | 14708 | `	int isNew;` |
|        - | 14709 | `	/* Initialize fields */` |
|     9494 | 14710 | `	pVm = pCtx->pVm;` |
|     9494 | 14711 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9494 | 14712 | `	isNew = 0;` |
|        - | 14713 | `	/* Extract the associated stream */` |
|     9494 | 14714 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14715 | `	/*` |
|        - | 14716 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14717 | `	 * in a read-only mode.` |
|        - | 14718 | `	 */` |
|     9494 | 14719 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9494 | 14720 | `	if( pHandle == 0 ){` |
|        8 | 14721 | `		return SXERR_IO;` |
|        - | 14722 | `	}` |
|     9488 | 14723 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9488 | 14724 | `	if( IncludeOnce && !isNew ){` |
|        - | 14725 | `		/* Already included */` |
|        5 | 14726 | `		rc = SXERR_EXISTS;` |
|        3 | 14727 | `	}else{` |
|        - | 14728 | `		/* Read the whole file contents */` |
|     9484 | 14729 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9484 | 14730 | `		if( rc == SXRET_OK ){` |
|        - | 14731 | `			SyString sScript;` |
|        - | 14732 | `			/* Compile and execute the script */` |
|     9484 | 14733 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9484 | 14734 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4741 | 14735 | `		}` |
|        - | 14736 | `	}` |
|        - | 14737 | `	/* Pop from the set of included file */` |
|     9488 | 14738 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14739 | `	/* Close the handle */` |
|     9488 | 14740 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14741 | `	/* Release the working buffer */` |
|     9488 | 14742 | `	SyBlobRelease(&sContents);` |
|        - | 14743 | `#else` |
|        - | 14744 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14745 | `	SXUNUSED(pPath);` |
|        - | 14746 | `	SXUNUSED(IncludeOnce);` |
|        - | 14747 | `	rc = SXERR_IO;` |
|        - | 14748 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9488 | 14749 | `	return rc;` |
|     4748 | 14750 |  |
|        - | 14751 | `/*` |
|        - | 14752 | ` * string get_include_path(void)` |
|        - | 14753 | ` *  Gets the current include_path configuration option.` |
|        - | 14754 | ` * Parameter` |
|        - | 14755 | ` *  None` |
|        - | 14756 | ` * Return` |
|        - | 14757 | ` *  Included paths as a string` |
|        - | 14758 | ` */` |
|        2 | 14759 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14760 |  |
|        3 | 14761 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14762 | `	SyString *aEntry;` |
|        - | 14763 | `	int dir_sep;` |
|        - | 14764 | `	sxu32 n;` |
|        - | 14765 | `#ifdef __WINNT__` |
|        1 | 14766 | `	dir_sep = ';';` |
|        - | 14767 | `#else` |
|        - | 14768 | `	/* Assume UNIX path separator */` |
|        2 | 14769 | `	dir_sep = ':';` |
|        - | 14770 | `#endif` |
|        1 | 14771 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14772 | `	SXUNUSED(apArg);` |
|        - | 14773 | `	/* Point to the list of import paths */` |
|        3 | 14774 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14775 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14776 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14777 | `		if( n > 0 ){` |
|        - | 14778 | `			/* Append dir seprator */` |
|      ! 0 | 14779 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14780 | `		}` |
|        - | 14781 | `		/* Append path */` |
|        3 | 14782 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14783 | `	}` |
|        3 | 14784 | `	return PH7_OK;` |
|        1 | 14785 |  |
|        - | 14786 | `/*` |
|        - | 14787 | ` * string get_get_included_files(void)` |
|        - | 14788 | ` *  Gets the current include_path configuration option.` |
|        - | 14789 | ` * Parameter` |
|        - | 14790 | ` *  None` |
|        - | 14791 | ` * Return` |
|        - | 14792 | ` *  Included paths as a string` |
|        - | 14793 | ` */` |
|        2 | 14794 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14795 |  |
|        3 | 14796 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14797 | `	ph7_value *pArray,*pWorker;` |
|        - | 14798 | `	SyString *pEntry;` |
|        - | 14799 | `	int c,d;` |
|        - | 14800 | `	/* Create an array and a working value */` |
|        3 | 14801 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14802 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14803 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14804 | `		/* Out of memory,return null */` |
|      ! 0 | 14805 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14806 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14807 | `		SXUNUSED(apArg);` |
|      ! 0 | 14808 | `		return PH7_OK;` |
|        - | 14809 | `	}` |
|        3 | 14810 | `	c = d = '/';` |
|        - | 14811 | `#ifdef __WINNT__` |
|        1 | 14812 | `	d = '\\';` |
|        - | 14813 | `#endif` |
|        - | 14814 | `	/* Iterate throw entries */` |
|        3 | 14815 | `	SySetResetCursor(pFiles);` |
|     3917 | 14816 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14817 | `		const char *zBase,*zEnd;` |
|        - | 14818 | `		int iLen;` |
|        - | 14819 | `		/* reset the string cursor */` |
|     3915 | 14820 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14821 | `		/* Extract base name */` |
|     3915 | 14822 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14823 | `		/* Ignore trailing '/' */` |
|     5872 | 14824 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14825 | `			zEnd--;` |
|      ! 0 | 14826 | `		}` |
|     3915 | 14827 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 14828 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 14829 | `			zEnd--;` |
|        1 | 14830 | `		}` |
|     3915 | 14831 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 14832 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14833 | `		/* Copy entry name */` |
|     3915 | 14834 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14835 | `		/* Perform the insertion */` |
|     3915 | 14836 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14837 | `	}` |
|        - | 14838 | `	/* All done,return the created array */` |
|        3 | 14839 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14840 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14841 | `	 * by the engine as soon we return from this foreign` |
|        - | 14842 | `	 * function.` |
|        - | 14843 | `	 */` |
|        3 | 14844 | `	return PH7_OK;` |
|        2 | 14845 |  |
|        - | 14846 | `/*` |
|        - | 14847 | ` * include:` |
|        - | 14848 | ` * According to the PHP reference manual.` |
|        - | 14849 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14850 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14851 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14852 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14853 | ` *  and the current working directory before failing. The include()` |
|        - | 14854 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14855 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14856 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14857 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14858 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14859 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14860 | ` *  directory to find the requested file.` |
|        - | 14861 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14862 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14863 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14864 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14865 | ` */` |
|     9474 | 14866 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14867 |  |
|        - | 14868 | `	SyString sFile;` |
|        - | 14869 | `	sxi32 rc;` |
|     9476 | 14870 | `	if( nArg < 1 ){` |
|        - | 14871 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14872 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14873 | `		return SXRET_OK;` |
|        - | 14874 | `	}` |
|        - | 14875 | `	/* File to include */` |
|     9476 | 14876 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9476 | 14877 | `	if( sFile.nByte < 1 ){` |
|        - | 14878 | `		/* Empty string,return NULL */` |
|      ! 0 | 14879 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14880 | `		return SXRET_OK;` |
|        - | 14881 | `	}` |
|        - | 14882 | `	/* Open,compile and execute the desired script */` |
|     9476 | 14883 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9476 | 14884 | `	if( rc != SXRET_OK ){` |
|        - | 14885 | `		/* Emit a warning and return false */` |
|        3 | 14886 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14887 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14888 | `	}` |
|     9476 | 14889 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14890 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 14891 | `		return PH7_ABORT;` |
|        - | 14892 | `	}` |
|     9472 | 14893 | `	return SXRET_OK;` |
|     4739 | 14894 |  |
|        - | 14895 | `/*` |
|        - | 14896 | ` * include_once:` |
|        - | 14897 | ` *  According to the PHP reference manual.` |
|        - | 14898 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14899 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14900 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14901 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14902 | ` *   just once.` |
|        - | 14903 | ` */` |
|        4 | 14904 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14905 |  |
|        - | 14906 | `	SyString sFile;` |
|        - | 14907 | `	sxi32 rc;` |
|        5 | 14908 | `	if( nArg < 1 ){` |
|        - | 14909 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14910 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14911 | `		return SXRET_OK;` |
|        - | 14912 | `	}` |
|        - | 14913 | `	/* File to include */` |
|        5 | 14914 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14915 | `	if( sFile.nByte < 1 ){` |
|        - | 14916 | `		/* Empty string,return NULL */` |
|      ! 0 | 14917 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14918 | `		return SXRET_OK;` |
|        - | 14919 | `	}` |
|        - | 14920 | `	/* Open,compile and execute the desired script */` |
|        5 | 14921 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14922 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14923 | `		/* File already included,return TRUE */` |
|        3 | 14924 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14925 | `		return SXRET_OK;` |
|        - | 14926 | `	}` |
|        3 | 14927 | `	if( rc != SXRET_OK ){` |
|        - | 14928 | `		/* Emit a warning and return false */` |
|      ! 0 | 14929 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14930 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14931 | ` 	}` |
|        3 | 14932 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14933 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 14934 | `		return PH7_ABORT;` |
|        - | 14935 | `	}` |
|        3 | 14936 | `	return SXRET_OK;` |
|        3 | 14937 |  |
|        - | 14938 | `/*` |
|        - | 14939 | ` * require.` |
|        - | 14940 | ` *  According to the PHP reference manual.` |
|        - | 14941 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14942 | ` *   also produce a fatal level error.` |
|        - | 14943 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14944 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14945 | ` */` |
|        6 | 14946 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14947 |  |
|        - | 14948 | `	SyString sFile;` |
|        - | 14949 | `	sxi32 rc;` |
|        8 | 14950 | `	if( nArg < 1 ){` |
|        - | 14951 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14952 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14953 | `		return SXRET_OK;` |
|        - | 14954 | `	}` |
|        - | 14955 | `	/* File to include */` |
|        8 | 14956 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14957 | `	if( sFile.nByte < 1 ){` |
|        - | 14958 | `		/* Empty string,return NULL */` |
|      ! 0 | 14959 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14960 | `		return SXRET_OK;` |
|        - | 14961 | `	}` |
|        - | 14962 | `	/* Open,compile and execute the desired script */` |
|        8 | 14963 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14964 | `	if( rc != SXRET_OK ){` |
|        - | 14965 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14966 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14967 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14968 | `		return PH7_ABORT;` |
|        - | 14969 | `	}` |
|        8 | 14970 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14971 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 14972 | `		return PH7_ABORT;` |
|        - | 14973 | `	}` |
|        8 | 14974 | `	return SXRET_OK;` |
|        5 | 14975 |  |
|        - | 14976 | `/*` |
|        - | 14977 | ` * require_once:` |
|        - | 14978 | ` *  According to the PHP reference manual.` |
|        - | 14979 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 14980 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 14981 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 14982 | ` *   and how it differs from its non _once siblings.` |
|        - | 14983 | ` */` |
|        4 | 14984 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14985 |  |
|        - | 14986 | `	SyString sFile;` |
|        - | 14987 | `	sxi32 rc;` |
|        5 | 14988 | `	if( nArg < 1 ){` |
|        - | 14989 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14990 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14991 | `		return SXRET_OK;` |
|        - | 14992 | `	}` |
|        - | 14993 | `	/* File to include */` |
|        5 | 14994 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14995 | `	if( sFile.nByte < 1 ){` |
|        - | 14996 | `		/* Empty string,return NULL */` |
|      ! 0 | 14997 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14998 | `		return SXRET_OK;` |
|        - | 14999 | `	}` |
|        - | 15000 | `	/* Open,compile and execute the desired script */` |
|        5 | 15001 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15002 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15003 | `		/* File already included,return TRUE */` |
|        3 | 15004 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15005 | `		return SXRET_OK;` |
|        - | 15006 | `	}` |
|        3 | 15007 | `	if( rc != SXRET_OK ){` |
|        - | 15008 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15009 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15010 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15011 | `		return PH7_ABORT;` |
|        - | 15012 | `	}` |
|        3 | 15013 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15014 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15015 | `		return PH7_ABORT;` |
|        - | 15016 | `	}` |
|        3 | 15017 | `	return SXRET_OK;` |
|        3 | 15018 |  |
|        - | 15019 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15020 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15021 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15022 | `/*` |
|        - | 15023 | ` * Section:` |
|        - | 15024 | ` *  SPL Autoloading functions.` |
|        - | 15025 | ` * Status:` |
|        - | 15026 | ` *  Stable.` |
|        - | 15027 | ` */` |
|        - | 15028 | `/*` |
|        - | 15029 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15030 | ` *  Register given function as __autoload() implementation.` |
|        - | 15031 | ` * Parameters` |
|        - | 15032 | ` *  callback` |
|        - | 15033 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15034 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15035 | ` *  throw` |
|        - | 15036 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15037 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15038 | ` *  prepend` |
|        - | 15039 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15040 | ` *   autoload stack instead of appending it.` |
|        - | 15041 | ` * Return` |
|        - | 15042 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15043 | ` */` |
|       34 | 15044 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15045 |  |
|        - | 15046 | `	VmAutoloadCB sEntry;` |
|       36 | 15047 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15048 | `	int iPrepend = 0;` |
|        - | 15049 | `	sxu32 n;` |
|       36 | 15050 | `	if( nArg < 1 ){` |
|        - | 15051 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15052 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15053 | `		/* Check for duplicates first */` |
|        9 | 15054 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15055 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15056 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15057 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15058 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15059 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15060 | `				return SXRET_OK;` |
|        - | 15061 | `			}` |
|      ! 0 | 15062 | `		}` |
|        5 | 15063 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15064 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15065 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15066 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15067 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15068 | `		return SXRET_OK;` |
|        - | 15069 | `	}` |
|        - | 15070 | `	/* Validate that the callback is callable */` |
|       28 | 15071 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15072 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15073 | `		if( nArg >= 2 ){` |
|      ! 0 | 15074 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15075 | `		}` |
|      ! 0 | 15076 | `		if( iThrow ){` |
|      ! 0 | 15077 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15078 | `				"Argument is not callable");` |
|      ! 0 | 15079 | `		}` |
|      ! 0 | 15080 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15081 | `		return SXRET_OK;` |
|        - | 15082 | `	}` |
|        - | 15083 | `	/* Check for duplicates */` |
|       46 | 15084 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15085 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15086 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15087 | `			/* Already registered */` |
|      ! 0 | 15088 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15089 | `			return SXRET_OK;` |
|        - | 15090 | `		}` |
|       11 | 15091 | `	}` |
|        - | 15092 | `	/* Check prepend flag */` |
|       28 | 15093 | `	if( nArg >= 3 ){` |
|        3 | 15094 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15095 | `	}` |
|        - | 15096 | `	/* Store the callback */` |
|       28 | 15097 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15098 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15099 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15100 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15101 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15102 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15103 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15104 | `		VmAutoloadCB *aBase;` |
|        3 | 15105 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15106 | `		/* Rotate: move last entry to front */` |
|        3 | 15107 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15108 | `		if( aBase ){` |
|        - | 15109 | `			VmAutoloadCB sTemp;` |
|        - | 15110 | `			sxu32 i;` |
|        3 | 15111 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15112 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15113 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15114 | `			}` |
|        3 | 15115 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15116 | `		}` |
|        2 | 15117 | `	}else{` |
|       26 | 15118 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15119 | `	}` |
|       28 | 15120 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15121 | `	return SXRET_OK;` |
|       19 | 15122 |  |
|        - | 15123 | `/*` |
|        - | 15124 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15125 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15126 | ` * Parameters` |
|        - | 15127 | ` *  callback` |
|        - | 15128 | ` *   The autoload function being unregistered.` |
|        - | 15129 | ` * Return` |
|        - | 15130 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15131 | ` */` |
|       32 | 15132 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15133 |  |
|       34 | 15134 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15135 | `	sxu32 n,nEntry;` |
|       34 | 15136 | `	if( nArg < 1 ){` |
|      ! 0 | 15137 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15138 | `		return SXRET_OK;` |
|        - | 15139 | `	}` |
|       34 | 15140 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15141 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15142 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15143 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15144 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15145 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15146 | `			sxu32 i;` |
|       32 | 15147 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15148 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15149 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15150 | `			}` |
|        - | 15151 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15152 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15153 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15154 | `			return SXRET_OK;` |
|        - | 15155 | `		}` |
|        3 | 15156 | `	}` |
|        3 | 15157 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15158 | `	return SXRET_OK;` |
|       18 | 15159 |  |
|        - | 15160 | `/*` |
|        - | 15161 | ` * array spl_autoload_functions(void)` |
|        - | 15162 | ` *  Return all registered __autoload() functions.` |
|        - | 15163 | ` * Return` |
|        - | 15164 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15165 | ` *  an empty array is returned.` |
|        - | 15166 | ` */` |
|       20 | 15167 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15168 |  |
|       21 | 15169 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15170 | `	ph7_value *pArray;` |
|        - | 15171 | `	sxu32 n,nEntry;` |
|       10 | 15172 | `	SXUNUSED(nArg);` |
|       10 | 15173 | `	SXUNUSED(apArg);` |
|       21 | 15174 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15175 | `	if( pArray == 0 ){` |
|      ! 0 | 15176 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15177 | `		return SXRET_OK;` |
|        - | 15178 | `	}` |
|       21 | 15179 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15180 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15181 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15182 | `		if( pEntry ){` |
|       15 | 15183 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15184 | `		}` |
|        8 | 15185 | `	}` |
|       21 | 15186 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15187 | `	return SXRET_OK;` |
|       11 | 15188 |  |
|        - | 15189 | `/*` |
|        - | 15190 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15191 | ` *  Default implementation of __autoload().` |
|        - | 15192 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15193 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15194 | ` * Parameters` |
|        - | 15195 | ` *  class` |
|        - | 15196 | ` *   The class name being searched.` |
|        - | 15197 | ` *  file_extensions` |
|        - | 15198 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15199 | ` */` |
|        2 | 15200 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15201 |  |
|        - | 15202 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15203 | `	SyBlob sPath;` |
|        - | 15204 | `	int nClass;` |
|        - | 15205 | `	sxi32 rc;` |
|        3 | 15206 | `	if( nArg < 1 ){` |
|      ! 0 | 15207 | `		return SXRET_OK;` |
|        - | 15208 | `	}` |
|        3 | 15209 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15210 | `	if( nClass < 1 ){` |
|      ! 0 | 15211 | `		return SXRET_OK;` |
|        - | 15212 | `	}` |
|        - | 15213 | `	/* Default extensions */` |
|        3 | 15214 | `	zExt = ".php,.inc";` |
|        3 | 15215 | `	if( nArg >= 2 ){` |
|        - | 15216 | `		int nExt;` |
|      ! 0 | 15217 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15218 | `		if( nExt < 1 ){` |
|      ! 0 | 15219 | `			zExt = ".php,.inc";` |
|      ! 0 | 15220 | `		}` |
|      ! 0 | 15221 | `	}` |
|        3 | 15222 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15223 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15224 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15225 | `	zCur = zExt;` |
|        7 | 15226 | `	while( zCur < zEnd ){` |
|        - | 15227 | `		const char *zComma;` |
|        - | 15228 | `		SyString sFile;` |
|        - | 15229 | `		int i;` |
|        - | 15230 | `		/* Find next comma or end */` |
|        5 | 15231 | `		zComma = zCur;` |
|       21 | 15232 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15233 | `			zComma++;` |
|        1 | 15234 | `		}` |
|        - | 15235 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15236 | `		SyBlobReset(&sPath);` |
|       69 | 15237 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15238 | `			char c = zClass[i];` |
|       65 | 15239 | `			if( c == '\\' ){` |
|      ! 0 | 15240 | `				c = '/';` |
|       65 | 15241 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15242 | `				c = c + ('a' - 'A');` |
|        6 | 15243 | `			}` |
|       65 | 15244 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15245 | `		}` |
|        - | 15246 | `		/* Append extension */` |
|        5 | 15247 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15248 | `		/* Try to include the file */` |
|        5 | 15249 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15250 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15251 | `		if( rc == SXRET_OK ){` |
|        - | 15252 | `			/* File included successfully */` |
|      ! 0 | 15253 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15254 | `			return SXRET_OK;` |
|        - | 15255 | `		}` |
|        - | 15256 | `		/* Move past the comma */` |
|        5 | 15257 | `		zCur = zComma;` |
|        5 | 15258 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15259 | `			zCur++;` |
|        1 | 15260 | `		}` |
|        1 | 15261 | `	}` |
|        3 | 15262 | `	SyBlobRelease(&sPath);` |
|        3 | 15263 | `	return SXRET_OK;` |
|        2 | 15264 |  |
|        - | 15265 | `/* Table of built-in VM functions. */` |
|        - | 15266 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15267 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15268 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15269 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15270 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15271 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15272 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15273 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15274 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15275 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15276 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15277 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15278 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15279 | `	    /* Constants management */` |
|        - | 15280 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15281 | `	{ "define",   vm_builtin_define               },` |
|        - | 15282 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15283 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15284 | `	   /* Class/Object functions */` |
|        - | 15285 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15286 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15287 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15288 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15289 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15290 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15291 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15292 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15293 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15294 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15295 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15296 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15297 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15298 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15299 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15300 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15301 | `	   /* SPL Autoloading */` |
|        - | 15302 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15303 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15304 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15305 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15306 | `	   /* Random numbers/strings generators */` |
|        - | 15307 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15308 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15309 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15310 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15311 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15312 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15313 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15314 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15315 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15316 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15317 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15318 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15319 | `	   /* Language constructs functions */` |
|        - | 15320 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15321 | `	{ "print", vm_builtin_print                   },` |
|        - | 15322 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15323 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15324 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15325 | `	  /* Variable handling functions */` |
|        - | 15326 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15327 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15328 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15329 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15330 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15331 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15332 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15333 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15334 | `	  /* Ouput control functions */` |
|        - | 15335 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15336 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15337 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15338 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15339 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15340 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15341 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15342 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15343 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15344 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15345 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15346 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15347 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15348 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15349 | `	  /* Assertion functions */` |
|        - | 15350 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15351 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15352 | `	  /* Error reporting functions */` |
|        - | 15353 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15354 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15355 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15356 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15357 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15358 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15359 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15360 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15361 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15362 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15363 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15364 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15365 | `	  /* Release info */` |
|        - | 15366 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15367 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15368 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15369 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15370 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15371 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15372 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15373 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15374 | `	  /* hashmap */` |
|        - | 15375 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15376 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15377 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15378 | `	  /* URL related function */` |
|        - | 15379 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15380 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15381 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15382 | `	   /* XML processing functions */` |
|        - | 15383 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15384 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15385 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15386 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15387 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15388 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15389 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15390 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15391 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15392 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15393 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15394 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15395 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15396 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15397 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15398 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15399 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15400 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15401 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15402 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15403 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15404 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15405 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15406 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15407 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15408 | `	   /* Command line processing */` |
|        - | 15409 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15410 | `	   /* JSON encoding/decoding */` |
|        - | 15411 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15412 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15413 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15414 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15415 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15416 | `	   /* Files/URI inclusion facility */` |
|        - | 15417 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15418 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15419 | `	{ "include",      vm_builtin_include          },` |
|        - | 15420 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15421 | `	{ "require",      vm_builtin_require          },` |
|        - | 15422 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15423 | `};` |
|        - | 15424 | `/*` |
|        - | 15425 | ` * Register the built-in VM functions defined above.` |
|        - | 15426 | ` */` |
|     2820 | 15427 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15428 |  |
|        - | 15429 | `	sxi32 rc;` |
|        - | 15430 | `	sxu32 n;` |
|   375062 | 15431 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15432 | `		/* Note that these special functions have access` |
|        - | 15433 | `		 * to the underlying virtual machine as their` |
|        - | 15434 | `		 * private data.` |
|        - | 15435 | `		 */` |
|   372242 | 15436 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   372242 | 15437 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15438 | `			return rc;` |
|        - | 15439 | `		}` |
|   186122 | 15440 | `	}` |
|     2822 | 15441 | `	return SXRET_OK;` |
|     1412 | 15442 |  |
|        - | 15443 | `/*` |
|        - | 15444 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15445 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15446 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15447 | ` */` |
|    97300 | 15448 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15449 |  |
|    97302 | 15450 | `	if( !iLoadable ){` |
|    95270 | 15451 | `		return pClass;` |
|        - | 15452 | `	}` |
|     2038 | 15453 | `	while(pClass){` |
|     2034 | 15454 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2030 | 15455 | `			return pClass;` |
|        - | 15456 | `		}` |
|        5 | 15457 | `		pClass = pClass->pNextName;` |
|        1 | 15458 | `	}` |
|        5 | 15459 | `	return 0;` |
|    48652 | 15460 |  |
|        - | 15461 | `/*` |
|        - | 15462 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15463 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15464 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15465 | ` * registered in the VM's class table.` |
|        - | 15466 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15467 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15468 | ` */` |
|       38 | 15469 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15470 |  |
|        - | 15471 | `	VmAutoloadCB *pEntry;` |
|        - | 15472 | `	ph7_value sArg,sResult;` |
|        - | 15473 | `	SyHashEntry *pHashEntry;` |
|        - | 15474 | `	ph7_class *pClass;` |
|        - | 15475 | `	sxu32 n,nEntry;` |
|       40 | 15476 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15477 | `	if( nEntry < 1 ){` |
|       26 | 15478 | `		return 0;` |
|        - | 15479 | `	}` |
|        - | 15480 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15481 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15482 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15483 | `	}` |
|        - | 15484 | `	/* Mark this class as being autoloaded */` |
|       14 | 15485 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15486 | `	/* Prepare the class name argument */` |
|       14 | 15487 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15488 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15489 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15490 | `	pClass = 0;` |
|       28 | 15491 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15492 | `		ph7_value *apArg[1];` |
|       24 | 15493 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15494 | `		if( pEntry == 0 ){` |
|      ! 0 | 15495 | `			continue;` |
|        - | 15496 | `		}` |
|       24 | 15497 | `		apArg[0] = &sArg;` |
|       24 | 15498 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15499 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15500 | `			continue;` |
|        - | 15501 | `		}` |
|        - | 15502 | `		/* Check if the class is now available */` |
|       24 | 15503 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15504 | `		if( pHashEntry ){` |
|       10 | 15505 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15506 | `			if( pClass ){` |
|       10 | 15507 | `				break;` |
|        - | 15508 | `			}` |
|      ! 0 | 15509 | `		}` |
|        9 | 15510 | `	}` |
|       14 | 15511 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15512 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15513 | `	/* Remove reentrancy guard */` |
|       14 | 15514 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15515 | `	return pClass;` |
|       21 | 15516 |  |
|        - | 15517 | `/*` |
|        - | 15518 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15519 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15520 | ` */` |
|       18 | 15521 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15522 |  |
|       20 | 15523 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15524 |  |
|        - | 15525 | `/*` |
|        - | 15526 | ` * Check if the given name refer to an installed class.` |
|        - | 15527 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15528 | ` */` |
|    97312 | 15529 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15530 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15531 | `	const char *zName,  /* Name of the target class */` |
|        - | 15532 | `	sxu32 nByte,        /* zName length */` |
|        - | 15533 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15534 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15535 | `						 */` |
|        - | 15536 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15537 | `	)` |
|        2 | 15538 |  |
|        - | 15539 | `	SyHashEntry *pEntry;` |
|        - | 15540 | `	ph7_class *pClass;` |
|    48656 | 15541 | `	SXUNUSED(iNest);` |
|        - | 15542 | `	/* Exact class lookup.` |
|        - | 15543 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15544 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    97314 | 15545 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    97314 | 15546 | `	if( pEntry == 0 ){` |
|        - | 15547 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15548 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15549 | `	}` |
|    97294 | 15550 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    97294 | 15551 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    48658 | 15552 |  |
|        - | 15553 | `/*` |
|        - | 15554 | ` * Reference Table Implementation` |
|        - | 15555 | ` * Status: stable <chm@symisc.net>` |
|        - | 15556 | ` * Intro` |
|        - | 15557 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15558 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15559 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15560 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15561 | ` *  Refer to the official for more information on this powerful` |
|        - | 15562 | ` *  extension.` |
|        - | 15563 | ` */` |
|        - | 15564 | `/*` |
|        - | 15565 | ` * Allocate a new reference entry.` |
|        - | 15566 | ` */` |
|  3192214 | 15567 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15568 |  |
|        - | 15569 | `	VmRefObj *pRef;` |
|        - | 15570 | `	/* Allocate a new instance */` |
|  3192216 | 15571 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3192216 | 15572 | `	if( pRef == 0 ){` |
|      ! 0 | 15573 | `		return 0;` |
|        - | 15574 | `	}` |
|        - | 15575 | `	/* Zero the structure */` |
|  3192216 | 15576 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15577 | `	/* Initialize fields */` |
|  3192216 | 15578 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3192216 | 15579 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3192216 | 15580 | `	pRef->nIdx = nIdx;` |
|  3192216 | 15581 | `	return pRef;` |
|  1596109 | 15582 |  |
|        - | 15583 | `/*` |
|        - | 15584 | ` * Default hash function used by the reference table` |
|        - | 15585 | ` * for lookup/insertion operations.` |
|        - | 15586 | ` */` |
| 17496852 | 15587 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15588 |  |
|        - | 15589 | `	/* Calculate the hash based on the memory object index */` |
| 17496854 | 15590 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15591 |  |
|        - | 15592 | `/*` |
|        - | 15593 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15594 | ` * in the reference table.` |
|        - | 15595 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15596 | ` * otherwise.` |
|        - | 15597 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15598 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15599 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15600 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15601 | ` * Refer to the official for more information on this powerful` |
|        - | 15602 | ` * extension.` |
|        - | 15603 | ` */` |
|  9517406 | 15604 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15605 |  |
|        - | 15606 | `	VmRefObj *pRef;` |
|        - | 15607 | `	sxu32 nBucket;` |
|        - | 15608 | `	/* Point to the appropriate bucket */` |
|  9517408 | 15609 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15610 | `	/* Perform the lookup */` |
|  9517408 | 15611 | `	pRef = pVm->apRefObj[nBucket];` |
| 20878919 | 15612 | `	for(;;){` |
| 41748421 | 15613 | `		if( pRef == 0 ){` |
|  3296916 | 15614 | `			break;` |
|        - | 15615 | `		}` |
| 38451507 | 15616 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15617 | `			/* Entry found */` |
|  6220494 | 15618 | `			return pRef;` |
|        - | 15619 | `		}` |
|        - | 15620 | `		/* Point to the next entry */` |
| 32231015 | 15621 | `		pRef = pRef->pNextCollide;` |
|        2 | 15622 | `	}` |
|        - | 15623 | `	/* No such entry,return NULL */` |
|  3296916 | 15624 | `	return 0;` |
|  4758705 | 15625 |  |
|        - | 15626 | `/*` |
|        - | 15627 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15628 | ` *` |
|        - | 15629 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15630 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15631 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15632 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15633 | ` * Refer to the official for more information on this powerful` |
|        - | 15634 | ` * extension.` |
|        - | 15635 | ` */` |
|  3192214 | 15636 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15637 |  |
|        - | 15638 | `	sxu32 nBucket;` |
|  3192216 | 15639 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15640 | `		VmRefObj **apNew;` |
|        - | 15641 | `		sxu32 nNew;` |
|        - | 15642 | `		/* Allocate a larger table */` |
|     4472 | 15643 | `		nNew = pVm->nRefSize << 1;` |
|     4472 | 15644 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4472 | 15645 | `		if( apNew ){` |
|     4472 | 15646 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15647 | `			sxu32 n;` |
|        - | 15648 | `			/* Zero the structure */` |
|     4472 | 15649 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15650 | `			/* Rehash all referenced entries */` |
|  2847974 | 15651 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15652 | `				/* Remove old collision links */` |
|  2843504 | 15653 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15654 | `				/* Point to the appropriate bucket */` |
|  2843504 | 15655 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15656 | `				/* Insert the entry  */` |
|  2843504 | 15657 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843504 | 15658 | `				if( apNew[nBucket] ){` |
|  2301116 | 15659 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15660 | `				}` |
|  2843504 | 15661 | `				apNew[nBucket] = pEntry;` |
|        - | 15662 | `				/* Point to the next entry */` |
|  2843504 | 15663 | `				pEntry = pEntry->pNext;` |
|  1421753 | 15664 | `			}` |
|        - | 15665 | `			/* Release the old table */` |
|     4472 | 15666 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15667 | `			/* Install the new one */` |
|     4472 | 15668 | `			pVm->apRefObj = apNew;` |
|     4472 | 15669 | `			pVm->nRefSize = nNew;` |
|     2235 | 15670 | `		}` |
|     2235 | 15671 | `	}` |
|        - | 15672 | `	/* Point to the appropriate bucket */` |
|  3192216 | 15673 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15674 | `	/* Insert the entry */` |
|  3192216 | 15675 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3192216 | 15676 | `	if( pVm->apRefObj[nBucket] ){` |
|  2607706 | 15677 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1303819 | 15678 | `	}` |
|  3192216 | 15679 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3192216 | 15680 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3192216 | 15681 | `	pVm->nRefUsed++;` |
|  3192216 | 15682 | `	return SXRET_OK;` |
|        2 | 15683 |  |
|        - | 15684 | `/*` |
|        - | 15685 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15686 | ` * the reference table.` |
|        - | 15687 | ` * This function is invoked when the user perform an unset` |
|        - | 15688 | ` * call [i.e: unset($var); ].` |
|        - | 15689 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15690 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15691 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15692 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15693 | ` * Refer to the official for more information on this powerful` |
|        - | 15694 | ` * extension.` |
|        - | 15695 | ` */` |
|  3151098 | 15696 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15697 |  |
|        - | 15698 | `	ph7_hashmap_node **apNode;` |
|        - | 15699 | `	SyHashEntry **apEntry;` |
|        - | 15700 | `	sxu32 n;` |
|        - | 15701 | `	/* Point to the reference table */` |
|  3151100 | 15702 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3151100 | 15703 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15704 | `	/* Unlink the entry from the reference table */` |
|  3261664 | 15705 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   110566 | 15706 | `		if( apEntry[n] ){` |
|   110516 | 15707 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55257 | 15708 | `		}` |
|    55284 | 15709 | `	}` |
|  6191814 | 15710 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3040716 | 15711 | `		if( apNode[n] ){` |
|     6812 | 15712 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3405 | 15713 | `		}` |
|  1520359 | 15714 | `	}` |
|  3151100 | 15715 | `	if( pRef->pPrevCollide ){` |
|  1207370 | 15716 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   604214 | 15717 | `	}else{` |
|  1943732 | 15718 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15719 | `	}` |
|  3151100 | 15720 | `	if( pRef->pNextCollide ){` |
|  1794863 | 15721 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   897423 | 15722 | `	}` |
|  3151100 | 15723 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15724 | `	/* Release the node */` |
|  3151100 | 15725 | `	SySetRelease(&pRef->aReference);` |
|  3151100 | 15726 | `	SySetRelease(&pRef->aArrEntries);` |
|  3151100 | 15727 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3151100 | 15728 | `	pVm->nRefUsed--;` |
|  3151100 | 15729 | `	return SXRET_OK;` |
|        2 | 15730 |  |
|        - | 15731 | `/*` |
|        - | 15732 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15733 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15734 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15735 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15736 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15737 | ` * Refer to the official for more information on this powerful` |
|        - | 15738 | ` * extension.` |
|        - | 15739 | ` */` |
|  3227626 | 15740 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15741 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15742 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15743 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15744 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15745 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15746 | `	)` |
|        2 | 15747 |  |
|  3227628 | 15748 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15749 | `	VmRefObj *pRef;` |
|        - | 15750 | `	/* Check if the referenced object already exists */` |
|  3227628 | 15751 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3227628 | 15752 | `	if( pRef == 0 ){` |
|        - | 15753 | `		/* Create a new entry */` |
|  3192216 | 15754 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3192216 | 15755 | `		if( pRef == 0 ){` |
|      ! 0 | 15756 | `			return SXERR_MEM;` |
|        - | 15757 | `		}` |
|  3192216 | 15758 | `		pRef->iFlags = iFlags;` |
|        - | 15759 | `		/* Install the entry */` |
|  3192216 | 15760 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1596107 | 15761 | `	}` |
|  3227628 | 15762 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3227628 | 15763 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15764 | `		VmSlot sRef;` |
|        - | 15765 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15766 | `		 * be deleted when we leave this frame.` |
|        - | 15767 | `		 */` |
|   104810 | 15768 | `		sRef.nIdx = nIdx;` |
|   104810 | 15769 | `		sRef.pUserData = pEntry;` |
|   104810 | 15770 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15771 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15772 | `		}` |
|    52404 | 15773 | `	}` |
|  3227628 | 15774 | `	if( pEntry ){` |
|        - | 15775 | `		/* Address of the hash-entry */` |
|   139998 | 15776 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    69998 | 15777 | `	}` |
|  3227628 | 15778 | `	if( pMapEntry ){` |
|        - | 15779 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3079430 | 15780 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1539714 | 15781 | `	}` |
|  3227628 | 15782 | `	return SXRET_OK;` |
|  1613815 | 15783 |  |
|        - | 15784 | `/*` |
|        - | 15785 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15786 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15787 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15788 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15789 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15790 | ` * Refer to the official for more information on this powerful` |
|        - | 15791 | ` * extension.` |
|        - | 15792 | ` */` |
|  3138676 | 15793 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15794 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15795 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15796 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15797 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15798 | `	)` |
|        2 | 15799 |  |
|        - | 15800 | `	VmRefObj *pRef;` |
|        - | 15801 | `	sxu32 n;` |
|        - | 15802 | `	/* Check if the referenced object already exists */` |
|  3138678 | 15803 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3138678 | 15804 | `	if( pRef == 0 ){` |
|        - | 15805 | `		/* Not such entry */` |
|   104696 | 15806 | `		return SXERR_NOTFOUND;` |
|        - | 15807 | `	}` |
|        - | 15808 | `	/* Remove the desired entry */` |
|  3033984 | 15809 | `	if( pEntry ){` |
|        - | 15810 | `		SyHashEntry **apEntry;` |
|       74 | 15811 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 15812 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 15813 | `			if( apEntry[n] == pEntry ){` |
|        - | 15814 | `				/* Nullify the entry */` |
|       74 | 15815 | `				apEntry[n] = 0;` |
|        - | 15816 | `				/*` |
|        - | 15817 | `				 * NOTE:` |
|        - | 15818 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15819 | `				 * we avoid wasting spaces.` |
|        - | 15820 | `				 */` |
|       36 | 15821 | `			}` |
|       97 | 15822 | `		}` |
|       36 | 15823 | `	}` |
|  3033984 | 15824 | `	if( pMapEntry ){` |
|        - | 15825 | `		ph7_hashmap_node **apNode;` |
|  3033912 | 15826 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6067916 | 15827 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3034006 | 15828 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15829 | `				/* nullify the entry */` |
|  3033912 | 15830 | `				apNode[n] = 0;` |
|  1516955 | 15831 | `			}` |
|  1517004 | 15832 | `		}` |
|  1516955 | 15833 | `	}` |
|  3033984 | 15834 | `	return SXRET_OK;` |
|  1569340 | 15835 |  |
|        - | 15836 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15837 | `/*` |
|        - | 15838 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15839 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15840 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15841 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15842 | ` * For more information on how to register IO stream devices,please` |
|        - | 15843 | ` * refer to the official documentation.` |
|        - | 15844 | ` */` |
|    28888 | 15845 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15846 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15847 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15848 | `	int nByte              /* *pzDevice length*/` |
|        - | 15849 | `	)` |
|        2 | 15850 |  |
|        - | 15851 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15852 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15853 | `	SyString sDev,sCur;` |
|        - | 15854 | `	sxu32 n,nEntry;` |
|        - | 15855 | `	int rc;` |
|        - | 15856 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    28890 | 15857 | `	zNext = zCur = zIn = *pzDevice;` |
|    28890 | 15858 | `	zEnd = &zIn[nByte];` |
|  1843514 | 15859 | `	while( zIn < zEnd ){` |
|  1814628 | 15860 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15861 | `			/* Got one */` |
|        3 | 15862 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15863 | `			break;` |
|        - | 15864 | `		}` |
|        - | 15865 | `		/* Advance the cursor */` |
|  1814626 | 15866 | `		zIn++;` |
|        2 | 15867 | `	}` |
|    28890 | 15868 | `	if( zIn >= zEnd ){` |
|        - | 15869 | `		/* No such scheme,return the default stream */` |
|    28888 | 15870 | `		return pVm->pDefStream;` |
|        - | 15871 | `	}` |
|        3 | 15872 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15873 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15874 | `	SyStringFullTrim(&sDev);` |
|        - | 15875 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15876 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15877 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15878 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15879 | `		pStream = apStream[n];` |
|        3 | 15880 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15881 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15882 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15883 | `		if( rc == 0 ){` |
|        - | 15884 | `			/* Stream device found */` |
|        3 | 15885 | `			*pzDevice = zNext;` |
|        3 | 15886 | `			return pStream;` |
|        - | 15887 | `		}` |
|      ! 0 | 15888 | `	}` |
|        - | 15889 | `	/* No such stream,return NULL */` |
|      ! 0 | 15890 | `	return 0;` |
|    14446 | 15891 |  |
|        - | 15892 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15893 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15894 |  |
