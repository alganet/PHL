# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6548/8410 lines (77.86%)

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
|   916832 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   916834 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   916800 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   916790 |   148 | `	return FALSE;` |
|   458440 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335448 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335450 |   162 | `	sxu8 bReal = FALSE;` |
|   335450 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335450 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335380 |   166 | `		return FALSE;` |
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
|   167748 |   183 |  |
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
|  1387766 |   248 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1387768 |   259 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1387768 |   260 | `	if( pFunc == 0 ){` |
|      ! 0 |   261 | `		return SXERR_MEM;` |
|        - |   262 | `	}` |
|        - |   263 | `	/* Duplicate function name */` |
|  1387768 |   264 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1387768 |   265 | `	if( zDup == 0 ){` |
|      ! 0 |   266 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   267 | `		return SXERR_MEM;` |
|        - |   268 | `	}` |
|        - |   269 | `	/* Zero the structure */` |
|  1387768 |   270 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   271 | `	/* Initialize structure fields */` |
|  1387768 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1387768 |   273 | `	pFunc->pVm   = pVm;` |
|  1387768 |   274 | `	pFunc->xFunc = xFunc;` |
|  1387768 |   275 | `	pFunc->pUserData = pUserData;` |
|  1387768 |   276 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   277 | `	/* Write a pointer to the new function */` |
|  1387768 |   278 | `	*ppOut = pFunc;` |
|  1387768 |   279 | `	return SXRET_OK;` |
|   693885 |   280 |  |
|        - |   281 | `/*` |
|        - |   282 | ` * Install a foreign function and it's associated callback so that` |
|        - |   283 | ` * it can be invoked from the target PHP code.` |
|        - |   284 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   285 | ` * return value indicates failure.` |
|        - |   286 | ` * Please refer to the official documentation for an introduction to` |
|        - |   287 | ` * the foreign function mechanism.` |
|        - |   288 | ` */` |
|  1390586 |   289 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1390588 |   300 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1390588 |   301 | `	if( pEntry ){` |
|     2822 |   302 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2822 |   303 | `		pFunc->pUserData = pUserData;` |
|     2822 |   304 | `		pFunc->xFunc = xFunc;` |
|     2822 |   305 | `		SySetReset(&pFunc->aAux);` |
|     2822 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* Create a new user function */` |
|  1387768 |   309 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1387768 |   310 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   311 | `		return rc;` |
|        - |   312 | `	}` |
|        - |   313 | `	/* Install the function in the corresponding hashtable */` |
|  1387768 |   314 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1387768 |   315 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   316 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   317 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   318 | `		return rc;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* User function successfully installed */` |
|  1387768 |   321 | `	return SXRET_OK;` |
|   695295 |   322 |  |
|        - |   323 | `/*` |
|        - |   324 | ` * Initialize a VM function.` |
|        - |   325 | ` */` |
|   277712 |   326 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   327 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   328 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   329 | `	const char *zName,  /* Function name */` |
|        - |   330 | `	sxu32 nByte,        /* zName length */` |
|        - |   331 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   332 | `	void *pUserData     /* Function private data */` |
|        - |   333 | `	)` |
|        2 |   334 |  |
|        - |   335 | `	/* Zero the structure */` |
|   277714 |   336 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   337 | `	/* Initialize structure fields */` |
|        - |   338 | `	/* Arguments container */` |
|   277714 |   339 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   340 | `	/* Static variable container */` |
|   277714 |   341 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   342 | `	/* Bytecode container */` |
|   277714 |   343 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   344 | `    /* Preallocate some instruction slots */` |
|   277714 |   345 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   346 | `	/* Closure environment */` |
|   277714 |   347 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   348 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   277714 |   349 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   277714 |   350 | `	pFunc->iFlags = iFlags;` |
|   277714 |   351 | `	pFunc->pUserData = pUserData;` |
|        - |   352 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   353 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   277714 |   354 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   277714 |   355 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   277714 |   356 | `	return SXRET_OK;` |
|        2 |   357 |  |
|        - |   358 | `/*` |
|        - |   359 | ` * Namespace-aware function lookup.` |
|        - |   360 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   361 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   362 | ` */` |
|        - |   363 | `/*` |
|        - |   364 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   365 | ` */` |
|  1454218 |   366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   367 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   368 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   369 | `	SyString *pName     /* Function name */` |
|        - |   370 | `	)` |
|        2 |   371 |  |
|        - |   372 | `	SyHashEntry *pEntry;` |
|        - |   373 | `	sxi32 rc;` |
|  1454220 |   374 | `	if( pName == 0 ){` |
|        - |   375 | `		/* Use the built-in name */` |
|    41812 |   376 | `		pName = &pFunc->sName;` |
|    20905 |   377 | `	}` |
|        - |   378 | `	/* Check for duplicates (functions with the same name) first */` |
|  1454220 |   379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1454220 |   380 | `	if( pEntry ){` |
|  1258282 |   381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1258282 |   382 | `		if( pLink != pFunc ){` |
|        - |   383 | `			/* Link */` |
|      188 |   384 | `			pFunc->pNextName = pLink;` |
|      188 |   385 | `			pEntry->pUserData = pFunc;` |
|       93 |   386 | `		}` |
|  1258282 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|        - |   389 | `	/* First time seen */` |
|   195940 |   390 | `	pFunc->pNextName = 0;` |
|   195940 |   391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   195940 |   392 | `	return rc;` |
|   727111 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   396 | ` */` |
|   120102 |   397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   398 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   399 | `	ph7_class *pClass /* Target Class */` |
|        - |   400 | `	)` |
|        2 |   401 |  |
|   120104 |   402 | `	SyString *pName = &pClass->sName;` |
|        - |   403 | `	SyHashEntry *pEntry;` |
|        - |   404 | `	sxi32 rc;` |
|        - |   405 | `	/* Check for duplicates */` |
|   120104 |   406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120104 |   407 | `	if( pEntry ){` |
|       31 |   408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   409 | `		/* Link entry with the same name */` |
|       31 |   410 | `		pClass->pNextName = pLink;` |
|       31 |   411 | `		pEntry->pUserData = pClass;` |
|       31 |   412 | `		return SXRET_OK;` |
|        - |   413 | `	}` |
|   120074 |   414 | `	pClass->pNextName = 0;` |
|        - |   415 | `	/* Perform a simple hashtable insertion */` |
|   120074 |   416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120074 |   417 | `	return rc;` |
|    60053 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Instruction builder interface.` |
|        - |   421 | ` */` |
|  4249486 |   422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4249488 |   434 | `	sInstr.iOp = (sxu8)iOp;` |
|  4249488 |   435 | `	sInstr.iP1 = iP1;` |
|  4249488 |   436 | `	sInstr.iP2 = iP2;` |
|  4249488 |   437 | `	sInstr.p3  = p3;` |
|  4249488 |   438 | `	if( pIndex ){` |
|        - |   439 | `		/* Instruction index in the bytecode array */` |
|   230804 |   440 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115401 |   441 | `	}` |
|        - |   442 | `	/* Finally,record the instruction */` |
|  4249488 |   443 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4249488 |   444 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   445 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   446 | `		/* Fall throw */` |
|      ! 0 |   447 | `	}` |
|  4249488 |   448 | `	return rc;` |
|        2 |   449 |  |
|        - |   450 | `/*` |
|        - |   451 | ` * Swap the current bytecode container with the given one.` |
|        - |   452 | ` */` |
|   551532 |   453 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   454 |  |
|   551534 |   455 | `	if( pContainer == 0 ){` |
|        - |   456 | `		/* Point to the default container */` |
|      ! 0 |   457 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   458 | `	}else{` |
|        - |   459 | `		/* Change container */` |
|   551534 |   460 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   461 | `	}` |
|   551534 |   462 | `	return SXRET_OK;` |
|        2 |   463 |  |
|        - |   464 | `/*` |
|        - |   465 | ` * Return the current bytecode container.` |
|        - |   466 | ` */` |
|   275766 |   467 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   468 |  |
|   275768 |   469 | `	return pVm->pByteContainer;` |
|        2 |   470 |  |
|        - |   471 | `/*` |
|        - |   472 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   473 | ` */` |
|   227592 |   474 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   475 |  |
|        - |   476 | `	VmInstr *pInstr;` |
|   227594 |   477 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   227594 |   478 | `	return pInstr;` |
|        2 |   479 |  |
|        - |   480 | `/*` |
|        - |   481 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   482 | ` */` |
|  1276572 |   483 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   484 |  |
|  1276574 |   485 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Pop the last VM instruction.` |
|        - |   489 | ` */` |
|   210520 |   490 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   491 |  |
|   210522 |   492 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   493 |  |
|        - |   494 | `/*` |
|        - |   495 | ` * Peek the last VM instruction.` |
|        - |   496 | ` */` |
|   836898 |   497 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   498 |  |
|   836900 |   499 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   500 |  |
|    33416 |   501 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   502 |  |
|        - |   503 | `	VmInstr *aInstr;` |
|        - |   504 | `	sxu32 n;` |
|    33418 |   505 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33418 |   506 | `	if( n < 2 ){` |
|      ! 0 |   507 | `		return 0;` |
|        - |   508 | `	}` |
|    33418 |   509 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33418 |   510 | `	return &aInstr[n - 2];` |
|    16710 |   511 |  |
|        - |   512 | `/*` |
|        - |   513 | ` * Allocate a new virtual machine frame.` |
|        - |   514 | ` */` |
|    22348 |   515 | `static VmFrame * VmNewFrame(` |
|        - |   516 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   517 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   518 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   519 | `	)` |
|        2 |   520 |  |
|        - |   521 | `	VmFrame *pFrame;` |
|        - |   522 | `	/* Allocate a new vm frame */` |
|    22350 |   523 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22350 |   524 | `	if( pFrame == 0 ){` |
|      ! 0 |   525 | `		return 0;` |
|        - |   526 | `	}` |
|        - |   527 | `	/* Zero the structure */` |
|    22350 |   528 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   529 | `	/* Initialize frame fields */` |
|    22350 |   530 | `	pFrame->pUserData = pUserData;` |
|    22350 |   531 | `	pFrame->pThis = pThis;` |
|    22350 |   532 | `	pFrame->pVm = pVm;` |
|    22350 |   533 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22350 |   534 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22350 |   535 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22350 |   536 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22350 |   537 | `	return pFrame;` |
|    11176 |   538 |  |
|        - |   539 | `/* Forward declaration */` |
|        - |   540 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   541 | `/*` |
|        - |   542 | ` * Enter a VM frame.` |
|        - |   543 | ` */` |
|    22302 |   544 | `static sxi32 VmEnterFrame(` |
|        - |   545 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   546 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   547 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   548 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   549 | `	)` |
|        2 |   550 |  |
|        - |   551 | `	VmFrame *pFrame;` |
|        - |   552 | `	/* Allocate a new frame */` |
|    22304 |   553 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22304 |   554 | `	if( pFrame == 0 ){` |
|      ! 0 |   555 | `		return SXERR_MEM;` |
|        - |   556 | `	}` |
|        - |   557 | `	/* Link to the list of active VM frame */` |
|    22304 |   558 | `	pFrame->pParent = pVm->pFrame;` |
|    22304 |   559 | `	pVm->pFrame = pFrame;` |
|    22304 |   560 | `	if( ppFrame ){` |
|        - |   561 | `		/* Write a pointer to the new VM frame */` |
|    19170 |   562 | `		*ppFrame = pFrame;` |
|     9584 |   563 | `	}` |
|    22304 |   564 | `	return SXRET_OK;` |
|    11153 |   565 |  |
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
|    19158 |   609 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   610 |  |
|    19160 |   611 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19160 |   612 | `	if( pCurFrame ){` |
|        - |   613 | `		/* Unlink from the list of active VM frame */` |
|    19160 |   614 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19160 |   615 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   616 | `			VmSlot  *aSlot;` |
|        - |   617 | `			sxu32 n;` |
|        - |   618 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18794 |   619 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   124024 |   620 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   621 | `				/* Unset the local variable */` |
|   105232 |   622 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52617 |   623 | `			}` |
|        - |   624 | `			/* Remove local reference */` |
|    18794 |   625 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   124098 |   626 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   105306 |   627 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52654 |   628 | `			}` |
|     9396 |   629 | `		}` |
|        - |   630 | `		/* Release internal containers */` |
|    19160 |   631 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19160 |   632 | `		SySetRelease(&pCurFrame->sArg);` |
|    19160 |   633 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19160 |   634 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   635 | `		/* Release the whole structure */` |
|    19160 |   636 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9579 |   637 | `	}` |
|    19160 |   638 |  |
|        - |   639 | `/*` |
|        - |   640 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   641 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   642 | ` * should be skipped when looking for the real execution context.` |
|        - |   643 | ` */` |
|  7113432 |   644 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   645 |  |
|  7115636 |   646 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2204 |   647 | `		pFrame = pFrame->pParent;` |
|        2 |   648 | `	}` |
|  7113434 |   649 | `	return pFrame;` |
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
|   353170 |   788 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   789 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   790 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   791 | `	)` |
|        2 |   792 |  |
|        - |   793 | `	ph7_class_method *pMeth;` |
|        - |   794 | `	ph7_class_attr *pAttr;` |
|        - |   795 | `	SyHashEntry *pEntry;` |
|        - |   796 | `	sxi32 rc;` |
|        - |   797 | `	/* Reset the loop cursor */` |
|   353172 |   798 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   799 | `	/* Process only static and constant attribute */` |
|  1396747 |   800 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   801 | `		/* Extract the current attribute */` |
|   866992 |   802 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   866992 |   803 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   353172 |   848 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   849 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   850 | `		 */` |
|   191564 |   851 | `		return SXRET_OK;` |
|        - |   852 | `	}` |
|        - |   853 | `	/* Create constructor alias if not yet done */` |
|   161610 |   854 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   855 | `		/* User constructor with the same base class name */` |
|     6658 |   856 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6658 |   857 | `		if( pEntry ){` |
|      ! 0 |   858 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   859 | `			/* Create the alias */` |
|      ! 0 |   860 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   861 | `		}` |
|     3328 |   862 | `	}` |
|        - |   863 | `	/* Install the methods now */` |
|   161610 |   864 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1654830 |   865 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1412418 |   866 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1412418 |   867 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1412410 |   868 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1412410 |   869 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   870 | `				return rc;` |
|        - |   871 | `			}` |
|   706204 |   872 | `		}` |
|        2 |   873 | `	}` |
|        - |   874 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   161610 |   875 | `	pClass->bMounted = TRUE;` |
|   161610 |   876 | `	return SXRET_OK;` |
|   176587 |   877 |  |
|        - |   878 | `/*` |
|        - |   879 | ` * Allocate a private frame for attributes of the given` |
|        - |   880 | ` * class instance (Object in the PHP jargon).` |
|        - |   881 | ` */` |
|     2096 |   882 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   883 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   884 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   885 | `	)` |
|        2 |   886 |  |
|     2098 |   887 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   888 | `	ph7_class_attr *pAttr;` |
|        - |   889 | `	SyHashEntry *pEntry;` |
|        - |   890 | `	sxi32 rc;` |
|        - |   891 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2098 |   892 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8694 |   893 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   894 | `		VmClassAttr *pVmAttr;` |
|        - |   895 | `		/* Extract the current attribute */` |
|     6598 |   896 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6598 |   897 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6598 |   898 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   899 | `			return SXERR_MEM;` |
|        - |   900 | `		}` |
|     6598 |   901 | `		pVmAttr->pAttr = pAttr;` |
|     6598 |   902 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   903 | `			ph7_value *pMemObj;` |
|        - |   904 | `			/* Reserve a memory object for this attribute */` |
|     6572 |   905 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6572 |   906 | `			if( pMemObj == 0 ){` |
|      ! 0 |   907 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   908 | `				return SXERR_MEM;` |
|        - |   909 | `			}` |
|     6572 |   910 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6572 |   911 | `			pVmAttr->iState = 0;` |
|     6572 |   912 | `			pVmAttr->pOwner = pClass;` |
|     6572 |   913 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   914 | `				/* Initialize attribute default value (any complex expression) */` |
|     2258 |   915 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5444 |   916 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   917 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   918 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   919 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   920 | `			}` |
|     6572 |   921 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6572 |   922 | `			if( rc != SXRET_OK ){` |
|        - |   923 | `				VmSlot sSlot;` |
|        - |   924 | `				/* Restore memory object */` |
|      ! 0 |   925 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   926 | `				sSlot.pUserData = 0;` |
|      ! 0 |   927 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   928 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   929 | `				return SXERR_MEM;` |
|        - |   930 | `			}` |
|        - |   931 | `			/* Install attribute in the reference table */` |
|     6572 |   932 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   933 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   934 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   935 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6572 |   936 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|     3287 |   948 | `		}else{` |
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
|     2098 |   960 | `	return SXRET_OK;` |
|     1050 |   961 |  |
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
|   454970 |   973 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   974 |  |
|        - |   975 | `	ph7_value *pObj;` |
|        - |   976 | `	sxi32 rc;` |
|   454972 |   977 | `	if( pIndex ){` |
|        - |   978 | `		/* Object index in the object table */` |
|   445570 |   979 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   222784 |   980 | `	}` |
|        - |   981 | `	/* Reserve a slot for the new object */` |
|   454972 |   982 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   454972 |   983 | `	if( rc != SXRET_OK ){` |
|        - |   984 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   985 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   986 | `		 */` |
|      ! 0 |   987 | `		return 0;` |
|        - |   988 | `	}` |
|   454972 |   989 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   454972 |   990 | `	return pObj;` |
|   227487 |   991 |  |
|        - |   992 | `/*` |
|        - |   993 | ` * Reserve a memory object.` |
|        - |   994 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   995 | ` */` |
|  2151632 |   996 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   997 |  |
|        - |   998 | `	ph7_value *pObj;` |
|        - |   999 | `	sxi32 rc;` |
|  2151634 |  1000 | `	if( pIndex ){` |
|        - |  1001 | `		/* Object index in the object table */` |
|  2151634 |  1002 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075816 |  1003 | `	}` |
|        - |  1004 | `	/* Reserve a slot for the new object */` |
|  2151634 |  1005 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151634 |  1006 | `	if( rc != SXRET_OK ){` |
|        - |  1007 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1008 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1009 | `		 */` |
|      ! 0 |  1010 | `		return 0;` |
|        - |  1011 | `	}` |
|  2151634 |  1012 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151634 |  1013 | `	return pObj;` |
|  1075818 |  1014 |  |
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
|        - |  1079 | `	"interface JsonSerializable {"\` |
|        - |  1080 | `	"public function jsonSerialize();"\` |
|        - |  1081 | `	"}"\` |
|        - |  1082 | `	"interface UnitEnum {"\` |
|        - |  1083 | `	"public static function cases();"\` |
|        - |  1084 | `	"}"\` |
|        - |  1085 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1086 | `	"public static function from($value);"\` |
|        - |  1087 | `	"public static function tryFrom($value);"\` |
|        - |  1088 | `	"}"\` |
|        - |  1089 | `	"class Exception implements Throwable { "\` |
|        - |  1090 | `    "protected $message = '';"\` |
|        - |  1091 | `    "protected $code = 0;"\` |
|        - |  1092 | `    "protected $file;"\` |
|        - |  1093 | `    "protected $line;"\` |
|        - |  1094 | `    "protected $trace;"\` |
|        - |  1095 | `    "protected $previous;"\` |
|        - |  1096 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1097 | `	"   if( isset($message) ){"\` |
|        - |  1098 | `	"	  $this->message = $message;"\` |
|        - |  1099 | `	"   }"\` |
|        - |  1100 | `	"   $this->code = $code;"\` |
|        - |  1101 | `	"   $this->file = __FILE__;"\` |
|        - |  1102 | `	"   $this->line = __LINE__;"\` |
|        - |  1103 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1104 | `	"   if( isset($previous) ){"\` |
|        - |  1105 | `	"     $this->previous = $previous;"\` |
|        - |  1106 | `	"   }"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	"public function getMessage(){"\` |
|        - |  1109 | `	"   return $this->message;"\` |
|        - |  1110 | `	"}"\` |
|        - |  1111 | `	" public function getCode(){"\` |
|        - |  1112 | `	"  return $this->code;"\` |
|        - |  1113 | `	"}"\` |
|        - |  1114 | `	"public function getFile(){"\` |
|        - |  1115 | `	"  return $this->file;"\` |
|        - |  1116 | `	"}"\` |
|        - |  1117 | `	"public function getLine(){"\` |
|        - |  1118 | `	"  return $this->line;"\` |
|        - |  1119 | `	"}"\` |
|        - |  1120 | `	"public function getTrace(){"\` |
|        - |  1121 | `	"   return $this->trace;"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"public function getTraceAsString(){"\` |
|        - |  1124 | `	"  return debug_string_backtrace();"\` |
|        - |  1125 | `	"}"\` |
|        - |  1126 | `	"public function getPrevious(){"\` |
|        - |  1127 | `	"    return $this->previous;"\` |
|        - |  1128 | `	"}"\` |
|        - |  1129 | `	"public function __toString(){"\` |
|        - |  1130 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1131 | `    "}"\` |
|        - |  1132 | `	"}"\` |
|        - |  1133 | `	"class Error implements Throwable { "\` |
|        - |  1134 | `    "protected $message = '';"\` |
|        - |  1135 | `    "protected $code = 0;"\` |
|        - |  1136 | `    "protected $file;"\` |
|        - |  1137 | `    "protected $line;"\` |
|        - |  1138 | `    "protected $trace;"\` |
|        - |  1139 | `    "protected $previous;"\` |
|        - |  1140 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1141 | `	"   if( isset($message) ){"\` |
|        - |  1142 | `	"	  $this->message = $message;"\` |
|        - |  1143 | `	"   }"\` |
|        - |  1144 | `	"   $this->code = $code;"\` |
|        - |  1145 | `	"   $this->file = __FILE__;"\` |
|        - |  1146 | `	"   $this->line = __LINE__;"\` |
|        - |  1147 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1148 | `	"   if( isset($previous) ){"\` |
|        - |  1149 | `	"     $this->previous = $previous;"\` |
|        - |  1150 | `	"   }"\` |
|        - |  1151 | `	"}"\` |
|        - |  1152 | `	"public function getMessage(){"\` |
|        - |  1153 | `	"   return $this->message;"\` |
|        - |  1154 | `	"}"\` |
|        - |  1155 | `	"public function getCode(){"\` |
|        - |  1156 | `	"  return $this->code;"\` |
|        - |  1157 | `	"}"\` |
|        - |  1158 | `	"public function getFile(){"\` |
|        - |  1159 | `	"  return $this->file;"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"public function getLine(){"\` |
|        - |  1162 | `	"  return $this->line;"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"public function getTrace(){"\` |
|        - |  1165 | `	"   return $this->trace;"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"public function getTraceAsString(){"\` |
|        - |  1168 | `	"  return debug_string_backtrace();"\` |
|        - |  1169 | `	"}"\` |
|        - |  1170 | `	"public function getPrevious(){"\` |
|        - |  1171 | `	"    return $this->previous;"\` |
|        - |  1172 | `	"}"\` |
|        - |  1173 | `	"public function __toString(){"\` |
|        - |  1174 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1175 | `	"}"\` |
|        - |  1176 | `	"}"\` |
|        - |  1177 | `	"class TypeError extends Error { }"\` |
|        - |  1178 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1179 | `	"class ValueError extends Error { }"\` |
|        - |  1180 | `	"class FiberError extends Error { }"\` |
|        - |  1181 | `	"class AssertionError extends Error { }"\` |
|        - |  1182 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1183 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1184 | `	"class ErrorException extends Exception { "\` |
|        - |  1185 | `	"protected $severity;"\` |
|        - |  1186 | `	"public function __construct(string $message = null,"\` |
|        - |  1187 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1188 | `	"   if( isset($message) ){"\` |
|        - |  1189 | `	"	  $this->message = $message;"\` |
|        - |  1190 | `	"   }"\` |
|        - |  1191 | `	"   $this->severity = $severity;"\` |
|        - |  1192 | `	"   $this->code = $code;"\` |
|        - |  1193 | `	"   $this->file = $filename;"\` |
|        - |  1194 | `	"   $this->line = $lineno;"\` |
|        - |  1195 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1196 | `	"   if( isset($previous) ){"\` |
|        - |  1197 | `	"     $this->previous = $previous;"\` |
|        - |  1198 | `	"   }"\` |
|        - |  1199 | `	"}"\` |
|        - |  1200 | `	"public function getSeverity(){"\` |
|        - |  1201 | `	"   return $this->severity;"\` |
|        - |  1202 | `    "}"\` |
|        - |  1203 | `	"}"\` |
|        - |  1204 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1205 | `	"class LogicException extends Exception { }"\` |
|        - |  1206 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1207 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1208 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1209 | `	"class DomainException extends LogicException { }"\` |
|        - |  1210 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1211 | `	"class LengthException extends LogicException { }"\` |
|        - |  1212 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1213 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1214 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1215 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1216 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1217 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1218 | `	"interface Iterator extends Traversable {"\` |
|        - |  1219 | `	"public function current();"\` |
|        - |  1220 | `	"public function key();"\` |
|        - |  1221 | `	"public function next();"\` |
|        - |  1222 | `	"public function rewind();"\` |
|        - |  1223 | `	"public function valid();"\` |
|        - |  1224 | `	"}"\` |
|        - |  1225 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1226 | `	"public function getIterator();"\` |
|        - |  1227 | `	"}"\` |
|        - |  1228 | `	"interface Serializable {"\` |
|        - |  1229 | `	"public function serialize();"\` |
|        - |  1230 | `	"public function unserialize(string $serialized);"\` |
|        - |  1231 | `	"}"\` |
|        - |  1232 | `	"/* Directory releated IO */"\` |
|        - |  1233 | `	"class Directory {"\` |
|        - |  1234 | `	"public $handle = null;"\` |
|        - |  1235 | `	"public $path  = null;"\` |
|        - |  1236 | `	"public function __construct(string $path)"\` |
|        - |  1237 | `	"{"\` |
|        - |  1238 | `	"   $this->handle = opendir($path);"\` |
|        - |  1239 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1240 | `	"      $this->path = $path;"\` |
|        - |  1241 | `	"   }"\` |
|        - |  1242 | `	"}"\` |
|        - |  1243 | `	"public function __destruct()"\` |
|        - |  1244 | `	"{"\` |
|        - |  1245 | `	"  if( $this->handle != null ){"\` |
|        - |  1246 | `	"       closedir($this->handle);"\` |
|        - |  1247 | `	"  }"\` |
|        - |  1248 | `	"}"\` |
|        - |  1249 | `	"public function read()"\` |
|        - |  1250 | `	"{"\` |
|        - |  1251 | `	"    return readdir($this->handle);"\` |
|        - |  1252 | `	"}"\` |
|        - |  1253 | `	"public function rewind()"\` |
|        - |  1254 | `	"{"\` |
|        - |  1255 | `	"    rewinddir($this->handle);"\` |
|        - |  1256 | `	"}"\` |
|        - |  1257 | `	"public function close()"\` |
|        - |  1258 | `	"{"\` |
|        - |  1259 | `	"    closedir($this->handle);"\` |
|        - |  1260 | `	"    $this->handle = null;"\` |
|        - |  1261 | `	"}"\` |
|        - |  1262 | `	"}"\` |
|        - |  1263 | `	"class Fiber {"\` |
|        - |  1264 | `	"  private $__ctx;"\` |
|        - |  1265 | `	"  private $__callable;"\` |
|        - |  1266 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1267 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1268 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1269 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1270 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1271 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1272 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1273 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1274 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1275 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1276 | `	"}"\` |
|        - |  1277 | `	"class Generator implements Iterator {"\` |
|        - |  1278 | `	"  private $__ctx;"\` |
|        - |  1279 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1280 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1281 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1282 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1283 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1284 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1285 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1286 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1287 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1288 | `	"}"\` |
|        - |  1289 | `	"class stdClass{"\` |
|        - |  1290 | `	"  public $value;"\` |
|        - |  1291 | `	" /* Magic methods */"\` |
|        - |  1292 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1293 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1294 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1295 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1296 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1297 | `	"}"\` |
|        - |  1298 | `	"function dir(string $path){"\` |
|        - |  1299 | `	"   return new Directory($path);"\` |
|        - |  1300 | `	"}"\` |
|        - |  1301 | `	"function Dir(string $path){"\` |
|        - |  1302 | `	"   return new Directory($path);"\` |
|        - |  1303 | `	"}"\` |
|        - |  1304 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1305 | `    "{"\` |
|        - |  1306 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1307 | `	"  $aDir = array();"\` |
|        - |  1308 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1309 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1310 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1311 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1312 | `	"   }"\` |
|        - |  1313 | `	"  closedir($pHandle);"\` |
|        - |  1314 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1315 | `	"      rsort($aDir);"\` |
|        - |  1316 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1317 | `	"      sort($aDir);"\` |
|        - |  1318 | `	"  }"\` |
|        - |  1319 | `	"  return $aDir;"\` |
|        - |  1320 | `	"}"\` |
|        - |  1321 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1322 | `	"/* Open the target directory */"\` |
|        - |  1323 | `	"$zDir = dirname($pattern);"\` |
|        - |  1324 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1325 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1326 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1327 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1328 | `	"	return FALSE;"\` |
|        - |  1329 | `	"}"\` |
|        - |  1330 | `	"$pattern = basename($pattern);"\` |
|        - |  1331 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1332 | `	"/* Loop throw available entries */"\` |
|        - |  1333 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1334 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1335 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1336 | `	"	if( $rc ){"\` |
|        - |  1337 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1338 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1339 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1340 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1341 | `	"		  }"\` |
|        - |  1342 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1343 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1344 | `	"		 continue;"\` |
|        - |  1345 | `	"	   }"\` |
|        - |  1346 | `	"	   /* Add the entry */"\` |
|        - |  1347 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1348 | `	"	}"\` |
|        - |  1349 | `	" }"\` |
|        - |  1350 | `	"/* Close the handle */"\` |
|        - |  1351 | `	"closedir($pHandle);"\` |
|        - |  1352 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1353 | `	"  /* Sort the array */"\` |
|        - |  1354 | `	"  sort($pArray);"\` |
|        - |  1355 | `	"}"\` |
|        - |  1356 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1357 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1358 | `	"  $pArray[] = $pattern;"\` |
|        - |  1359 | `	"}"\` |
|        - |  1360 | `	"/* Return the created array */"\` |
|        - |  1361 | `	"return $pArray;"\` |
|        - |  1362 | `   "}"\` |
|        - |  1363 | `   "/* Creates a temporary file */"\` |
|        - |  1364 | `   "function tmpfile(){"\` |
|        - |  1365 | `   "  /* Extract the temp directory */"\` |
|        - |  1366 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1367 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1368 | `   "    /* Use the current dir */"\` |
|        - |  1369 | `   "    $zTempDir = '.';"\` |
|        - |  1370 | `   "  }"\` |
|        - |  1371 | `   "  /* Create the file */"\` |
|        - |  1372 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1373 | `   "  return $pHandle;"\` |
|        - |  1374 | `   "}"\` |
|        - |  1375 | `   "/* Creates a temporary filename */"\` |
|        - |  1376 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1377 | `   "{"\` |
|        - |  1378 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1379 | `   "}"\` |
|        - |  1380 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1381 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1382 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1383 | `   "/* Copy arguments */"\` |
|        - |  1384 | `   "$nArgs = func_num_args();"\` |
|        - |  1385 | `   "$pNew = array();"\` |
|        - |  1386 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1387 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1388 | `    "}"\` |
|        - |  1389 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1390 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1391 | `	"/* Erase */"\` |
|        - |  1392 | `	"array_erase($pArray);"\` |
|        - |  1393 | `	"/* Unshift */"\` |
|        - |  1394 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1395 | `	"return sizeof($pArray);"\` |
|        - |  1396 | `    "}"\` |
|        - |  1397 | `	"function array_merge_recursive(){"\` |
|        - |  1398 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1399 | `    "$arrays = func_get_args();"\` |
|        - |  1400 | `    "$narrays = count($arrays);"\` |
|        - |  1401 | `    "$ret = array();"\` |
|        - |  1402 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1403 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1404 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1405 | `	 " }"\` |
|        - |  1406 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1407 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1408 | `     "  if( $keyIsInt ) {"\` |
|        - |  1409 | `     "   $ret[] = $value;"\` |
|        - |  1410 | `     "  } else {"\` |
|        - |  1411 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1412 | `     "    $cur = $ret[$key];"\` |
|        - |  1413 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1414 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1415 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1416 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1417 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1418 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1419 | `     "    } else {"\` |
|        - |  1420 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1421 | `     "    }"\` |
|        - |  1422 | `     "   } else {"\` |
|        - |  1423 | `     "    $ret[$key] = $value;"\` |
|        - |  1424 | `     "   }"\` |
|        - |  1425 | `     "  }"\` |
|        - |  1426 | `     " }"\` |
|        - |  1427 | `	 " }"\` |
|        - |  1428 | `	 " return $ret;"\` |
|        - |  1429 | `    "}"\` |
|        - |  1430 | `	"function max(){"\` |
|        - |  1431 | `    "  $pArgs = func_get_args();"\` |
|        - |  1432 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1433 | `	"  return null;"\` |
|        - |  1434 | `    " }"\` |
|        - |  1435 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1436 | `    " $pArg = $pArgs[0];"\` |
|        - |  1437 | `	" if( !is_array($pArg) ){"\` |
|        - |  1438 | `	"   return $pArg; "\` |
|        - |  1439 | `	" }"\` |
|        - |  1440 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1441 | `	"   return null;"\` |
|        - |  1442 | `	" }"\` |
|        - |  1443 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1444 | `	" reset($pArg);"\` |
|        - |  1445 | `	" $max = current($pArg);"\` |
|        - |  1446 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1447 | `	"   if( $val > $max ){"\` |
|        - |  1448 | `	"     $max = $val;"\` |
|        - |  1449 | `    " }"\` |
|        - |  1450 | `	" }"\` |
|        - |  1451 | `	" return $max;"\` |
|        - |  1452 | `    " }"\` |
|        - |  1453 | `    " $max = $pArgs[0];"\` |
|        - |  1454 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1455 | `    " $val = $pArgs[$i];"\` |
|        - |  1456 | `	"if( $val > $max ){"\` |
|        - |  1457 | `	" $max = $val;"\` |
|        - |  1458 | `	"}"\` |
|        - |  1459 | `    " }"\` |
|        - |  1460 | `	" return $max;"\` |
|        - |  1461 | `    "}"\` |
|        - |  1462 | `	"function min(){"\` |
|        - |  1463 | `    "  $pArgs = func_get_args();"\` |
|        - |  1464 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1465 | `	"  return null;"\` |
|        - |  1466 | `    " }"\` |
|        - |  1467 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1468 | `    " $pArg = $pArgs[0];"\` |
|        - |  1469 | `	" if( !is_array($pArg) ){"\` |
|        - |  1470 | `	"   return $pArg; "\` |
|        - |  1471 | `	" }"\` |
|        - |  1472 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1473 | `	"   return null;"\` |
|        - |  1474 | `	" }"\` |
|        - |  1475 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1476 | `	" reset($pArg);"\` |
|        - |  1477 | `	" $min = current($pArg);"\` |
|        - |  1478 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1479 | `	"   if( $val < $min ){"\` |
|        - |  1480 | `	"     $min = $val;"\` |
|        - |  1481 | `    " }"\` |
|        - |  1482 | `	" }"\` |
|        - |  1483 | `	" return $min;"\` |
|        - |  1484 | `    " }"\` |
|        - |  1485 | `    " $min = $pArgs[0];"\` |
|        - |  1486 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1487 | `    " $val = $pArgs[$i];"\` |
|        - |  1488 | `	"if( $val < $min ){"\` |
|        - |  1489 | `	" $min = $val;"\` |
|        - |  1490 | `	" }"\` |
|        - |  1491 | `    " }"\` |
|        - |  1492 | `	" return $min;"\` |
|        - |  1493 | `	"}"\` |
|        - |  1494 | `	"function fileowner(string $file){"\` |
|        - |  1495 | `    " $a = stat($file);"\` |
|        - |  1496 | `	" if( !is_array($a) ){"\` |
|        - |  1497 | `	"	return false;"\` |
|        - |  1498 | `	" }"\` |
|        - |  1499 | `	" return $a['uid'];"\` |
|        - |  1500 | `    "}"\` |
|        - |  1501 | `    "function filegroup(string $file){"\` |
|        - |  1502 | `	" $a = stat($file);"\` |
|        - |  1503 | `	" if( !is_array($a) ){"\` |
|        - |  1504 | `	"	return false;"\` |
|        - |  1505 | `	" }"\` |
|        - |  1506 | `	" return $a['gid'];"\` |
|        - |  1507 | `    "}"\` |
|        - |  1508 | `	 "function fileinode(string $file){"\` |
|        - |  1509 | `	" $a = stat($file);"\` |
|        - |  1510 | `	" if( !is_array($a) ){"\` |
|        - |  1511 | `	"	return false;"\` |
|        - |  1512 | `	" }"\` |
|        - |  1513 | `	" return $a['ino'];"\` |
|        - |  1514 | `    "}"` |
|        - |  1515 |  |
|        - |  1516 | `/*` |
|        - |  1517 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1518 | ` * start compiling the target PHP program.` |
|        - |  1519 | ` */` |
|     3134 |  1520 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1521 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1522 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1523 | `	 )` |
|        2 |  1524 |  |
|        - |  1525 | `	SyString sBuiltin;` |
|        - |  1526 | `	ph7_value *pObj;` |
|        - |  1527 | `	sxi32 rc;` |
|        - |  1528 | `	/* Zero the structure */` |
|     3136 |  1529 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1530 | `	/* Initialize VM fields */` |
|     3136 |  1531 | `	pVm->pEngine = &(*pEngine);` |
|     3136 |  1532 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1533 | `	/* Instructions containers */` |
|     3136 |  1534 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3136 |  1535 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3136 |  1536 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1537 | `	/* Object containers */` |
|     3136 |  1538 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3136 |  1539 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1540 | `	/* Virtual machine internal containers */` |
|     3136 |  1541 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3136 |  1542 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3136 |  1543 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3136 |  1544 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3136 |  1545 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3136 |  1546 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3136 |  1547 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3136 |  1548 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3136 |  1549 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3136 |  1550 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3136 |  1551 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3136 |  1552 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3136 |  1553 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3136 |  1554 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3136 |  1555 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3136 |  1556 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3136 |  1557 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3136 |  1558 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3136 |  1559 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3136 |  1560 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3136 |  1561 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3136 |  1562 | `	pVm->pPendingException = 0;` |
|        - |  1563 | `	/* Configuration containers */` |
|     3136 |  1564 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1565 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1566 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1567 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3136 |  1568 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3136 |  1569 | `	pVm->iResponseStatus = 200;` |
|     3136 |  1570 | `	pVm->bHeadersSent = 0;` |
|     3136 |  1571 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1572 | `	/* Error callbacks containers */` |
|     3136 |  1573 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3136 |  1574 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3136 |  1575 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3136 |  1576 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3136 |  1577 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1578 | `	/* Set a default recursion limit */` |
|        - |  1579 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3136 |  1580 | `	pVm->nMaxDepth = 32;` |
|        - |  1581 | `#else` |
|        - |  1582 | `	pVm->nMaxDepth = 16;` |
|        - |  1583 | `#endif` |
|        - |  1584 | `	/* Default assertion flags */` |
|     3136 |  1585 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1586 | `	/* JSON return status */` |
|     3136 |  1587 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1588 | `	/* PRNG context */` |
|     3136 |  1589 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1590 | `	/* Install the null constant */` |
|     3136 |  1591 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1592 | `	if( pObj == 0 ){` |
|      ! 0 |  1593 | `		rc = SXERR_MEM;` |
|      ! 0 |  1594 | `		goto Err;` |
|        - |  1595 | `	}` |
|     3136 |  1596 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1597 | `	/* Install the boolean TRUE constant */` |
|     3136 |  1598 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1599 | `	if( pObj == 0 ){` |
|      ! 0 |  1600 | `		rc = SXERR_MEM;` |
|      ! 0 |  1601 | `		goto Err;` |
|        - |  1602 | `	}` |
|     3136 |  1603 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1604 | `	/* Install the boolean FALSE constant */` |
|     3136 |  1605 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1606 | `	if( pObj == 0 ){` |
|      ! 0 |  1607 | `		rc = SXERR_MEM;` |
|      ! 0 |  1608 | `		goto Err;` |
|        - |  1609 | `	}` |
|     3136 |  1610 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1611 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1612 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1613 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3136 |  1614 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3136 |  1615 | `	if( pObj == 0 ){` |
|      ! 0 |  1616 | `		rc = SXERR_MEM;` |
|      ! 0 |  1617 | `		goto Err;` |
|        - |  1618 | `	}` |
|     3136 |  1619 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1620 | `	/* Create the global frame */` |
|     3136 |  1621 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3136 |  1622 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1623 | `		goto Err;` |
|        - |  1624 | `	}` |
|        - |  1625 | `	/* Initialize the code generator */` |
|     3136 |  1626 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3136 |  1627 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1628 | `		goto Err;` |
|        - |  1629 | `	}` |
|        - |  1630 | `	/* VM correctly initialized,set the magic number */` |
|     3136 |  1631 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3136 |  1632 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1633 | `	/* Compile the built-in library */` |
|     3136 |  1634 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1635 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3136 |  1636 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1637 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3136 |  1638 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3136 |  1639 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3136 |  1640 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3136 |  1641 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|        - |  1642 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3136 |  1643 | `	pVm->pCoalesceObj = 0;` |
|     3136 |  1644 | `	pVm->bCoalesceArmed = 0;` |
|     3136 |  1645 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1646 | `	/* Register Fiber internal C functions */` |
|     3136 |  1647 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3136 |  1648 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3136 |  1649 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3136 |  1650 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3136 |  1651 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3136 |  1652 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3136 |  1653 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3136 |  1654 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3136 |  1655 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3136 |  1656 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1657 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3136 |  1658 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3136 |  1659 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3136 |  1660 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3136 |  1661 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3136 |  1662 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3136 |  1663 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3136 |  1664 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3136 |  1665 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3136 |  1666 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3136 |  1667 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1668 | `	/* Reset the code generator */` |
|     3136 |  1669 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3136 |  1670 | `	return SXRET_OK;` |
|      ! 0 |  1671 | `Err:` |
|      ! 0 |  1672 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1673 | `	return rc;` |
|     1569 |  1674 |  |
|        - |  1675 | `/*` |
|        - |  1676 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1677 | ` * routine which store the output in an internal blob.` |
|        - |  1678 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1679 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1680 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1681 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1682 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1683 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1684 | ` * to finish executing and extracting the output.` |
|        - |  1685 | ` */` |
|       38 |  1686 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1687 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1688 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1689 | `	void *pUserData     /* User private data */` |
|        - |  1690 | `	)` |
|      ! 0 |  1691 |  |
|        - |  1692 | `	 sxi32 rc;` |
|        - |  1693 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1694 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1695 | `	 return rc;` |
|      ! 0 |  1696 |  |
|        - |  1697 | `/*` |
|        - |  1698 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1699 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1700 | ` */` |
|    20474 |  1701 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1702 |  |
|    20476 |  1703 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20476 |  1704 | `	if( xCons != VmObConsumer ){` |
|     8206 |  1705 | `		pVm->nOutputLen += nLen;` |
|     8206 |  1706 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1022 |  1707 | `			pVm->bHeadersSent = 1;` |
|      510 |  1708 | `		}` |
|     4102 |  1709 | `	}` |
|    20476 |  1710 |  |
|        - |  1711 | `#define VM_STACK_GUARD 16` |
|        - |  1712 | `/*` |
|        - |  1713 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1714 | ` * our compiled PHP program.` |
|        - |  1715 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1716 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1717 | ` */` |
|    44930 |  1718 | `static ph7_value * VmNewOperandStack(` |
|        - |  1719 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1720 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1721 | `	)` |
|        2 |  1722 |  |
|        - |  1723 | `	ph7_value *pStack;` |
|        - |  1724 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1725 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1726 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1727 | `  ** on the maximum stack depth required.` |
|        - |  1728 | `  **` |
|        - |  1729 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1730 | `  */` |
|    44932 |  1731 | `	nInstr += VM_STACK_GUARD;` |
|    44932 |  1732 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    44932 |  1733 | `	if( pStack == 0 ){` |
|      ! 0 |  1734 | `		return 0;` |
|        - |  1735 | `	}` |
|        - |  1736 | `	/* Initialize the operand stack */` |
|  3030710 |  1737 | `	while( nInstr > 0 ){` |
|  2985780 |  1738 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2985780 |  1739 | `		--nInstr;` |
|        2 |  1740 | `	}` |
|        - |  1741 | `	/* Ready for bytecode execution */` |
|    44932 |  1742 | `	return pStack;` |
|    22467 |  1743 |  |
|        - |  1744 | `/* Forward declaration */` |
|        - |  1745 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1746 | `/*` |
|        - |  1747 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1748 | ` * This routine gets called by the PH7 engine after` |
|        - |  1749 | ` * successful compilation of the target PHP program.` |
|        - |  1750 | ` */` |
|     2820 |  1751 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1752 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|        - |  1755 | `	SyHashEntry *pEntry;` |
|        - |  1756 | `	sxi32 rc;` |
|     2822 |  1757 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1758 | `		/* Initialize your VM first */` |
|      ! 0 |  1759 | `		return SXERR_CORRUPT;` |
|        - |  1760 | `	}` |
|        - |  1761 | `	/* Mark the VM ready for byte-code execution */` |
|     2822 |  1762 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1763 | `	/* Release the code generator now we have compiled our program */` |
|     2822 |  1764 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1765 | `	/* Emit the DONE instruction */` |
|     2822 |  1766 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2822 |  1767 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1768 | `		return SXERR_MEM;` |
|        - |  1769 | `	}` |
|        - |  1770 | `	/* Script return value */` |
|     2822 |  1771 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1772 | `	/* Allocate a new operand stack */` |
|     2822 |  1773 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2822 |  1774 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1775 | `		return SXERR_MEM;` |
|        - |  1776 | `	}` |
|        - |  1777 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1778 | `	 * private data. */` |
|     2822 |  1779 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2822 |  1780 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1781 | `	/* Allocate the reference table */` |
|     2822 |  1782 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2822 |  1783 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2822 |  1784 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1785 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1786 | `		return SXERR_MEM;` |
|        - |  1787 | `	}` |
|        - |  1788 | `	/* Zero the reference table */` |
|     2822 |  1789 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1790 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2822 |  1791 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2822 |  1792 | `	if( rc != SXRET_OK ){` |
|        - |  1793 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1794 | `		return rc;` |
|        - |  1795 | `	}` |
|        - |  1796 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2822 |  1797 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2822 |  1798 | `	if( rc != SXRET_OK ){` |
|        - |  1799 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1800 | `		return rc;` |
|        - |  1801 | `	}` |
|        - |  1802 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2822 |  1803 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1804 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2822 |  1805 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1806 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2822 |  1807 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1808 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1809 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2822 |  1810 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2822 |  1811 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1812 | `#endif` |
|        - |  1813 | `	/* Initialize and install static and constants class attributes */` |
|     2822 |  1814 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   110326 |  1815 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   107506 |  1816 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   107506 |  1817 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1818 | `			return rc;` |
|        - |  1819 | `		}` |
|        2 |  1820 | `	}` |
|        - |  1821 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2822 |  1822 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1823 | `	/* VM is ready for bytecode execution */` |
|     2822 |  1824 | `	return SXRET_OK;` |
|     1412 |  1825 |  |
|        - |  1826 | `/*` |
|        - |  1827 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1828 | ` */` |
|      ! 0 |  1829 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1830 |  |
|      ! 0 |  1831 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1832 | `		return SXERR_CORRUPT;` |
|        - |  1833 | `	}` |
|        - |  1834 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1835 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1836 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1837 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1838 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1839 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1840 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1841 | `	pVm->bHttpContext = 0;` |
|        - |  1842 | `	/* Set the ready flag */` |
|      ! 0 |  1843 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1844 | `	return SXRET_OK;` |
|      ! 0 |  1845 |  |
|        - |  1846 | `/*` |
|        - |  1847 | ` * Release a Virtual Machine.` |
|        - |  1848 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1849 | ` */` |
|     2820 |  1850 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1851 |  |
|        - |  1852 | `	/* Set the stale magic number */` |
|     2822 |  1853 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1854 | `	/* Release the private memory subsystem */` |
|     2822 |  1855 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2822 |  1856 | `	return SXRET_OK;` |
|        2 |  1857 |  |
|        - |  1858 | `/*` |
|        - |  1859 | ` * Initialize a foreign function call context.` |
|        - |  1860 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1861 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1862 | ` * functions.` |
|        - |  1863 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1864 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1865 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1866 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1867 | ` */` |
|   696232 |  1868 | `static sxi32 VmInitCallContext(` |
|        - |  1869 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1870 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1871 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1872 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1873 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1874 | `	)` |
|        2 |  1875 |  |
|   696234 |  1876 | `	pOut->pFunc = pFunc;` |
|   696234 |  1877 | `	pOut->pVm   = pVm;` |
|   696234 |  1878 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   696234 |  1879 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1880 | `	/* Assume a null return value */` |
|   696234 |  1881 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   696234 |  1882 | `	pOut->pRet = pRet;` |
|   696234 |  1883 | `	pOut->iFlags = iFlags;` |
|   696234 |  1884 | `	return SXRET_OK;` |
|        2 |  1885 |  |
|        - |  1886 | `/*` |
|        - |  1887 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1888 | ` * left behind.` |
|        - |  1889 | ` */` |
|   696232 |  1890 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1891 |  |
|        - |  1892 | `	sxu32 n;` |
|   696234 |  1893 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8626 |  1894 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25196 |  1895 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16572 |  1896 | `			if( apObj[n] == 0 ){` |
|        - |  1897 | `				/* Already released */` |
|      384 |  1898 | `				continue;` |
|        - |  1899 | `			}` |
|    16190 |  1900 | `			PH7_MemObjRelease(apObj[n]);` |
|    16190 |  1901 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8096 |  1902 | `		}` |
|     8626 |  1903 | `		SySetRelease(&pCtx->sVar);` |
|     4312 |  1904 | `	}` |
|   696234 |  1905 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1906 | `		ph7_aux_data *aAux;` |
|        - |  1907 | `		void *pChunk;` |
|        - |  1908 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1909 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1910 | `		 */` |
|        9 |  1911 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1912 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1913 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1914 | `			/* Release the chunk */` |
|       25 |  1915 | `			if( pChunk ){` |
|       25 |  1916 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1917 | `			}` |
|       13 |  1918 | `		}` |
|        9 |  1919 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1920 | `	}` |
|   696234 |  1921 |  |
|        - |  1922 | `/*` |
|        - |  1923 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1924 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1925 | ` */` |
|      382 |  1926 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1927 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1928 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1929 | `	)` |
|        2 |  1930 |  |
|      384 |  1931 | `	if( pValue == 0 ){` |
|        - |  1932 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1933 | `		return;` |
|        - |  1934 | `	}` |
|      384 |  1935 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      384 |  1936 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1937 | `		sxu32 n;` |
|     1282 |  1938 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1282 |  1939 | `			if( apObj[n] == pValue ){` |
|      384 |  1940 | `				PH7_MemObjRelease(pValue);` |
|      384 |  1941 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1942 | `				/* Mark as released */` |
|      384 |  1943 | `				apObj[n] = 0;` |
|      384 |  1944 | `				break;` |
|        - |  1945 | `			}` |
|      451 |  1946 | `		}` |
|      191 |  1947 | `	}` |
|      193 |  1948 |  |
|        - |  1949 | `/*` |
|        - |  1950 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1951 | ` */` |
|  3946662 |  1952 | `static void VmPopOperand(` |
|        - |  1953 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1954 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1955 | `	)` |
|        2 |  1956 |  |
|  3946664 |  1957 | `	ph7_value *pTos = *ppTos;` |
|  8408528 |  1958 | `	while( nPop > 0 ){` |
|  4461866 |  1959 | `		PH7_MemObjRelease(pTos);` |
|  4461866 |  1960 | `		pTos--;` |
|  4461866 |  1961 | `		nPop--;` |
|        2 |  1962 | `	}` |
|        - |  1963 | `	/* Top of the stack */` |
|  3946664 |  1964 | `	*ppTos = pTos;` |
|  3946664 |  1965 |  |
|        - |  1966 | `/*` |
|        - |  1967 | ` * Reserve a memory object.` |
|        - |  1968 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1969 | ` */` |
|  3204834 |  1970 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1971 |  |
|  3204836 |  1972 | `	ph7_value *pObj = 0;` |
|        - |  1973 | `	VmSlot *pSlot;` |
|        - |  1974 | `	sxu32 nIdx;` |
|        - |  1975 | `	/* Check for a free slot */` |
|  3204836 |  1976 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3204836 |  1977 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3204836 |  1978 | `	if( pSlot ){` |
|  1053204 |  1979 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1053204 |  1980 | `		nIdx = pSlot->nIdx;` |
|   526601 |  1981 | `	}` |
|  3204836 |  1982 | `	if( pObj == 0 ){` |
|        - |  1983 | `		/* Reserve a new memory object */` |
|  2151634 |  1984 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151634 |  1985 | `		if( pObj == 0 ){` |
|      ! 0 |  1986 | `			return 0;` |
|        - |  1987 | `		}` |
|  1075816 |  1988 | `	}` |
|        - |  1989 | `	/* Set a null default value */` |
|  3204836 |  1990 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3204836 |  1991 | `	pObj->nIdx = nIdx;` |
|  3204836 |  1992 | `	return pObj;` |
|  1602419 |  1993 |  |
|        - |  1994 | `/*` |
|        - |  1995 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1996 | ` */` |
|    35192 |  1997 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1998 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1999 | `	const char *zKey,  /* Entry key */` |
|        - |  2000 | `	sxu32 nByte,       /* Key length */` |
|        - |  2001 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2002 | `	)` |
|        2 |  2003 |  |
|        - |  2004 | `	ph7_value sKey;` |
|        - |  2005 | `	sxi32 rc;` |
|    35194 |  2006 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35194 |  2007 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2008 | `	/* Perform the insertion */` |
|    35194 |  2009 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35194 |  2010 | `	PH7_MemObjRelease(&sKey);` |
|    35194 |  2011 | `	return rc;` |
|        2 |  2012 |  |
|        - |  2013 | `/*` |
|        - |  2014 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2015 | ` * Return a pointer to the variable value on success.` |
|        - |  2016 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2017 | ` */` |
|  3667834 |  2018 | `static ph7_value * VmExtractMemObj(` |
|        - |  2019 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2020 | `	const SyString *pName, /* Variable name */` |
|        - |  2021 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2022 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2023 | `	)` |
|        2 |  2024 |  |
|  3667836 |  2025 | `	int bNullify = FALSE;` |
|        - |  2026 | `	SyHashEntry *pEntry;` |
|        - |  2027 | `	VmFrame *pFrame;` |
|        - |  2028 | `	ph7_value *pObj;` |
|        - |  2029 | `	sxu32 nIdx;` |
|        - |  2030 | `	sxi32 rc;` |
|        - |  2031 | `	/* Point to the top active frame */` |
|  3667836 |  2032 | `	pFrame = pVm->pFrame;` |
|  3667836 |  2033 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2034 | `	/* Perform the lookup */` |
|  3667836 |  2035 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2036 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2037 | `		pName = &sAnnon;` |
|        - |  2038 | `		/* Always nullify the object */` |
|      ! 0 |  2039 | `		bNullify = TRUE;` |
|      ! 0 |  2040 | `		bDup = FALSE;` |
|      ! 0 |  2041 | `	}` |
|        - |  2042 | `	/* Check the superglobals table first */` |
|  3667836 |  2043 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3667836 |  2044 | `	if( pEntry == 0 ){` |
|        - |  2045 | `		/* Query the top active frame */` |
|  3667796 |  2046 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3667796 |  2047 | `		if( pEntry == 0 ){` |
|   113200 |  2048 | `			char *zName = (char *)pName->zString;` |
|        - |  2049 | `			VmSlot sLocal;` |
|   113200 |  2050 | `			if( !bCreate ){` |
|        - |  2051 | `				/* Do not create the variable,return NULL instead */` |
|      958 |  2052 | `				return 0;` |
|        - |  2053 | `			}` |
|        - |  2054 | `			/* No such variable,automatically create a new one and install` |
|        - |  2055 | `			 * it in the current frame.` |
|        - |  2056 | `			 */` |
|   112244 |  2057 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   112244 |  2058 | `			if( pObj == 0 ){` |
|      ! 0 |  2059 | `				return 0;` |
|        - |  2060 | `			}` |
|   112244 |  2061 | `			nIdx = pObj->nIdx;` |
|   112244 |  2062 | `			if( bDup ){` |
|        - |  2063 | `				/* Duplicate name */` |
|      230 |  2064 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2065 | `				if( zName == 0 ){` |
|      ! 0 |  2066 | `					return 0;` |
|        - |  2067 | `				}` |
|      114 |  2068 | `			}` |
|        - |  2069 | `			/* Link to the top active VM frame */` |
|   112244 |  2070 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   112244 |  2071 | `			if( rc != SXRET_OK ){` |
|        - |  2072 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2073 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2074 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2075 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2076 | `				return 0;` |
|        - |  2077 | `			}` |
|   112244 |  2078 | `			if( pFrame->pParent != 0 ){` |
|        - |  2079 | `				/* Local variable */` |
|   105280 |  2080 | `				sLocal.nIdx = nIdx;` |
|   105280 |  2081 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52641 |  2082 | `			}else{` |
|        - |  2083 | `				/* Register in the $GLOBALS array */` |
|     6966 |  2084 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2085 | `			}` |
|        - |  2086 | `			/* Install in the reference table */` |
|   112244 |  2087 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2088 | `			/* Save object index */` |
|   112244 |  2089 | `			pObj->nIdx = nIdx;` |
|    56123 |  2090 | `		}else{` |
|        - |  2091 | `			/* Extract variable contents */` |
|  3554598 |  2092 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3554598 |  2093 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3554598 |  2094 | `			if( bNullify && pObj ){` |
|      ! 0 |  2095 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2096 | `			}` |
|        - |  2097 | `		}` |
|  1833531 |  2098 | `	}else{` |
|        - |  2099 | `		/* Superglobal */` |
|       42 |  2100 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2101 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2102 | `	}` |
|  3666880 |  2103 | `	return pObj;` |
|  1834029 |  2104 |  |
|        - |  2105 | `/*` |
|        - |  2106 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2107 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2108 | ` */` |
|     3124 |  2109 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2110 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2111 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2112 | `	sxu32 nByte        /* zName length */` |
|        - |  2113 | `	)` |
|        2 |  2114 |  |
|        - |  2115 | `	SyHashEntry *pEntry;` |
|        - |  2116 | `	ph7_value *pValue;` |
|        - |  2117 | `	sxu32 nIdx;` |
|        - |  2118 | `	/* Query the superglobal table */` |
|     3126 |  2119 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3126 |  2120 | `	if( pEntry == 0 ){` |
|        - |  2121 | `		/* No such entry */` |
|      ! 0 |  2122 | `		return 0;` |
|        - |  2123 | `	}` |
|        - |  2124 | `	/* Extract the superglobal index in the global object pool */` |
|     3126 |  2125 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2126 | `	/* Extract the variable value  */` |
|     3126 |  2127 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3126 |  2128 | `	return pValue;` |
|     1564 |  2129 |  |
|        - |  2130 | `/*` |
|        - |  2131 | ` * Perform a raw hashmap insertion.` |
|        - |  2132 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2133 | ` */` |
|     3154 |  2134 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2135 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2136 | `	const char *zKey,   /* Entry key */` |
|        - |  2137 | `	int nKeylen,        /* zKey length*/` |
|        - |  2138 | `	const char *zData,  /* Entry data */` |
|        - |  2139 | `	int nLen            /* zData length */` |
|        - |  2140 | `	)` |
|        2 |  2141 |  |
|        - |  2142 | `	ph7_value sKey,sValue;` |
|        - |  2143 | `	sxi32 rc;` |
|     3156 |  2144 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3156 |  2145 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3156 |  2146 | `	if( zKey ){` |
|     3134 |  2147 | `		if( nKeylen < 0 ){` |
|     3082 |  2148 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1540 |  2149 | `		}` |
|     3134 |  2150 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1566 |  2151 | `	}` |
|     3156 |  2152 | `	if( zData ){` |
|     3156 |  2153 | `		if( nLen < 0 ){` |
|        - |  2154 | `			/* Compute length automatically */` |
|      144 |  2155 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2156 | `		}` |
|     3156 |  2157 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1577 |  2158 | `	}` |
|        - |  2159 | `	/* Perform the insertion */` |
|     3156 |  2160 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3156 |  2161 | `	PH7_MemObjRelease(&sKey);` |
|     3156 |  2162 | `	PH7_MemObjRelease(&sValue);` |
|     3156 |  2163 | `	return rc;` |
|        2 |  2164 |  |
|        - |  2165 | `/*` |
|        - |  2166 | ` * Configure a working virtual machine instance.` |
|        - |  2167 | ` *` |
|        - |  2168 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2169 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2170 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2171 | ` * The second argument to this function is an integer configuration option` |
|        - |  2172 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2173 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2174 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2175 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2176 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2177 | ` */` |
|    45450 |  2178 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2179 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2180 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2181 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2182 | `	)` |
|        2 |  2183 |  |
|    45452 |  2184 | `	sxi32 rc = SXRET_OK;` |
|    45452 |  2185 | `	switch(nOp){` |
|     1402 |  2186 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2806 |  2187 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2806 |  2188 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2189 | `		/* VM output consumer callback */` |
|        - |  2190 | `#ifdef UNTRUST` |
|        - |  2191 | `		if( xConsumer == 0 ){` |
|        - |  2192 | `			rc = SXERR_CORRUPT;` |
|        - |  2193 | `			break;` |
|        - |  2194 | `		}` |
|        - |  2195 | `#endif` |
|        - |  2196 | `		/* Install the output consumer */` |
|     2806 |  2197 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2806 |  2198 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2806 |  2199 | `		break;` |
|        - |  2200 | `							   }` |
|     1410 |  2201 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2202 | `		/* Import path */` |
|        - |  2203 | `		  const char *zPath;` |
|        - |  2204 | `		  SyString sPath;` |
|     2822 |  2205 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2206 | `#if defined(UNTRUST)` |
|        - |  2207 | `		  if( zPath == 0 ){` |
|        - |  2208 | `			  rc = SXERR_EMPTY;` |
|        - |  2209 | `			  break;` |
|        - |  2210 | `		  }` |
|        - |  2211 | `#endif` |
|     2822 |  2212 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2213 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2214 | `#ifdef __WINNT__` |
|        2 |  2215 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2216 | `#endif` |
|     5642 |  2217 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2218 | `		  /* Remove leading and trailing white spaces */` |
|     2822 |  2219 | `		  SyStringFullTrim(&sPath);` |
|     2822 |  2220 | `		  if( sPath.nByte > 0 ){` |
|        - |  2221 | `			  /* Store the path in the corresponding conatiner */` |
|     2822 |  2222 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1410 |  2223 | `		  }` |
|     2822 |  2224 | `		  break;` |
|        - |  2225 | `									 }` |
|     1410 |  2226 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2227 | `		/* Run-Time Error report */` |
|     2822 |  2228 | `		pVm->bErrReport = 1;` |
|     2822 |  2229 | `		break;` |
|      ! 0 |  2230 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2231 | `		/* Recursion depth */` |
|      ! 0 |  2232 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2233 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2234 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2235 | `		}` |
|      ! 0 |  2236 | `		break;` |
|        - |  2237 | `									   }` |
|      ! 0 |  2238 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2239 | `		/* VM output length in bytes */` |
|      ! 0 |  2240 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2241 | `#ifdef UNTRUST` |
|        - |  2242 | `		if( pOut == 0 ){` |
|        - |  2243 | `			rc = SXERR_CORRUPT;` |
|        - |  2244 | `			break;` |
|        - |  2245 | `		}` |
|        - |  2246 | `#endif` |
|      ! 0 |  2247 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2248 | `		break;` |
|        - |  2249 | `							   }` |
|        - |  2250 |  |
|    14100 |  2251 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2252 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2253 | `		/* Create a new superglobal/global variable */` |
|    28202 |  2254 | `		const char *zName = va_arg(ap,const char *);` |
|    28202 |  2255 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2256 | `		SyHashEntry *pEntry;` |
|        - |  2257 | `		ph7_value *pObj;` |
|        - |  2258 | `		sxu32 nByte;` |
|        - |  2259 | `		sxu32 nIdx;` |
|        - |  2260 | `#ifdef UNTRUST` |
|        - |  2261 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2262 | `			rc = SXERR_CORRUPT;` |
|        - |  2263 | `			break;` |
|        - |  2264 | `		}` |
|        - |  2265 | `#endif` |
|    28202 |  2266 | `		nByte = SyStrlen(zName);` |
|    28202 |  2267 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2268 | `			/* Check if the superglobal is already installed */` |
|    28202 |  2269 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14102 |  2270 | `		}else{` |
|        - |  2271 | `			/* Query the top active VM frame */` |
|      ! 0 |  2272 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2273 | `		}` |
|    28202 |  2274 | `		if( pEntry ){` |
|        - |  2275 | `			/* Variable already installed */` |
|      ! 0 |  2276 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2277 | `			/* Extract contents */` |
|      ! 0 |  2278 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2279 | `			if( pObj ){` |
|        - |  2280 | `				/* Overwrite old contents */` |
|      ! 0 |  2281 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2282 | `			}` |
|      ! 0 |  2283 | `		}else{` |
|        - |  2284 | `			/* Install a new variable */` |
|    28202 |  2285 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28202 |  2286 | `			if( pObj == 0 ){` |
|      ! 0 |  2287 | `				rc = SXERR_MEM;` |
|      ! 0 |  2288 | `				break;` |
|        - |  2289 | `			}` |
|    28202 |  2290 | `			nIdx = pObj->nIdx;` |
|        - |  2291 | `			/* Copy value */` |
|    28202 |  2292 | `			PH7_MemObjStore(pValue,pObj);` |
|    28202 |  2293 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2294 | `				/* Install the superglobal */` |
|    28202 |  2295 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14102 |  2296 | `			}else{` |
|        - |  2297 | `				/* Install in the current frame */` |
|      ! 0 |  2298 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2299 | `			}` |
|    28202 |  2300 | `			if( rc == SXRET_OK ){` |
|        - |  2301 | `				SyHashEntry *pRef;` |
|    28202 |  2302 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28202 |  2303 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14102 |  2304 | `				}else{` |
|      ! 0 |  2305 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2306 | `				}` |
|        - |  2307 | `				/* Install in the reference table */` |
|    28202 |  2308 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28202 |  2309 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2310 | `					/* Register in the $GLOBALS array */` |
|    28202 |  2311 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14100 |  2312 | `				}` |
|    14100 |  2313 | `			}` |
|        - |  2314 | `		}` |
|    28202 |  2315 | `		break;` |
|        - |  2316 | `									}` |
|     1540 |  2317 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2318 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2319 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2320 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2321 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2322 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2323 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3082 |  2324 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3082 |  2325 | `		const char *zValue = va_arg(ap,const char *);` |
|     3082 |  2326 | `		int nLen = va_arg(ap,int);` |
|        - |  2327 | `		ph7_hashmap *pMap;` |
|        - |  2328 | `		ph7_value *pValue;` |
|     3082 |  2329 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2330 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2331 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3081 |  2332 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2333 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2334 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3080 |  2335 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2336 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2337 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3080 |  2338 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2339 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2340 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3080 |  2341 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2342 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2343 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3080 |  2344 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2345 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2346 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2347 | `		}else{` |
|        - |  2348 | `			/* Extract the $_SERVER superglobal */` |
|     3080 |  2349 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2350 | `		}` |
|     3082 |  2351 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2352 | `			/* No such entry */` |
|      ! 0 |  2353 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2354 | `			break;` |
|        - |  2355 | `		}` |
|        - |  2356 | `		/* Point to the hashmap */` |
|     3082 |  2357 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2358 | `		/* Perform the insertion */` |
|     3082 |  2359 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3082 |  2360 | `		break;` |
|        - |  2361 | `								   }` |
|       11 |  2362 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2363 | `		/* Script arguments */` |
|       24 |  2364 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2365 | `		ph7_hashmap *pMap;` |
|        - |  2366 | `		ph7_value *pValue;` |
|        - |  2367 | `		sxu32 n;` |
|       24 |  2368 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2369 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2370 | `			break;` |
|        - |  2371 | `		}` |
|        - |  2372 | `		/* Extract the $argv array */` |
|       24 |  2373 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2374 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2375 | `			/* No such entry */` |
|      ! 0 |  2376 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2377 | `			break;` |
|        - |  2378 | `		}` |
|        - |  2379 | `		/* Point to the hashmap */` |
|       24 |  2380 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2381 | `		/* Perform the insertion */` |
|       24 |  2382 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2383 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2384 | `		if( rc == SXRET_OK ){` |
|       24 |  2385 | `			if( pMap->nEntry > 1 ){` |
|        - |  2386 | `				/* Append space separator first */` |
|       18 |  2387 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2388 | `			}` |
|       24 |  2389 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2390 | `		}` |
|       24 |  2391 | `		break;` |
|        - |  2392 | `								  }` |
|      ! 0 |  2393 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2394 | `		/* error_log() consumer */` |
|      ! 0 |  2395 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2396 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2397 | `		break;` |
|        - |  2398 | `										}` |
|      ! 0 |  2399 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2400 | `		/* Script return value */` |
|      ! 0 |  2401 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2402 | `#ifdef UNTRUST` |
|        - |  2403 | `		if( ppValue == 0 ){` |
|        - |  2404 | `			rc = SXERR_CORRUPT;` |
|        - |  2405 | `			break;` |
|        - |  2406 | `		}` |
|        - |  2407 | `#endif` |
|      ! 0 |  2408 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2409 | `		break;` |
|        - |  2410 | `								   }` |
|     2820 |  2411 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2412 | `		/* Register an IO stream device */` |
|     5642 |  2413 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2414 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8460 |  2415 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5642 |  2416 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2417 | `				/* Invalid stream */` |
|      ! 0 |  2418 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2419 | `				break;` |
|        - |  2420 | `		}` |
|     5642 |  2421 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2422 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2822 |  2423 | `			pVm->pDefStream = pStream;` |
|     1410 |  2424 | `		}` |
|        - |  2425 | `		/* Insert in the appropriate container */` |
|     5642 |  2426 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5642 |  2427 | `		break;` |
|        - |  2428 | `								  }` |
|        8 |  2429 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2430 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2431 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2432 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2433 | `#ifdef UNTRUST` |
|        - |  2434 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2435 | `			rc = SXERR_CORRUPT;` |
|        - |  2436 | `			break;` |
|        - |  2437 | `		}` |
|        - |  2438 | `#endif` |
|       16 |  2439 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2440 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2441 | `		break;` |
|        - |  2442 | `									   }` |
|        8 |  2443 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2444 | `		/* Raw HTTP request*/` |
|       16 |  2445 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2446 | `		int nByte = va_arg(ap,int);` |
|       16 |  2447 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2448 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2449 | `			break;` |
|        - |  2450 | `		}` |
|       16 |  2451 | `		if( nByte < 0 ){` |
|        - |  2452 | `			/* Compute length automatically */` |
|      ! 0 |  2453 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2454 | `		}` |
|        - |  2455 | `		/* Process the request */` |
|       16 |  2456 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2457 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2458 | `		if( rc == SXRET_OK ){` |
|       16 |  2459 | `			pVm->bHttpContext = 1;` |
|        8 |  2460 | `		}` |
|       16 |  2461 | `		break;` |
|        - |  2462 | `									}` |
|        8 |  2463 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2464 | `		/* Extract HTTP response status code */` |
|       16 |  2465 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2466 | `		if( pStatus ){` |
|       16 |  2467 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2468 | `		}` |
|       16 |  2469 | `		break;` |
|        - |  2470 | `										}` |
|        8 |  2471 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2472 | `		/* Iterate response headers via callback */` |
|        - |  2473 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2474 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2475 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2476 | `		if( xCallback ){` |
|       16 |  2477 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2478 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2479 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2480 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2481 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2482 | `							   pUserData);` |
|       12 |  2483 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2484 | `					break;` |
|        - |  2485 | `				}` |
|        6 |  2486 | `			}` |
|        8 |  2487 | `		}` |
|       16 |  2488 | `		break;` |
|        - |  2489 | `										 }` |
|      ! 0 |  2490 | `	default:` |
|        - |  2491 | `		/* Unknown configuration option */` |
|      ! 0 |  2492 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2493 | `		break;` |
|        - |  2494 | `	}` |
|    45452 |  2495 | `	return rc;` |
|        2 |  2496 |  |
|        - |  2497 | `/* Forward declaration */` |
|        - |  2498 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2499 | `/*` |
|        - |  2500 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2501 | ` * format.` |
|        - |  2502 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2503 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2504 | ` * (STDOUT).` |
|        - |  2505 | ` */` |
|        2 |  2506 | `static sxi32 VmByteCodeDump(` |
|        - |  2507 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2508 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2509 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2510 | `	)` |
|        1 |  2511 |  |
|        - |  2512 | `	static const char zDump[] = {` |
|        - |  2513 | `		"====================================================\n"` |
|        - |  2514 | `		"PH7 VM Dump\n"` |
|        - |  2515 | `		"====================================================\n"` |
|        - |  2516 | `	};` |
|        - |  2517 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2518 | `	sxi32 rc = SXRET_OK;` |
|        - |  2519 | `	sxu32 n;` |
|        - |  2520 | `	/* Point to the PH7 instructions */` |
|        3 |  2521 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2522 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2523 | `	n = 0;` |
|        3 |  2524 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2525 | `	/* Dump instructions */` |
|        7 |  2526 | `	for(;;){` |
|       15 |  2527 | `		if( pInstr >= pEnd ){` |
|        - |  2528 | `			/* No more instructions */` |
|        3 |  2529 | `			break;` |
|        - |  2530 | `		}` |
|        - |  2531 | `		/* Format and call the consumer callback */` |
|       19 |  2532 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2533 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2534 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2535 | `		if( rc != SXRET_OK ){` |
|        - |  2536 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2537 | `			return rc;` |
|        - |  2538 | `		}` |
|       13 |  2539 | `		++n;` |
|       13 |  2540 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2541 | `	}` |
|        3 |  2542 | `	return rc;` |
|        2 |  2543 |  |
|        - |  2544 | `/* Forward declaration */` |
|        - |  2545 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2546 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2547 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2548 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2549 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2550 | `/*` |
|        - |  2551 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2552 | ` * consumer callback.` |
|        - |  2553 | ` */` |
|      600 |  2554 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2555 |  |
|      601 |  2556 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      601 |  2557 | `	sxi32 rc = SXRET_OK;` |
|        - |  2558 | `	/* Append a new line */` |
|        - |  2559 | `#ifdef __WINNT__` |
|        1 |  2560 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2561 | `#else` |
|      600 |  2562 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2563 | `#endif` |
|        - |  2564 | `	/* Invoke the output consumer callback */` |
|      601 |  2565 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      601 |  2566 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      601 |  2567 | `	return rc;` |
|        1 |  2568 |  |
|        - |  2569 | `/*` |
|        - |  2570 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2571 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2572 | ` * information.` |
|        - |  2573 | ` */` |
|      148 |  2574 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2575 |  |
|      150 |  2576 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2577 | `		ph7_value apArg[4];` |
|        - |  2578 | `		ph7_value *apArgPtr[4];` |
|        - |  2579 | `		ph7_value sResult;` |
|        - |  2580 | `		SyString sErr;` |
|        - |  2581 | `		/* Prepare arguments */` |
|       76 |  2582 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2583 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2584 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2585 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2586 | `		if( pFile ){` |
|       76 |  2587 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2588 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2589 | `		}else{` |
|      ! 0 |  2590 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2591 | `		}` |
|       76 |  2592 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2593 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2594 | `		/* Set up pointer array */` |
|       76 |  2595 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2596 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2597 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2598 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2599 | `		/* Call the handler */` |
|       76 |  2600 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2601 | `		/* Check return value */` |
|       76 |  2602 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2603 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2604 | `		}` |
|        - |  2605 | `		/* Release */` |
|       76 |  2606 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2607 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2608 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2609 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2610 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2611 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2612 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2613 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2614 | `	}` |
|        - |  2615 | `	/* No handler, always call error handler */` |
|       75 |  2616 | `	return TRUE;` |
|       76 |  2617 |  |
|      110 |  2618 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2619 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2620 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2621 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2622 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2623 | `	)` |
|        2 |  2624 |  |
|      112 |  2625 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2626 | `	SyString *pFile;` |
|        - |  2627 | `	char *zErr;` |
|      112 |  2628 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2629 | `	if( !pVm->bErrReport ){` |
|        - |  2630 | `		/* Don't bother reporting errors */` |
|        3 |  2631 | `		return SXRET_OK;` |
|        - |  2632 | `	}` |
|        - |  2633 | `	/* Reset the working buffer */` |
|      110 |  2634 | `	SyBlobReset(pWorker);` |
|        - |  2635 | `	/* Peek the processed file if available */` |
|      110 |  2636 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2637 | `	if( pFile ){` |
|        - |  2638 | `		/* Append file name */` |
|      110 |  2639 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2640 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2641 | `	}` |
|        - |  2642 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2643 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2644 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2645 | `	 * E_DEPRECATED). */` |
|      110 |  2646 | `	zErr = "Error:  ";` |
|      110 |  2647 | `	switch(iErr){` |
|       19 |  2648 | `	case PH7_CTX_WARNING:` |
|       40 |  2649 | `		zErr = "Warning:  ";` |
|       40 |  2650 | `		break;` |
|        6 |  2651 | `	case PH7_CTX_NOTICE:` |
|       14 |  2652 | `		zErr = "Notice:  ";` |
|       12 |  2653 | `		break;` |
|       29 |  2654 | `	default:` |
|        - |  2655 | `		/* keep iErr unchanged */` |
|       58 |  2656 | `		break;` |
|        - |  2657 | `	}` |
|      110 |  2658 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2659 | `	if( pFuncName ){` |
|        - |  2660 | `		/* Append function name first */` |
|       23 |  2661 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2662 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2663 | `	}` |
|      110 |  2664 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2665 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2666 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2667 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2668 | `	}` |
|      110 |  2669 | `	return rc;` |
|       57 |  2670 |  |
|        - |  2671 | `/*` |
|        - |  2672 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  2673 | ` *` |
|        - |  2674 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  2675 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  2676 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  2677 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  2678 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  2679 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  2680 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  2681 | ` * calling it from a VM op.` |
|        - |  2682 | ` */` |
|      ! 0 |  2683 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  2684 |  |
|      ! 0 |  2685 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  2686 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  2687 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  2688 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  2689 | `	return PH7_ABORT;` |
|      ! 0 |  2690 |  |
|        - |  2691 | `/*` |
|        - |  2692 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  2693 | ` */` |
|      ! 0 |  2694 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  2695 |  |
|      ! 0 |  2696 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  2697 |  |
|        - |  2698 | `/*` |
|        - |  2699 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2700 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2701 | ` * information.` |
|        - |  2702 | ` */` |
|       40 |  2703 | `static sxi32 VmThrowErrorAp(` |
|        - |  2704 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2705 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2706 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2707 | `	const char *zFormat, /* Format message */` |
|        - |  2708 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2709 | `	)` |
|        2 |  2710 |  |
|       42 |  2711 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2712 | `	SyBlob sMsg;` |
|        - |  2713 | `	SyString *pFile;` |
|        - |  2714 | `	char *zErr;` |
|       42 |  2715 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2716 | `	if( !pVm->bErrReport ){` |
|        - |  2717 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2718 | `		return SXRET_OK;` |
|        - |  2719 | `	}` |
|        - |  2720 | `	/* Reset the working buffer */` |
|       42 |  2721 | `	SyBlobReset(pWorker);` |
|        - |  2722 | `	/* Peek the processed file if available */` |
|       42 |  2723 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2724 | `	if( pFile ){` |
|        - |  2725 | `		/* Append file name */` |
|       42 |  2726 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2727 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2728 | `	}` |
|        - |  2729 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2730 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2731 | `	 * the correct errno value. */` |
|       42 |  2732 | `	zErr = "Error:  ";` |
|       42 |  2733 | `	switch(iErr){` |
|        4 |  2734 | `	case PH7_CTX_WARNING:` |
|        9 |  2735 | `		zErr = "Warning:  ";` |
|        9 |  2736 | `		break;` |
|        3 |  2737 | `	case PH7_CTX_NOTICE:` |
|        7 |  2738 | `		zErr = "Notice:  ";` |
|        6 |  2739 | `		break;` |
|       13 |  2740 | `	default:` |
|        - |  2741 | `		/* do not change iErr */` |
|       26 |  2742 | `		break;` |
|        - |  2743 | `	}` |
|       42 |  2744 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2745 | `	if( pFuncName ){` |
|        - |  2746 | `		/* Append function name first */` |
|       26 |  2747 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2748 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2749 | `	}` |
|        - |  2750 | `	/* Format the raw message */` |
|       42 |  2751 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2752 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2753 | `	/* Check if a user error handler is installed */` |
|       42 |  2754 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2755 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2756 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2757 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2758 | `	}` |
|       42 |  2759 | `	SyBlobRelease(&sMsg);` |
|       42 |  2760 | `	return rc;` |
|       22 |  2761 |  |
|        - |  2762 | `/*` |
|        - |  2763 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2764 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2765 | ` * possible.` |
|        - |  2766 | ` */` |
|       38 |  2767 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2768 |  |
|        - |  2769 | `	ph7_class *pClass;` |
|       39 |  2770 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2771 | `	ph7_class_instance *pThis;` |
|        - |  2772 | `	ph7_class_method *pCons;` |
|        - |  2773 | `	ph7_value sArg;` |
|        - |  2774 | `	ph7_value *apArg[1];` |
|        - |  2775 | `	SyBlob sMsg;` |
|        - |  2776 | `	SyString sMsgStr;` |
|        - |  2777 | `	VmFrame *pFrame;` |
|        - |  2778 | `	sxi32 rc;` |
|       39 |  2779 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2780 | `	if( pClass == 0 ){` |
|      ! 0 |  2781 | `		return PH7_ABORT;` |
|        - |  2782 | `	}` |
|       39 |  2783 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2784 | `	if( pThis == 0 ){` |
|      ! 0 |  2785 | `		return PH7_ABORT;` |
|        - |  2786 | `	}` |
|       39 |  2787 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2788 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2789 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2790 | `	{` |
|       39 |  2791 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2792 | `		if( pOwner ){` |
|       39 |  2793 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2794 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2795 | `		}else{` |
|      ! 0 |  2796 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2797 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2798 | `		}` |
|        - |  2799 | `	}` |
|       39 |  2800 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2801 | `	if( pCons ){` |
|       39 |  2802 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2803 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2804 | `		apArg[0] = &sArg;` |
|       39 |  2805 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2806 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2807 | `	}` |
|       39 |  2808 | `	SyBlobRelease(&sMsg);` |
|       39 |  2809 | `	pFrame = pVm->pFrame;` |
|       39 |  2810 | `	if( pFrame ){` |
|       39 |  2811 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2812 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2813 | `	}` |
|       39 |  2814 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2815 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2816 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2817 | `		return PH7_ABORT;` |
|        - |  2818 | `	}` |
|       39 |  2819 | `	return PH7_EXCEPTION;` |
|       20 |  2820 |  |
|        - |  2821 |  |
|        - |  2822 | `/*` |
|        - |  2823 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2824 | ` */` |
|        4 |  2825 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2826 |  |
|        - |  2827 | `	ph7_class *pErrClass;` |
|        - |  2828 | `	ph7_class_instance *pThis;` |
|        - |  2829 | `	ph7_class_method *pCons;` |
|        - |  2830 | `	ph7_value sArg;` |
|        - |  2831 | `	ph7_value *apArg[1];` |
|        - |  2832 | `	SyBlob sMsg;` |
|        - |  2833 | `	SyString sMsgStr;` |
|        - |  2834 | `	VmFrame *pFrame;` |
|        - |  2835 | `	sxi32 rc;` |
|        5 |  2836 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2837 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2838 | `		return PH7_ABORT;` |
|        - |  2839 | `	}` |
|        5 |  2840 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2841 | `	if( pThis == 0 ){` |
|      ! 0 |  2842 | `		return PH7_ABORT;` |
|        - |  2843 | `	}` |
|        5 |  2844 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2845 | `	{` |
|        5 |  2846 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2847 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2848 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2849 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2850 | `	}` |
|        5 |  2851 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2852 | `	if( pCons ){` |
|        5 |  2853 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2854 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2855 | `		apArg[0] = &sArg;` |
|        5 |  2856 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2857 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2858 | `	}` |
|        5 |  2859 | `	SyBlobRelease(&sMsg);` |
|        5 |  2860 | `	pFrame = pVm->pFrame;` |
|        5 |  2861 | `	if( pFrame ){` |
|        5 |  2862 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2863 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2864 | `	}` |
|        5 |  2865 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2866 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2867 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2868 | `		return PH7_ABORT;` |
|        - |  2869 | `	}` |
|        5 |  2870 | `	return PH7_EXCEPTION;` |
|        3 |  2871 |  |
|        - |  2872 |  |
|        - |  2873 | `/*` |
|        - |  2874 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2875 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2876 | ` * For class types, instanceof is verified.` |
|        - |  2877 | ` *` |
|        - |  2878 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2879 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2880 | ` */` |
|        - |  2881 | `/*` |
|        - |  2882 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2883 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2884 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2885 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2886 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2887 | ` */` |
|       20 |  2888 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2889 |  |
|        - |  2890 | `	const char *z, *zEnd, *zTail;` |
|        - |  2891 | `	sxu32 n;` |
|        - |  2892 | `	sxu8 bReal;` |
|        - |  2893 | `	sxi32 rc;` |
|       22 |  2894 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2895 | `		return 0;` |
|        - |  2896 | `	}` |
|       22 |  2897 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2898 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2899 | `	zEnd = z + n;` |
|       22 |  2900 | `	if( n == 0 ){` |
|      ! 0 |  2901 | `		return 0;` |
|        - |  2902 | `	}` |
|       22 |  2903 | `	zTail = 0;` |
|       22 |  2904 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2905 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2906 | `		return 0;` |
|        - |  2907 | `	}` |
|        - |  2908 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2909 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2910 | `		zTail++;` |
|      ! 0 |  2911 | `	}` |
|       16 |  2912 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2913 |  |
|        - |  2914 |  |
|        - |  2915 | `/*` |
|        - |  2916 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2917 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2918 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2919 | ` *   0 if it's not strictly numeric.` |
|        - |  2920 | ` */` |
|       16 |  2921 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2922 |  |
|        - |  2923 | `	const char *z, *zEnd, *zTail;` |
|        - |  2924 | `	sxu32 n;` |
|       18 |  2925 | `	sxu8 bReal = 0;` |
|        - |  2926 | `	sxi32 rc;` |
|       18 |  2927 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2928 | `		return 0;` |
|        - |  2929 | `	}` |
|       18 |  2930 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2931 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2932 | `	zEnd = z + n;` |
|       18 |  2933 | `	if( n == 0 ) return 0;` |
|       18 |  2934 | `	zTail = 0;` |
|       18 |  2935 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2936 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2937 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2938 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2939 | `	return bReal ? 2 : 1;` |
|       10 |  2940 |  |
|        - |  2941 |  |
|        - |  2942 | `/*` |
|        - |  2943 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2944 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2945 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2946 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2947 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2948 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2949 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2950 | ` * throw.` |
|        - |  2951 | ` *` |
|        - |  2952 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2953 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2954 | ` */` |
|       98 |  2955 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2956 |  |
|        - |  2957 | `	sxu32 i;` |
|        - |  2958 | `	ph7_type_alt *aAlts;` |
|        - |  2959 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2960 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2961 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2962 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2963 | `	}` |
|       88 |  2964 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2965 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2966 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2967 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2968 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2969 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2970 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2971 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2972 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2973 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2974 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2975 | `	}` |
|        - |  2976 | `	/* Object handling */` |
|       88 |  2977 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2978 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2979 | `		if( bHasClassAlt ){` |
|       14 |  2980 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2981 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2982 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2983 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2984 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2985 | `			}` |
|       26 |  2986 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2987 | `				ph7_class *pExpected;` |
|        - |  2988 | `				SyString *pCN;` |
|       22 |  2989 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2990 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2991 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2992 | `					pExpected = pSelfNow;` |
|       22 |  2993 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2994 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2995 | `				}else{` |
|       22 |  2996 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2997 | `				}` |
|       22 |  2998 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2999 | `					return SXRET_OK;` |
|        - |  3000 | `				}` |
|        8 |  3001 | `			}` |
|        2 |  3002 | `		}` |
|        9 |  3003 | `		return SXERR_INVALID;` |
|        - |  3004 | `	}` |
|        - |  3005 | `	/* Array handling */` |
|       72 |  3006 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3007 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3008 | `	}` |
|        - |  3009 | `	/* Scalar handling — exact match first */` |
|       66 |  3010 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  3011 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3012 | `	}` |
|       42 |  3013 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3014 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3015 | `	}` |
|       38 |  3016 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  3017 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3018 | `	}` |
|       18 |  3019 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3020 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3021 | `	}` |
|       18 |  3022 | `	if( bStrict ){` |
|        - |  3023 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3024 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3025 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3026 | `			return SXRET_OK;` |
|        - |  3027 | `		}` |
|      ! 0 |  3028 | `		return SXERR_INVALID;` |
|        - |  3029 | `	}` |
|        - |  3030 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3031 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3032 | `	 * to match PHP's union RFC. */` |
|        - |  3033 | `	{` |
|       18 |  3034 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3035 | `		if( bHasInt ){` |
|        - |  3036 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3037 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3038 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3039 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3040 | `				return SXRET_OK;` |
|        - |  3041 | `			}` |
|       18 |  3042 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3043 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3044 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3045 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3046 | `					return SXRET_OK;` |
|        - |  3047 | `				}` |
|      ! 0 |  3048 | `			}` |
|       18 |  3049 | `			if( kind == 1 ){` |
|        9 |  3050 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3051 | `				return SXRET_OK;` |
|        - |  3052 | `			}` |
|        4 |  3053 | `		}` |
|       10 |  3054 | `		if( bHasFloat ){` |
|       10 |  3055 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3056 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3057 | `				return SXRET_OK;` |
|        - |  3058 | `			}` |
|       10 |  3059 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3060 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3061 | `				return SXRET_OK;` |
|        - |  3062 | `			}` |
|        1 |  3063 | `		}` |
|        3 |  3064 | `		if( bHasString ){` |
|      ! 0 |  3065 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3066 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3067 | `				return SXRET_OK;` |
|        - |  3068 | `			}` |
|      ! 0 |  3069 | `		}` |
|        3 |  3070 | `		if( bHasBool ){` |
|      ! 0 |  3071 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3072 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3073 | `				return SXRET_OK;` |
|        - |  3074 | `			}` |
|      ! 0 |  3075 | `		}` |
|        - |  3076 | `	}` |
|        3 |  3077 | `	return SXERR_INVALID;` |
|       51 |  3078 |  |
|        - |  3079 |  |
|        - |  3080 | `/*` |
|        - |  3081 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3082 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3083 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3084 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3085 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3086 | ` */` |
|       36 |  3087 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3088 |  |
|       38 |  3089 | `	if( bStrict ){` |
|        - |  3090 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3091 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3092 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3093 | `			return SXRET_OK;` |
|        - |  3094 | `		}` |
|       10 |  3095 | `		return SXERR_INVALID;` |
|        - |  3096 | `	}` |
|        - |  3097 | `	{` |
|       28 |  3098 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3099 | `		if( xCast ) xCast(pVal);` |
|        - |  3100 | `	}` |
|       28 |  3101 | `	return SXRET_OK;` |
|       20 |  3102 |  |
|        - |  3103 |  |
|        - |  3104 | `/*` |
|        - |  3105 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3106 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3107 | ` *` |
|        - |  3108 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3109 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3110 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3111 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3112 | ` */` |
|       10 |  3113 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3114 |  |
|       12 |  3115 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       12 |  3116 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       12 |  3117 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       12 |  3118 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       12 |  3119 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        5 |  3120 | `		}` |
|       12 |  3121 | `		zBuf[nCopy] = 0;` |
|       12 |  3122 | `		return zBuf;` |
|        - |  3123 | `	}` |
|      ! 0 |  3124 | `	switch( nType ){` |
|      ! 0 |  3125 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3126 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3127 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3128 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3129 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3130 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3131 | `		default:             return "scalar";` |
|        - |  3132 | `	}` |
|        7 |  3133 |  |
|        - |  3134 |  |
|        - |  3135 | `/*` |
|        - |  3136 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3137 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3138 | ` */` |
|       18 |  3139 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3140 |  |
|       19 |  3141 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3142 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3143 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3144 | `	return zBuf;` |
|        1 |  3145 |  |
|        - |  3146 |  |
|    14626 |  3147 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3148 |  |
|        - |  3149 | `	SyHashEntry *pSlot;` |
|        - |  3150 | `	VmClassAttr *pVmAttr;` |
|        - |  3151 | `	ph7_class_attr *pAttr;` |
|    14628 |  3152 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14628 |  3153 | `	if( pSlot == 0 ){` |
|    14420 |  3154 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3155 | `	}` |
|      210 |  3156 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3157 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3158 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3159 | `		return SXRET_OK;` |
|        - |  3160 | `	}` |
|        - |  3161 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3162 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3163 | `	 * matching PHP's documented behavior. */` |
|      210 |  3164 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3165 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3166 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3167 |  |
|       16 |  3168 | `		if( rc == SXRET_OK ){` |
|        9 |  3169 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3170 | `			return SXRET_OK;` |
|        - |  3171 | `		}` |
|        7 |  3172 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3173 | `			char zBuf[128];` |
|        4 |  3174 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3175 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3176 | `		}` |
|        5 |  3177 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3178 | `	}` |
|        - |  3179 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3180 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3181 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3182 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3183 | `			return SXRET_OK;` |
|        - |  3184 | `		}` |
|        3 |  3185 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3186 | `	}` |
|        - |  3187 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3188 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3189 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3190 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3191 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3192 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3193 | `			return SXRET_OK;` |
|        - |  3194 | `		}` |
|        7 |  3195 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3196 | `	}` |
|      174 |  3197 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3198 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3199 | `		 * currently active on the self-stack. */` |
|       26 |  3200 | `		ph7_class *pExpected = 0;` |
|       26 |  3201 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3202 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3203 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3204 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3205 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3206 | `		}` |
|       26 |  3207 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3208 | `			pExpected = pSelfNow;` |
|       24 |  3209 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3210 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3211 | `		}else{` |
|       22 |  3212 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3213 | `		}` |
|       26 |  3214 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3215 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3216 | `		}` |
|       26 |  3217 | `		if( pExpected ){` |
|       22 |  3218 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3219 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3220 | `				char zBuf[128];` |
|        7 |  3221 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3222 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3223 | `			}` |
|        8 |  3224 | `		}` |
|       22 |  3225 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3226 | `		return SXRET_OK;` |
|        - |  3227 | `	}` |
|        - |  3228 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3229 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3230 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3231 | `		char zBuf[128];` |
|       10 |  3232 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3233 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3234 | `	}` |
|      144 |  3235 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3236 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3237 | `		if( xCast ){` |
|        - |  3238 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3239 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3240 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3241 | `			}` |
|       24 |  3242 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3243 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3244 | `			}` |
|        - |  3245 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3246 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3247 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3248 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3249 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3250 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3251 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3252 | `			}` |
|       12 |  3253 | `			xCast(pValue);` |
|        5 |  3254 | `		}` |
|        5 |  3255 | `	}` |
|      130 |  3256 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3257 | `	return SXRET_OK;` |
|     7315 |  3258 |  |
|        - |  3259 |  |
|        - |  3260 | `/*` |
|        - |  3261 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3262 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3263 | ` * information.` |
|        - |  3264 | ` * ------------------------------------` |
|        - |  3265 | ` * Simple boring wrapper function.` |
|        - |  3266 | ` * ------------------------------------` |
|        - |  3267 | ` */` |
|       16 |  3268 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3269 |  |
|        - |  3270 | `	va_list ap;` |
|        - |  3271 | `	sxi32 rc;` |
|       17 |  3272 | `	va_start(ap,zFormat);` |
|       17 |  3273 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3274 | `	va_end(ap);` |
|       17 |  3275 | `	return rc;` |
|        1 |  3276 |  |
|        - |  3277 | `/*` |
|        - |  3278 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3279 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3280 | ` */` |
|       36 |  3281 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3282 |  |
|        - |  3283 | `	ph7_class *pClass;` |
|        - |  3284 | `	ph7_class_instance *pThis;` |
|        - |  3285 | `	ph7_class_method *pCons;` |
|        - |  3286 | `	ph7_value sArg;` |
|        - |  3287 | `	ph7_value *apArg[1];` |
|        - |  3288 | `	SyBlob sMsg;` |
|        - |  3289 | `	SyString sMsgStr;` |
|        - |  3290 | `	VmFrame *pFrame;` |
|        - |  3291 | `	sxi32 rc;` |
|       38 |  3292 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       38 |  3293 | `	if( pClass == 0 ){` |
|      ! 0 |  3294 | `		return PH7_ABORT;` |
|        - |  3295 | `	}` |
|       38 |  3296 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       38 |  3297 | `	if( pThis == 0 ){` |
|      ! 0 |  3298 | `		return PH7_ABORT;` |
|        - |  3299 | `	}` |
|       38 |  3300 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       38 |  3301 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       18 |  3302 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       38 |  3303 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       38 |  3304 | `	if( pCons ){` |
|       38 |  3305 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       38 |  3306 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       38 |  3307 | `		apArg[0] = &sArg;` |
|       38 |  3308 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       38 |  3309 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  3310 | `	}` |
|       38 |  3311 | `	SyBlobRelease(&sMsg);` |
|       38 |  3312 | `	pFrame = pVm->pFrame;` |
|       38 |  3313 | `	if( pFrame ){` |
|       38 |  3314 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  3315 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  3316 | `	}` |
|       38 |  3317 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  3318 | `	PH7_ClassInstanceUnref(pThis);` |
|       38 |  3319 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3320 | `		return PH7_ABORT;` |
|        - |  3321 | `	}` |
|       34 |  3322 | `	return PH7_EXCEPTION;` |
|       20 |  3323 |  |
|        - |  3324 | `/*` |
|        - |  3325 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3326 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3327 | ` */` |
|        6 |  3328 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3329 |  |
|        - |  3330 | `	ph7_class *pClass;` |
|        - |  3331 | `	ph7_class_instance *pThis;` |
|        - |  3332 | `	ph7_class_method *pCons;` |
|        - |  3333 | `	ph7_value sArg;` |
|        - |  3334 | `	ph7_value *apArg[1];` |
|        - |  3335 | `	SyBlob sMsg;` |
|        - |  3336 | `	SyString sMsgStr;` |
|        - |  3337 | `	VmFrame *pFrame;` |
|        - |  3338 | `	sxi32 rc;` |
|        7 |  3339 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3340 | `	if( pClass == 0 ){` |
|      ! 0 |  3341 | `		return PH7_ABORT;` |
|        - |  3342 | `	}` |
|        7 |  3343 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3344 | `	if( pThis == 0 ){` |
|      ! 0 |  3345 | `		return PH7_ABORT;` |
|        - |  3346 | `	}` |
|        7 |  3347 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3348 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3349 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3350 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3351 | `	if( pCons ){` |
|        7 |  3352 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3353 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3354 | `		apArg[0] = &sArg;` |
|        7 |  3355 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3356 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3357 | `	}` |
|        7 |  3358 | `	SyBlobRelease(&sMsg);` |
|        7 |  3359 | `	pFrame = pVm->pFrame;` |
|        7 |  3360 | `	if( pFrame ){` |
|        7 |  3361 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3362 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3363 | `	}` |
|        7 |  3364 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3365 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3366 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3367 | `		return PH7_ABORT;` |
|        - |  3368 | `	}` |
|      ! 0 |  3369 | `	return PH7_EXCEPTION;` |
|        4 |  3370 |  |
|        - |  3371 | `/*` |
|        - |  3372 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3373 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3374 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3375 | ` */` |
|       16 |  3376 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3377 |  |
|       17 |  3378 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3379 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3380 | `	}` |
|       13 |  3381 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3382 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3383 | `		if( pThis && pThis->pClass ){` |
|        5 |  3384 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3385 | `			sxu32 n = pName->nByte;` |
|        5 |  3386 | `			if( n >= nBuf ){` |
|      ! 0 |  3387 | `				n = nBuf - 1;` |
|      ! 0 |  3388 | `			}` |
|        5 |  3389 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3390 | `			zBuf[n] = 0;` |
|        5 |  3391 | `			return zBuf;` |
|        - |  3392 | `		}` |
|      ! 0 |  3393 | `		return "object";` |
|        - |  3394 | `	}` |
|        9 |  3395 | `	return ph7_type_name(pVal);` |
|        9 |  3396 |  |
|        - |  3397 | `/*` |
|        - |  3398 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3399 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3400 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3401 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3402 | ` */` |
|       16 |  3403 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3404 |  |
|        - |  3405 | `	ph7_class *pClass;` |
|        - |  3406 | `	ph7_class_instance *pThis;` |
|        - |  3407 | `	ph7_class_method *pCons;` |
|        - |  3408 | `	ph7_value sArg;` |
|        - |  3409 | `	ph7_value *apArg[1];` |
|        - |  3410 | `	SyBlob sMsg;` |
|        - |  3411 | `	SyString sMsgStr;` |
|        - |  3412 | `	VmFrame *pFrame;` |
|        - |  3413 | `	sxi32 rc;` |
|       17 |  3414 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3415 | `	char zNameBuf[64];` |
|       17 |  3416 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3417 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3418 | `	if( pClass == 0 ){` |
|      ! 0 |  3419 | `		return PH7_ABORT;` |
|        - |  3420 | `	}` |
|       17 |  3421 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3422 | `	if( pThis == 0 ){` |
|      ! 0 |  3423 | `		return PH7_ABORT;` |
|        - |  3424 | `	}` |
|       17 |  3425 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3426 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3427 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3428 | `	if( pCons ){` |
|       17 |  3429 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3430 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3431 | `		apArg[0] = &sArg;` |
|       17 |  3432 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3433 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3434 | `	}` |
|       17 |  3435 | `	SyBlobRelease(&sMsg);` |
|       17 |  3436 | `	pFrame = pVm->pFrame;` |
|       17 |  3437 | `	if( pFrame ){` |
|       17 |  3438 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3439 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3440 | `	}` |
|       17 |  3441 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3442 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3443 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3444 | `		return PH7_ABORT;` |
|        - |  3445 | `	}` |
|       17 |  3446 | `	return PH7_EXCEPTION;` |
|        9 |  3447 |  |
|        - |  3448 | `/*` |
|        - |  3449 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3450 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3451 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3452 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3453 | ` */` |
|        - |  3454 | `/*` |
|        - |  3455 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3456 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3457 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3458 | ` */` |
|       24 |  3459 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3460 |  |
|        - |  3461 | `	sxu32 nCopy;` |
|       26 |  3462 | `	if( nBuf == 0 ) return "";` |
|       26 |  3463 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3464 | `		zBuf[0] = 0;` |
|      ! 0 |  3465 | `		return zBuf;` |
|        - |  3466 | `	}` |
|       26 |  3467 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3468 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3469 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3470 | `	zBuf[nCopy] = 0;` |
|       26 |  3471 | `	return zBuf;` |
|       14 |  3472 |  |
|        - |  3473 |  |
|      396 |  3474 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3475 |  |
|      398 |  3476 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3477 | `	const char *zGiven;` |
|        - |  3478 | `	char zBuf[128];` |
|        - |  3479 | `	char zTypeBuf[128];` |
|        - |  3480 | `	/* Untyped function: no enforcement. */` |
|      398 |  3481 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3482 | `		return SXRET_OK;` |
|        - |  3483 | `	}` |
|        - |  3484 | `	/* void return type: the function must not produce a value. */` |
|      398 |  3485 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3486 | `		if( pValue == 0 ){` |
|      134 |  3487 | `			return SXRET_OK;` |
|        - |  3488 | `		}` |
|        - |  3489 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3490 | `		 * still counts as "returned a value" here. */` |
|        3 |  3491 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3492 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3493 | `	}` |
|        - |  3494 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3495 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3496 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      264 |  3497 | `	if( pValue == 0 ){` |
|      ! 0 |  3498 | `		const char *zExpected = "value";` |
|      ! 0 |  3499 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3500 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3501 | `		}` |
|      ! 0 |  3502 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3503 | `	}` |
|        - |  3504 | ``	/* `mixed` accepts any explicitly returned value, including null. It is`` |
|        - |  3505 | `	 * parsed as a class-name atom (SXU32_HIGH, sReturnClass = "mixed") since` |
|        - |  3506 | `	 * it is not a scalar keyword, so short-circuit it here before the null /` |
|        - |  3507 | `	 * class-type checks below — which would otherwise demand an object. */` |
|      272 |  3508 | `	if( pFunc->nReturnType == SXU32_HIGH` |
|      143 |  3509 | `	 && pFunc->sReturnClass.nByte == 5` |
|       24 |  3510 | `	 && SyStrnicmp(pFunc->sReturnClass.zString,"mixed",5) == 0 ){` |
|       21 |  3511 | `		return SXRET_OK;` |
|        - |  3512 | `	}` |
|        - |  3513 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3514 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3515 | `	 * bNullable=0 here. */` |
|      244 |  3516 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3517 | `		sxi32 rcU;` |
|      ! 0 |  3518 | `		int bNullable = 0;` |
|      ! 0 |  3519 | `		const char *zExpected = "union";` |
|        - |  3520 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3521 | `		{` |
|        - |  3522 | `			sxu32 i;` |
|      ! 0 |  3523 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3524 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3525 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3526 | `			}` |
|        - |  3527 | `		}` |
|      ! 0 |  3528 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3529 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3530 | `			return SXRET_OK;` |
|        - |  3531 | `		}` |
|      ! 0 |  3532 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3533 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3534 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3535 | `			zGiven = "null";` |
|      ! 0 |  3536 | `		}else{` |
|      ! 0 |  3537 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3538 | `		}` |
|      ! 0 |  3539 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3540 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3541 | `		}` |
|      ! 0 |  3542 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3543 | `	}` |
|        - |  3544 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3545 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3546 | `	 * it into the TypeError message. */` |
|      244 |  3547 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3548 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3549 | `		const char *zExpected;` |
|        - |  3550 | `		ph7_class *pExpected;` |
|        6 |  3551 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3552 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3553 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3554 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3555 | `		}` |
|        6 |  3556 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3557 | `			pExpected = pSelfNow;` |
|        4 |  3558 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3559 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3560 | `		}else{` |
|        3 |  3561 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3562 | `		}` |
|        6 |  3563 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3564 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3565 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3566 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3567 | `		}` |
|        6 |  3568 | `		if( pExpected ){` |
|        6 |  3569 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3570 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3571 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3572 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3573 | `			}` |
|        2 |  3574 | `		}` |
|        6 |  3575 | `		return SXRET_OK;` |
|        - |  3576 | `	}` |
|        - |  3577 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3578 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3579 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3580 | `	 * via the type-text leading '?'. */` |
|      240 |  3581 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3582 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3583 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3584 | `			return SXRET_OK;` |
|        - |  3585 | `		}` |
|      ! 0 |  3586 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3587 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3588 | `			"null");` |
|        - |  3589 | `	}` |
|        - |  3590 | `	/* Exact match? Done. */` |
|      234 |  3591 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3592 | `		return SXRET_OK;` |
|        - |  3593 | `	}` |
|        - |  3594 | `	/* Object->scalar is never compatible. */` |
|        8 |  3595 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3596 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3597 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3598 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3599 | `			zGiven);` |
|        - |  3600 | `	}` |
|        - |  3601 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3602 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3603 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3604 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3605 | `			ph7_type_name(pValue));` |
|        - |  3606 | `	}` |
|        - |  3607 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3608 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3609 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3610 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3611 | `	if( !bStrict` |
|        5 |  3612 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3613 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3614 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3615 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3616 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3617 | `			"string");` |
|        - |  3618 | `	}` |
|        6 |  3619 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3620 | `		return SXRET_OK;` |
|        - |  3621 | `	}` |
|        4 |  3622 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3623 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3624 | `		ph7_type_name(pValue));` |
|      200 |  3625 |  |
|        - |  3626 | `/*` |
|        - |  3627 | ` * Report a fatal named-argument error.` |
|        - |  3628 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3629 | ` */` |
|        6 |  3630 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3631 |  |
|        7 |  3632 | `	const char *zFunc = 0;` |
|        7 |  3633 | `	int nFunc = 0;` |
|        7 |  3634 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3635 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3636 |  |
|        - |  3637 | `/*` |
|        - |  3638 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3639 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3640 | ` * information.` |
|        - |  3641 | ` * ------------------------------------` |
|        - |  3642 | ` * Simple boring wrapper function.` |
|        - |  3643 | ` * ------------------------------------` |
|        - |  3644 | ` */` |
|       24 |  3645 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3646 |  |
|        - |  3647 | `	sxi32 rc;` |
|       26 |  3648 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3649 | `	return rc;` |
|        2 |  3650 |  |
|        - |  3651 | `/*` |
|        - |  3652 | ` * Resolve function context from the current frame.` |
|        - |  3653 | ` */` |
|     1018 |  3654 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3655 |  |
|        - |  3656 | `	VmFrame *pFrame;` |
|        - |  3657 | `	ph7_vm_func *pFunc;` |
|     1019 |  3658 | `	*pzFuncName = 0;` |
|     1019 |  3659 | `	*pnFuncLen = 0;` |
|     1019 |  3660 | `	pFrame = pVm->pFrame;` |
|     1019 |  3661 | `	if( pFrame == 0 ){` |
|      ! 0 |  3662 | `		return;` |
|        - |  3663 | `	}` |
|     1019 |  3664 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3665 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3666 | `		return;` |
|        - |  3667 | `	}` |
|       25 |  3668 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3669 | `	if( pFunc == 0 ){` |
|      ! 0 |  3670 | `		return;` |
|        - |  3671 | `	}` |
|       25 |  3672 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3673 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3674 |  |
|        - |  3675 | `/*` |
|        - |  3676 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3677 | ` */` |
|      524 |  3678 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3679 |  |
|        - |  3680 | `	SyBlob sOut;` |
|        - |  3681 | `	SyString *pFile;` |
|      525 |  3682 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3683 | `		return PH7_OK;` |
|        - |  3684 | `	}` |
|      525 |  3685 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3686 | `		zClass = "Exception";` |
|      ! 0 |  3687 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3688 | `	}` |
|      525 |  3689 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3690 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3691 | `	}` |
|      525 |  3692 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3693 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3694 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3695 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3696 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3697 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3698 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3699 | `	}` |
|      525 |  3700 | `	if( pFile ){` |
|      525 |  3701 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  3702 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3703 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  3704 | `	}` |
|      525 |  3705 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  3706 | `	if( pFile ){` |
|      525 |  3707 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  3708 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3709 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3710 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3711 | `		}else{` |
|      501 |  3712 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3713 | `		}` |
|      262 |  3714 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3715 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3716 | `	}else{` |
|      ! 0 |  3717 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3718 | `	}` |
|      525 |  3719 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  3720 | `	if( pFile ){` |
|      525 |  3721 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  3722 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  3723 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3724 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  3725 | `	}` |
|      525 |  3726 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  3727 | `	SyBlobRelease(&sOut);` |
|      525 |  3728 | `	return PH7_ABORT;` |
|      263 |  3729 |  |
|        - |  3730 | `/*` |
|        - |  3731 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  3732 | ` *` |
|        - |  3733 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  3734 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  3735 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  3736 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  3737 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  3738 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  3739 | ` */` |
|      868 |  3740 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  3741 |  |
|      870 |  3742 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  3743 | `		if( pVm->pCoalesceObj ){` |
|        7 |  3744 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  3745 | `		}` |
|        7 |  3746 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  3747 | `		pVm->pCoalesceObj = 0;` |
|        7 |  3748 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  3749 | `	}` |
|      870 |  3750 |  |
|        - |  3751 | `/*` |
|        - |  3752 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  3753 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  3754 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  3755 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  3756 | ` *` |
|        - |  3757 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  3758 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  3759 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  3760 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  3761 | ` */` |
|        4 |  3762 | `static sxi32 VmThrowFromVm(` |
|        - |  3763 | `	ph7_vm *pVm,` |
|        - |  3764 | `	const char *zClass,` |
|        - |  3765 | `	const char *zMsg,` |
|        - |  3766 | `	sxu32 nMsg` |
|        1 |  3767 | `){` |
|        - |  3768 | `	ph7_class *pClass;` |
|        - |  3769 | `	ph7_class_instance *pThis;` |
|        - |  3770 | `	ph7_class_method *pCons;` |
|        - |  3771 | `	VmFrame *pFrame;` |
|        - |  3772 | `	sxi32 rc;` |
|        5 |  3773 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  3774 | `	if( pClass == 0 ){` |
|      ! 0 |  3775 | `		return SXERR_ABORT;` |
|        - |  3776 | `	}` |
|        5 |  3777 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  3778 | `	if( pThis == 0 ){` |
|      ! 0 |  3779 | `		return SXERR_ABORT;` |
|        - |  3780 | `	}` |
|        5 |  3781 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3782 | `	if( pCons ){` |
|        - |  3783 | `		ph7_value sArg;` |
|        - |  3784 | `		ph7_value *apArg[1];` |
|        - |  3785 | `		SyString sMsgStr;` |
|        5 |  3786 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  3787 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  3788 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  3789 | `		apArg[0] = &sArg;` |
|        5 |  3790 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  3791 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3792 | `	}` |
|        5 |  3793 | `	pFrame = pVm->pFrame;` |
|        5 |  3794 | `	if( pFrame ){` |
|        5 |  3795 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3796 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3797 | `	}` |
|        5 |  3798 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  3799 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3800 | `	return rc;` |
|        3 |  3801 |  |
|        - |  3802 | `/*` |
|        - |  3803 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3804 | ` */` |
|      574 |  3805 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3806 |  |
|        - |  3807 | `	ph7_vm *pVm;` |
|        - |  3808 | `	ph7_class *pClass;` |
|        - |  3809 | `	ph7_class_instance *pThis;` |
|        - |  3810 | `	ph7_class_method *pCons;` |
|        - |  3811 | `	ph7_value sArg;` |
|        - |  3812 | `	ph7_value *apArg[1];` |
|        - |  3813 | `	SyBlob sMsg;` |
|        - |  3814 | `	SyString sMsgStr;` |
|        - |  3815 | `	VmFrame *pFrame;` |
|        - |  3816 | `	va_list ap;` |
|        - |  3817 | `	sxi32 rc;` |
|        - |  3818 |  |
|      576 |  3819 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3820 | `		return PH7_ABORT;` |
|        - |  3821 | `	}` |
|      576 |  3822 | `	pVm = pCtx->pVm;` |
|      576 |  3823 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3824 | `		zClass = "Error";` |
|      ! 0 |  3825 | `	}` |
|      576 |  3826 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  3827 | `	if( pClass == 0 ){` |
|      ! 0 |  3828 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3829 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3830 | `			zClass` |
|        - |  3831 | `			);` |
|        - |  3832 | `	}` |
|      576 |  3833 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  3834 | `	if( pThis == 0 ){` |
|      ! 0 |  3835 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3836 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3837 | `			);` |
|        - |  3838 | `	}` |
|        - |  3839 |  |
|      576 |  3840 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  3841 | `	va_start(ap,zFormat);` |
|      576 |  3842 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  3843 | `	va_end(ap);` |
|        - |  3844 |  |
|      576 |  3845 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  3846 | `	if( pCons ){` |
|      576 |  3847 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  3848 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  3849 | `		apArg[0] = &sArg;` |
|      576 |  3850 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  3851 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  3852 | `	}` |
|      576 |  3853 | `	SyBlobRelease(&sMsg);` |
|        - |  3854 |  |
|      576 |  3855 | `	pFrame = pVm->pFrame;` |
|      576 |  3856 | `	if( pFrame ){` |
|      576 |  3857 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  3858 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  3859 | `	}` |
|      576 |  3860 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  3861 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  3862 | `	if( rc == SXERR_ABORT ){` |
|      491 |  3863 | `		return PH7_ABORT;` |
|        - |  3864 | `	}` |
|       86 |  3865 | `	return PH7_EXCEPTION;` |
|      289 |  3866 |  |
|        - |  3867 | `/*` |
|        - |  3868 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3869 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3870 | ` */` |
|      ! 0 |  3871 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3872 |  |
|        - |  3873 | `	ph7_vm *pVm;` |
|        - |  3874 | `	SyBlob sMsg;` |
|      ! 0 |  3875 | `	const char *zFuncName = 0;` |
|      ! 0 |  3876 | `	int nFuncLen = 0;` |
|        - |  3877 | `	va_list ap;` |
|        - |  3878 | `	sxi32 rc;` |
|        - |  3879 |  |
|      ! 0 |  3880 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3881 | `		return PH7_OK;` |
|        - |  3882 | `	}` |
|      ! 0 |  3883 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3884 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3885 | `		zClass = "Error";` |
|      ! 0 |  3886 | `	}` |
|        - |  3887 |  |
|      ! 0 |  3888 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3889 |  |
|      ! 0 |  3890 | `	va_start(ap,zFormat);` |
|      ! 0 |  3891 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3892 | `	va_end(ap);` |
|        - |  3893 |  |
|      ! 0 |  3894 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3895 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3896 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3897 | `	}` |
|      ! 0 |  3898 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3899 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3900 | `	}` |
|      ! 0 |  3901 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3902 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3903 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3904 | `	return rc;` |
|      ! 0 |  3905 |  |
|        - |  3906 | `/*` |
|        - |  3907 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3908 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3909 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3910 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3911 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3912 | ` * when VmByteCodeExec returns.` |
|        - |  3913 | ` */` |
|      144 |  3914 | `static sxi32 VmSuspendCtx(` |
|        - |  3915 | `	ph7_vm *pVm,` |
|        - |  3916 | `	ph7_exec_ctx *pCtx,` |
|        - |  3917 | `	sxi32 pc,` |
|        - |  3918 | `	sxi32 nTos` |
|        - |  3919 | `	)` |
|        2 |  3920 |  |
|       72 |  3921 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3922 | `	pCtx->pc = pc;` |
|      146 |  3923 | `	pCtx->nTos = nTos;` |
|      146 |  3924 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3925 | `	return PH7_SUSPEND;` |
|        2 |  3926 |  |
|        - |  3927 | `/*` |
|        - |  3928 | ` * Resolve named-argument mapping.` |
|        - |  3929 | ` *` |
|        - |  3930 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3931 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3932 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3933 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3934 | ` * every formal parameter that received a value.` |
|        - |  3935 | ` *` |
|        - |  3936 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3937 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3938 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3939 | ` */` |
|       98 |  3940 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3941 | `	ph7_vm *pVm,` |
|        - |  3942 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3943 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3944 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3945 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3946 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3947 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3948 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3949 |  |
|        2 |  3950 |  |
|      100 |  3951 | `	sxi32 posIdx = 0;` |
|        - |  3952 | `	sxu32 i;` |
|        - |  3953 | `	char zErrMsg[256];` |
|      100 |  3954 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3955 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3956 | `		aSlot[i] = -2;` |
|      100 |  3957 | `	}` |
|      290 |  3958 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3959 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3960 | `			/* Named argument — find formal by name */` |
|      184 |  3961 | `			int found = 0;` |
|        - |  3962 | `			sxu32 k;` |
|      304 |  3963 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3964 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3965 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3966 | `						pMap->aNames[i].zString,` |
|      402 |  3967 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3968 | `					if( aUsed[k] ){` |
|        7 |  3969 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3970 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3971 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3972 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3973 | `						return PH7_ABORT;` |
|        - |  3974 | `					}` |
|      168 |  3975 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3976 | `					aUsed[k] = 1;` |
|      168 |  3977 | `					found = 1;` |
|      168 |  3978 | `					break;` |
|        - |  3979 | `				}` |
|       62 |  3980 | `			}` |
|      180 |  3981 | `			if( !found ){` |
|       14 |  3982 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3983 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3984 | `				}else{` |
|        4 |  3985 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3986 | `						"Unknown named parameter $%.*s",` |
|        2 |  3987 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3988 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3989 | `					return PH7_ABORT;` |
|        - |  3990 | `				}` |
|        5 |  3991 | `			}` |
|       90 |  3992 | `		}else{` |
|        - |  3993 | `			/* Positional argument */` |
|       16 |  3994 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3995 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3996 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3997 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3998 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3999 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4000 | `					return PH7_ABORT;` |
|        - |  4001 | `				}` |
|       16 |  4002 | `				aSlot[i] = posIdx;` |
|       16 |  4003 | `				aUsed[posIdx] = 1;` |
|        7 |  4004 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4005 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4006 | `			}` |
|       16 |  4007 | `			posIdx++;` |
|        - |  4008 | `		}` |
|       97 |  4009 | `	}` |
|       93 |  4010 | `	return SXRET_OK;` |
|       51 |  4011 |  |
|        - |  4012 | `/*` |
|        - |  4013 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4014 | ` *` |
|        - |  4015 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4016 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4017 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4018 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4019 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4020 | ` * then the program execution is halted.` |
|        - |  4021 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4022 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4023 | ` * or to reset the VM to it's initial state.` |
|        - |  4024 | ` */` |
|    45028 |  4025 | `static sxi32 VmByteCodeExec(` |
|        - |  4026 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4027 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4028 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4029 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4030 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4031 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4032 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4033 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4034 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4035 | `	)` |
|        2 |  4036 |  |
|        - |  4037 | `	VmInstr *pInstr;` |
|        - |  4038 | `	ph7_value *pTos;` |
|        - |  4039 | `	SySet aArg;` |
|        - |  4040 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4041 | `	sxi32 pc;` |
|        - |  4042 | `	sxi32 rc;` |
|        - |  4043 | `	/* Argument container */` |
|    45030 |  4044 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    45030 |  4045 | `	if( nTos < 0 ){` |
|    41856 |  4046 | `		pTos = &pStack[-1];` |
|    20929 |  4047 | `	}else{` |
|     3176 |  4048 | `		pTos = &pStack[nTos];` |
|        - |  4049 | `	}` |
|    45030 |  4050 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    45030 |  4051 | `	pc = nPc;` |
|        - |  4052 | `/*` |
|        - |  4053 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4054 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4055 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4056 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4057 | ` */` |
|        - |  4058 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4059 | `	{ \` |
|        - |  4060 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4061 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4062 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4063 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4064 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4065 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4066 | `				break; \` |
|        - |  4067 | `			} \` |
|        - |  4068 | `			goto Exception; \` |
|        - |  4069 | `		} \` |
|        - |  4070 | `	}` |
|        - |  4071 | `	/* Execute as much as we can */` |
|  5902215 |  4072 | `	for(;;){` |
|        - |  4073 | `		/* Fetch the instruction to execute */` |
| 11803728 |  4074 | `		pInstr = &aInstr[pc];` |
| 11803728 |  4075 | `		rc = SXRET_OK;` |
|        - |  4076 | `/*` |
|        - |  4077 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4078 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4079 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4080 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4081 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4082 | ` */` |
| 11803728 |  4083 | `		switch(pInstr->iOp){` |
|        - |  4084 | `/*` |
|        - |  4085 | ` * DONE: P1 * *` |
|        - |  4086 | ` *` |
|        - |  4087 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4088 | ` * and return immediately.` |
|        - |  4089 | ` */` |
|    22136 |  4090 | `case PH7_OP_DONE:` |
|        - |  4091 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4092 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4093 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4094 | `	 * callback trampolines, and the main script. */` |
|    44272 |  4095 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      402 |  4096 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4097 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4098 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4099 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4100 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4101 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4102 | `		 * exception. */` |
|      398 |  4103 | `		ph7_value *pRetVal = 0;` |
|      398 |  4104 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      266 |  4105 | `			pRetVal = pTos;` |
|      132 |  4106 | `		}` |
|      398 |  4107 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      398 |  4108 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      392 |  4109 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4110 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4111 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4112 | `				pTos--;` |
|      ! 0 |  4113 | `			}` |
|      ! 0 |  4114 | `			goto Exception;` |
|        - |  4115 | `		}` |
|        - |  4116 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4117 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4118 | `		 * defensively we clear the pointer after a successful check). */` |
|      392 |  4119 | `		pEnforceRetFunc = 0;` |
|      195 |  4120 | `	}` |
|    44268 |  4121 | `	if( pInstr->iP1 ){` |
|        - |  4122 | `#ifdef UNTRUST` |
|        - |  4123 | `		if( pTos < pStack ){` |
|        - |  4124 | `			goto Abort;` |
|        - |  4125 | `		}` |
|        - |  4126 | `#endif` |
|    26930 |  4127 | `		if( pLastRef ){` |
|    16396 |  4128 | `			*pLastRef = pTos->nIdx;` |
|     8197 |  4129 | `		}` |
|    26930 |  4130 | `		if( pResult ){` |
|        - |  4131 | `			/* Execution result */` |
|    25430 |  4132 | `			PH7_MemObjStore(pTos,pResult);` |
|    12714 |  4133 | `		}` |
|    26930 |  4134 | `		VmPopOperand(&pTos,1);` |
|    30804 |  4135 | `	}else if( pLastRef ){` |
|        - |  4136 | `		/* Nothing referenced */` |
|     1960 |  4137 | `		*pLastRef = SXU32_HIGH;` |
|      979 |  4138 | `	}` |
|        - |  4139 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4140 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4141 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4142 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4143 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4144 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4145 | `	 * block can override it.` |
|        - |  4146 | `	 */` |
|    44270 |  4147 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4148 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4149 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4150 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4151 | `		pExc->pFrame = 0;` |
|        3 |  4152 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4153 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4154 | `			pExc->iFinallyDone = 1;` |
|        - |  4155 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4156 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4157 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4158 | `				goto Abort;` |
|        - |  4159 | `			}` |
|        1 |  4160 | `		}` |
|        1 |  4161 | `	}` |
|    44268 |  4162 | `	goto Done;` |
|        - |  4163 | `/*` |
|        - |  4164 | ` * HALT: P1 * *` |
|        - |  4165 | ` *` |
|        - |  4166 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4167 | ` * and abort immediately.` |
|        - |  4168 | ` */` |
|        7 |  4169 | `case PH7_OP_HALT:` |
|       15 |  4170 | `	if( pInstr->iP1 ){` |
|        - |  4171 | `#ifdef UNTRUST` |
|        - |  4172 | `		if( pTos < pStack ){` |
|        - |  4173 | `			goto Abort;` |
|        - |  4174 | `		}` |
|        - |  4175 | `#endif` |
|       15 |  4176 | `		if( pLastRef ){` |
|        3 |  4177 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4178 | `		}` |
|       15 |  4179 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4180 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4181 | `				/* Output the exit message */` |
|       16 |  4182 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4183 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4184 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4185 | `			}` |
|       10 |  4186 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4187 | `			/* Record exit status */` |
|        5 |  4188 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4189 | `		}` |
|       15 |  4190 | `		VmPopOperand(&pTos,1);` |
|        7 |  4191 | `	}else if( pLastRef ){` |
|        - |  4192 | `		/* Nothing referenced */` |
|      ! 0 |  4193 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4194 | `	}` |
|        - |  4195 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4196 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4197 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4198 | `	 */` |
|       15 |  4199 | `	pVm->bHaltRequested = 1;` |
|       15 |  4200 | `	goto Abort;` |
|        - |  4201 | `/*` |
|        - |  4202 | ` * JMP: * P2 *` |
|        - |  4203 | ` *` |
|        - |  4204 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4205 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4206 | ` */` |
|   251448 |  4207 | `case PH7_OP_JMP:` |
|   502942 |  4208 | `	pc = pInstr->iP2 - 1;` |
|   502942 |  4209 | `	break;` |
|        - |  4210 | `/*` |
|        - |  4211 | ` * JZ: P1 P2 *` |
|        - |  4212 | ` *` |
|        - |  4213 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4214 | ` * entry in the stack if P1 is zero.` |
|        - |  4215 | ` */` |
|   596987 |  4216 | `case PH7_OP_JZ:` |
|        - |  4217 | `#ifdef UNTRUST` |
|        - |  4218 | `	if( pTos < pStack ){` |
|        - |  4219 | `		goto Abort;` |
|        - |  4220 | `	}` |
|        - |  4221 | `#endif` |
|        - |  4222 | `	/* Get a boolean value */` |
|  1194064 |  4223 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4224 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4225 | `	}` |
|  1194064 |  4226 | `	if( !pTos->x.iVal ){` |
|        - |  4227 | `		/* Take the jump */` |
|   614362 |  4228 | `		pc = pInstr->iP2 - 1;` |
|   307180 |  4229 | `	}` |
|  1194064 |  4230 | `	if( !pInstr->iP1 ){` |
|   946206 |  4231 | `		VmPopOperand(&pTos,1);` |
|   473124 |  4232 | `	}` |
|  1194064 |  4233 | `	break;` |
|        - |  4234 | `/*` |
|        - |  4235 | ` * JNZ: P1 P2 *` |
|        - |  4236 | ` *` |
|        - |  4237 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4238 | ` * entry in the stack if P1 is zero.` |
|        - |  4239 | ` */` |
|    61364 |  4240 | `case PH7_OP_JNZ:` |
|        - |  4241 | `#ifdef UNTRUST` |
|        - |  4242 | `	if( pTos < pStack ){` |
|        - |  4243 | `		goto Abort;` |
|        - |  4244 | `	}` |
|        - |  4245 | `#endif` |
|        - |  4246 | `	/* Get a boolean value */` |
|   122730 |  4247 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4248 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4249 | `	}` |
|   122730 |  4250 | `	if( pTos->x.iVal ){` |
|        - |  4251 | `		/* Take the jump */` |
|     5594 |  4252 | `		pc = pInstr->iP2 - 1;` |
|     2796 |  4253 | `	}` |
|   122730 |  4254 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4255 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4256 | `	}` |
|   122730 |  4257 | `	break;` |
|        - |  4258 | `/*` |
|        - |  4259 | ` * NOOP: * * *` |
|        - |  4260 | ` *` |
|        - |  4261 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4262 | ` * destination.` |
|        - |  4263 | ` */` |
|      ! 0 |  4264 | `case PH7_OP_NOOP:` |
|      ! 0 |  4265 | `	break;` |
|        - |  4266 | `/*` |
|        - |  4267 | ` * POP: P1 * *` |
|        - |  4268 | ` *` |
|        - |  4269 | ` * Pop P1 elements from the operand stack.` |
|        - |  4270 | ` */` |
|   462773 |  4271 | `case PH7_OP_POP: {` |
|   925592 |  4272 | `	sxi32 n = pInstr->iP1;` |
|   925592 |  4273 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4274 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4275 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4276 | `	}` |
|   925592 |  4277 | `	VmPopOperand(&pTos,n);` |
|   925592 |  4278 | `	break;` |
|        - |  4279 | `				 }` |
|        - |  4280 | `/*` |
|        - |  4281 | ` * DUP: * * *` |
|        - |  4282 | ` *` |
|        - |  4283 | ` * Duplicate the top of the stack.` |
|        - |  4284 | ` */` |
|       41 |  4285 | `case PH7_OP_DUP:` |
|        - |  4286 | `#ifdef UNTRUST` |
|        - |  4287 | `	if( pTos < pStack ){` |
|        - |  4288 | `		goto Abort;` |
|        - |  4289 | `	}` |
|        - |  4290 | `#endif` |
|       84 |  4291 | `	pTos++;` |
|       84 |  4292 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4293 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4294 | `	break;` |
|        - |  4295 | `/*` |
|        - |  4296 | ` * NSSWITCH: * * P3` |
|        - |  4297 | ` *` |
|        - |  4298 | ` * Switch the active namespace at runtime.` |
|        - |  4299 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4300 | ` */` |
|     7818 |  4301 | `case PH7_OP_NSSWITCH:` |
|    15638 |  4302 | `	SyBlobReset(&pVm->sNamespace);` |
|    15638 |  4303 | `	if( pInstr->p3 ){` |
|      100 |  4304 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4305 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4306 | `	}` |
|        - |  4307 | `	/* Clear namespace-scoped use-const imports */` |
|    15638 |  4308 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15638 |  4309 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15638 |  4310 | `	break;` |
|        - |  4311 | `/* OP_USECONST P1 * P3` |
|        - |  4312 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4313 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4314 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4315 | ` */` |
|        7 |  4316 | `case PH7_OP_USECONST: {` |
|       16 |  4317 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4318 | `	if( azPair ){` |
|       16 |  4319 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4320 | `	}` |
|       16 |  4321 | `	break;` |
|        - |  4322 | `				}` |
|        - |  4323 | `/*` |
|        - |  4324 | ` * CVT_INT: * * *` |
|        - |  4325 | ` *` |
|        - |  4326 | ` * Force the top of the stack to be an integer.` |
|        - |  4327 | ` */` |
|       80 |  4328 | `case PH7_OP_CVT_INT:` |
|        - |  4329 | `#ifdef UNTRUST` |
|        - |  4330 | `	if( pTos < pStack ){` |
|        - |  4331 | `		goto Abort;` |
|        - |  4332 | `	}` |
|        - |  4333 | `#endif` |
|      162 |  4334 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4335 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4336 | `	}` |
|        - |  4337 | `	/* Invalidate any prior representation */` |
|      162 |  4338 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4339 | `	break;` |
|        - |  4340 | `/*` |
|        - |  4341 | ` * CVT_REAL: * * *` |
|        - |  4342 | ` *` |
|        - |  4343 | ` * Force the top of the stack to be a real.` |
|        - |  4344 | ` */` |
|        5 |  4345 | `case PH7_OP_CVT_REAL:` |
|        - |  4346 | `#ifdef UNTRUST` |
|        - |  4347 | `	if( pTos < pStack ){` |
|        - |  4348 | `		goto Abort;` |
|        - |  4349 | `	}` |
|        - |  4350 | `#endif` |
|       11 |  4351 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4352 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4353 | `	}` |
|        - |  4354 | `	/* Invalidate any prior representation */` |
|       11 |  4355 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4356 | `	break;` |
|        - |  4357 | `/*` |
|        - |  4358 | ` * CVT_STR: * * *` |
|        - |  4359 | ` *` |
|        - |  4360 | ` * Force the top of the stack to be a string.` |
|        - |  4361 | ` */` |
|      163 |  4362 | `case PH7_OP_CVT_STR:` |
|        - |  4363 | `#ifdef UNTRUST` |
|        - |  4364 | `	if( pTos < pStack ){` |
|        - |  4365 | `		goto Abort;` |
|        - |  4366 | `	}` |
|        - |  4367 | `#endif` |
|      328 |  4368 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4369 | `		PH7_MemObjToString(pTos);` |
|      153 |  4370 | `	}` |
|      328 |  4371 | `	break;` |
|        - |  4372 | `/*` |
|        - |  4373 | ` * CVT_BOOL: * * *` |
|        - |  4374 | ` *` |
|        - |  4375 | ` * Force the top of the stack to be a boolean.` |
|        - |  4376 | ` */` |
|        5 |  4377 | `case PH7_OP_CVT_BOOL:` |
|        - |  4378 | `#ifdef UNTRUST` |
|        - |  4379 | `	if( pTos < pStack ){` |
|        - |  4380 | `		goto Abort;` |
|        - |  4381 | `	}` |
|        - |  4382 | `#endif` |
|       11 |  4383 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4384 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4385 | `	}` |
|       11 |  4386 | `	break;` |
|        - |  4387 | `/*` |
|        - |  4388 | ` * CVT_NULL: * * *` |
|        - |  4389 | ` *` |
|        - |  4390 | ` * Nullify the top of the stack.` |
|        - |  4391 | ` */` |
|        3 |  4392 | `case PH7_OP_CVT_NULL:` |
|        - |  4393 | `#ifdef UNTRUST` |
|        - |  4394 | `	if( pTos < pStack ){` |
|        - |  4395 | `		goto Abort;` |
|        - |  4396 | `	}` |
|        - |  4397 | `#endif` |
|        7 |  4398 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4399 | `	break;` |
|        - |  4400 | `/*` |
|        - |  4401 | ` * CVT_NUMC: * * *` |
|        - |  4402 | ` *` |
|        - |  4403 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4404 | ` */` |
|      ! 0 |  4405 | `case PH7_OP_CVT_NUMC:` |
|        - |  4406 | `#ifdef UNTRUST` |
|        - |  4407 | `	if( pTos < pStack ){` |
|        - |  4408 | `		goto Abort;` |
|        - |  4409 | `	}` |
|        - |  4410 | `#endif` |
|        - |  4411 | `	/* Force a numeric cast */` |
|      ! 0 |  4412 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4413 | `	break;` |
|        - |  4414 | `/*` |
|        - |  4415 | ` * CVT_ARRAY: * * *` |
|        - |  4416 | ` *` |
|        - |  4417 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4418 | ` */` |
|       10 |  4419 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4420 | `#ifdef UNTRUST` |
|        - |  4421 | `	if( pTos < pStack ){` |
|        - |  4422 | `		goto Abort;` |
|        - |  4423 | `	}` |
|        - |  4424 | `#endif` |
|        - |  4425 | `	/* Force a hashmap cast */` |
|       21 |  4426 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4427 | `	if( rc != SXRET_OK ){` |
|        - |  4428 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4429 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4430 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4431 | `	}` |
|       21 |  4432 | `	break;` |
|        - |  4433 | `/*` |
|        - |  4434 | ` * CVT_OBJ: * * *` |
|        - |  4435 | ` *` |
|        - |  4436 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4437 | ` */` |
|        8 |  4438 | `case PH7_OP_CVT_OBJ:` |
|        - |  4439 | `#ifdef UNTRUST` |
|        - |  4440 | `	if( pTos < pStack ){` |
|        - |  4441 | `		goto Abort;` |
|        - |  4442 | `	}` |
|        - |  4443 | `#endif` |
|       17 |  4444 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4445 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4446 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4447 | `	}` |
|       17 |  4448 | `	break;` |
|        - |  4449 | `/*` |
|        - |  4450 | ` * ERR_CTRL * * *` |
|        - |  4451 | ` *` |
|        - |  4452 | ` * Error control operator.` |
|        - |  4453 | ` */` |
|    16056 |  4454 | `case PH7_OP_ERR_CTRL:` |
|        - |  4455 | `	/*` |
|        - |  4456 | `	 * TICKET 1433-038:` |
|        - |  4457 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4458 | `	 * use the public API,to control error output.` |
|        - |  4459 | `	 */` |
|    32112 |  4460 | `	break;` |
|        - |  4461 | `/*` |
|        - |  4462 | ` * IS_A * * *` |
|        - |  4463 | ` *` |
|        - |  4464 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4465 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4466 | ` * holding a class name or an object).` |
|        - |  4467 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4468 | ` */` |
|       75 |  4469 | `case PH7_OP_IS_A:{` |
|      152 |  4470 | `	ph7_value *pNos = &pTos[-1];` |
|      152 |  4471 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4472 | `#ifdef UNTRUST` |
|        - |  4473 | `	if( pNos < pStack ){` |
|        - |  4474 | `		goto Abort;` |
|        - |  4475 | `	}` |
|        - |  4476 | `#endif` |
|      152 |  4477 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      150 |  4478 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      150 |  4479 | `		ph7_class *pClass = 0;` |
|        - |  4480 | `		/* Extract the target class */` |
|      150 |  4481 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4482 | `			/* Instance already loaded */` |
|      ! 0 |  4483 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      150 |  4484 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      150 |  4485 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      150 |  4486 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4487 | `			/* Handle self/static/parent keywords */` |
|      150 |  4488 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4489 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      148 |  4490 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4491 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      147 |  4492 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4493 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4494 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4495 | `					pClass = pSelf->pBase;` |
|        2 |  4496 | `				}` |
|        3 |  4497 | `			}else{` |
|      140 |  4498 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4499 | `			}` |
|       74 |  4500 | `		}` |
|      150 |  4501 | `		if( pClass ){` |
|        - |  4502 | `			/* Perform the query */` |
|      150 |  4503 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       74 |  4504 | `		}` |
|       74 |  4505 | `	}` |
|        - |  4506 | `	/* Push result */` |
|      152 |  4507 | `	VmPopOperand(&pTos,1);` |
|      152 |  4508 | `	PH7_MemObjRelease(pTos);` |
|      152 |  4509 | `	pTos->x.iVal = iRes;` |
|      152 |  4510 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      152 |  4511 | `	break;` |
|        - |  4512 | `				 }` |
|        - |  4513 |  |
|        - |  4514 | `/*` |
|        - |  4515 | ` * LOADC P1 P2 *` |
|        - |  4516 | ` *` |
|        - |  4517 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4518 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4519 | ` */` |
|  1015240 |  4520 | `case PH7_OP_LOADC: {` |
|        - |  4521 | `	ph7_value *pObj;` |
|        - |  4522 | `	/* Reserve a room */` |
|  2030526 |  4523 | `	pTos++;` |
|  3035941 |  4524 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2030526 |  4525 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4526 | `			SyHashEntry *pEntry;` |
|        - |  4527 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4528 | `			{` |
|        - |  4529 | `				SyHashEntry *pConstImport;` |
|    29615 |  4530 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19742 |  4531 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19744 |  4532 | `				if( pConstImport ){` |
|       11 |  4533 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4534 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4535 | `					if( pEntry ){` |
|       11 |  4536 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4537 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4538 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4539 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4540 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4541 | `						break;` |
|        - |  4542 | `					}` |
|        - |  4543 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4544 | `				}` |
|        - |  4545 | `			}` |
|        - |  4546 | `			/* Candidate for expansion via user defined callbacks */` |
|    19734 |  4547 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19734 |  4548 | `			if( pEntry ){` |
|    19728 |  4549 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4550 | `				/* Set a NULL default value */` |
|    19728 |  4551 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19728 |  4552 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4553 | `				/* Invoke the callback and deal with the expanded value */` |
|    19728 |  4554 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4555 | `				/* Mark as constant */` |
|    19728 |  4556 | `				pTos->nIdx = SXU32_HIGH;` |
|    19728 |  4557 | `				break;` |
|        - |  4558 | `			}` |
|        - |  4559 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4560 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4561 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4562 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4563 | `			{` |
|        8 |  4564 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4565 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4566 | `				sxu32 j;` |
|        8 |  4567 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4568 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4569 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4570 | `				}` |
|        8 |  4571 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4572 | `					/* Try current_namespace\name */` |
|      ! 0 |  4573 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4574 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4575 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4576 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4577 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4578 | `					if( pEntry ){` |
|      ! 0 |  4579 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4580 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4581 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4582 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4583 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4584 | `						break;` |
|        - |  4585 | `					}` |
|        - |  4586 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4587 | `				}` |
|        8 |  4588 | `				if( isQualified ){` |
|        - |  4589 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4590 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4591 | `					SyBlob sErr;` |
|        3 |  4592 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4593 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4594 | `					if( pErrFile ){` |
|        3 |  4595 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4596 | `					}` |
|        3 |  4597 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4598 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4599 | `					SyBlobRelease(&sErr);` |
|        3 |  4600 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4601 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4602 | `					goto LoadC_Done;` |
|        - |  4603 | `				}` |
|        - |  4604 | `			}` |
|        2 |  4605 | `		}` |
|  2010788 |  4606 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1005417 |  4607 | `	}else{` |
|        - |  4608 | `		/* Set a NULL value */` |
|      ! 0 |  4609 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4610 | `	}` |
|  1005372 |  4611 | `LoadC_Done:` |
|        - |  4612 | `	/* Mark as constant */` |
|  2010790 |  4613 | `	pTos->nIdx = SXU32_HIGH;` |
|  2010790 |  4614 | `	break;` |
|        - |  4615 | `				  }` |
|        - |  4616 | `/*` |
|        - |  4617 | ` * LOAD: P1 * P3` |
|        - |  4618 | ` *` |
|        - |  4619 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4620 | ` * from the P3 operand.` |
|        - |  4621 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4622 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4623 | ` */` |
|  1576065 |  4624 | `case PH7_OP_LOAD:{` |
|        - |  4625 | `	ph7_value *pObj;` |
|        - |  4626 | `	SyString sName;` |
|  3152352 |  4627 | `	if( pInstr->p3 == 0 ){` |
|        - |  4628 | `		/* Take the variable name from the top of the stack */` |
|        - |  4629 | `#ifdef UNTRUST` |
|        - |  4630 | `		if( pTos < pStack ){` |
|        - |  4631 | `			goto Abort;` |
|        - |  4632 | `		}` |
|        - |  4633 | `#endif` |
|        - |  4634 | `		/* Force a string cast */` |
|       19 |  4635 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4636 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4637 | `		}` |
|       19 |  4638 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4639 | `	}else{` |
|  3152334 |  4640 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4641 | `		/* Reserve a room for the target object */` |
|  3152334 |  4642 | `		pTos++;` |
|        - |  4643 | `	}` |
|        - |  4644 | `	/* Extract the requested memory object */` |
|  3152352 |  4645 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3152352 |  4646 | `	if( pObj == 0 ){` |
|      836 |  4647 | `		if( pInstr->iP1 ){` |
|        - |  4648 | `			/* Variable not found,load NULL */` |
|      836 |  4649 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4650 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4651 | `			}else{` |
|      836 |  4652 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4653 | `			}` |
|      836 |  4654 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1576484 |  4655 | `			break;` |
|      ! 0 |  4656 | `		}else{` |
|        - |  4657 | `			/* Fatal error */` |
|      ! 0 |  4658 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4659 | `			goto Abort;` |
|        - |  4660 | `		}` |
|        - |  4661 | `	}` |
|        - |  4662 | `	/* Load variable contents */` |
|  3151518 |  4663 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3151518 |  4664 | `	pTos->nIdx = pObj->nIdx;` |
|  3151518 |  4665 | `	break;` |
|        - |  4666 | `				   }` |
|        - |  4667 | `/*` |
|        - |  4668 | ` * LOAD_MAP P1 * *` |
|        - |  4669 | ` *` |
|        - |  4670 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4671 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4672 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4673 | ` */` |
|    22802 |  4674 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4675 | `	ph7_hashmap *pMap;` |
|        - |  4676 | `	/* Allocate a new hashmap instance */` |
|    45606 |  4677 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45606 |  4678 | `	if( pMap == 0 ){` |
|      ! 0 |  4679 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4680 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4681 | `		goto Abort;` |
|        - |  4682 | `	}` |
|    45606 |  4683 | `	if( pInstr->iP1 > 0 ){` |
|     2784 |  4684 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2784 |  4685 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4686 | `		/* Perform the insertion */` |
|     8502 |  4687 | `		while( pEntry < pTos ){` |
|     5736 |  4688 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4689 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4690 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4691 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  4692 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  4693 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  4694 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4695 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4696 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4697 | `						 * map dangling. */` |
|      ! 0 |  4698 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4699 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4700 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4701 | `						break;` |
|        - |  4702 | `					}` |
|       27 |  4703 | `				}else{` |
|        - |  4704 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  4705 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  4706 | `					break;` |
|        1 |  4707 | `				}` |
|     5694 |  4708 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4709 | `				/* Insertion by reference */` |
|      151 |  4710 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  4711 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  4712 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4713 | `					);` |
|       51 |  4714 | `			}else{` |
|        - |  4715 | `				/* Standard insertion */` |
|     8351 |  4716 | `				PH7_HashmapInsert(pMap,` |
|     5566 |  4717 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2783 |  4718 | `					&pEntry[1]` |
|        - |  4719 | `				);` |
|        - |  4720 | `			}` |
|        - |  4721 | `			/* Next pair on the stack */` |
|     5720 |  4722 | `			pEntry += 2;` |
|        2 |  4723 | `		}` |
|        - |  4724 | `		/* Pop P1 elements */` |
|     2784 |  4725 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2784 |  4726 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4727 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  4728 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  4729 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4730 | `				goto Abort;` |
|        - |  4731 | `			}` |
|        - |  4732 | `			{` |
|       17 |  4733 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  4734 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  4735 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  4736 | `					break;` |
|        - |  4737 | `				}` |
|        - |  4738 | `			}` |
|       15 |  4739 | `			goto Exception;` |
|        - |  4740 | `		}` |
|     1383 |  4741 | `	}` |
|        - |  4742 | `	/* Push the hashmap */` |
|    45590 |  4743 | `	pTos++;` |
|    45590 |  4744 | `	pTos->nIdx = SXU32_HIGH;` |
|    45590 |  4745 | `	pTos->x.pOther = pMap;` |
|    45590 |  4746 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45590 |  4747 | `	break;` |
|        - |  4748 | `					  }` |
|        - |  4749 | `/*` |
|        - |  4750 | ` * LOAD_LIST: P1 * *` |
|        - |  4751 | ` *` |
|        - |  4752 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4753 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4754 | ` * Caveats:` |
|        - |  4755 | ` *  This implementation support only a single nesting level.` |
|        - |  4756 | ` */` |
|       48 |  4757 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4758 | `	ph7_value *pEntry;` |
|       98 |  4759 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4760 | `		/* Empty list,break immediately */` |
|      ! 0 |  4761 | `		break;` |
|        - |  4762 | `	}` |
|       98 |  4763 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4764 | `#ifdef UNTRUST` |
|        - |  4765 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4766 | `		goto Abort;` |
|        - |  4767 | `	}` |
|        - |  4768 | `#endif` |
|       98 |  4769 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4770 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4771 | `		ph7_hashmap_node *pNode;` |
|        - |  4772 | `		ph7_value sKey,*pObj;` |
|        - |  4773 | `		/* Start Copying */` |
|       91 |  4774 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4775 | `		while( pEntry <= pTos ){` |
|      193 |  4776 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4777 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4778 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4779 | `					if( rc == SXRET_OK ){` |
|        - |  4780 | `						/* Store node value */` |
|      165 |  4781 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4782 | `					}else{` |
|        - |  4783 | `						/* Undefined array key */` |
|        - |  4784 | `						char zMsg[128];` |
|      ! 0 |  4785 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4786 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4787 | `						PH7_MemObjRelease(pObj);` |
|        - |  4788 | `					}` |
|       82 |  4789 | `				}` |
|       82 |  4790 | `			}` |
|      193 |  4791 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4792 | `			pEntry++;` |
|        1 |  4793 | `		}` |
|       46 |  4794 | `	}else{` |
|        - |  4795 | `		/* Source is not an array */` |
|        - |  4796 | `		ph7_value *pObj;` |
|       18 |  4797 | `		while( pEntry <= pTos ){` |
|       12 |  4798 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4799 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4800 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4801 | `				}` |
|        5 |  4802 | `			}` |
|       12 |  4803 | `			pEntry++;` |
|        2 |  4804 | `		}` |
|        8 |  4805 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4806 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4807 | `			const char *zType = "unknown";` |
|        3 |  4808 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4809 | `			char zMsg[256];` |
|        3 |  4810 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4811 | `				zType = "string";` |
|        1 |  4812 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4813 | `				zType = "int";` |
|      ! 0 |  4814 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4815 | `				zType = "float";` |
|      ! 0 |  4816 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4817 | `				zType = "object";` |
|      ! 0 |  4818 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4819 | `				zType = "resource";` |
|      ! 0 |  4820 | `			}` |
|        3 |  4821 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4822 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4823 | `		}` |
|        - |  4824 | `	}` |
|       98 |  4825 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4826 | `	break;` |
|        - |  4827 | `					   }` |
|        - |  4828 | `/*` |
|        - |  4829 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4830 | ` *` |
|        - |  4831 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4832 | ` * from the stack.` |
|        - |  4833 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4834 | ` * instead.` |
|        - |  4835 | ` */` |
|   250693 |  4836 | `case PH7_OP_LOAD_IDX: {` |
|   501432 |  4837 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   501432 |  4838 | `	ph7_hashmap *pMap = 0;` |
|        - |  4839 | `	ph7_value *pIdx;` |
|   501432 |  4840 | `	pIdx = 0;` |
|   501432 |  4841 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4842 | `		if( !pInstr->iP2){` |
|        - |  4843 | `			/* No available index,load NULL */` |
|      ! 0 |  4844 | `			if( pTos >= pStack ){` |
|      ! 0 |  4845 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4846 | `			}else{` |
|        - |  4847 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4848 | `				pTos++;` |
|      ! 0 |  4849 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4850 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4851 | `			}` |
|        - |  4852 | `			/* Emit a notice */` |
|      ! 0 |  4853 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4854 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4855 | `			break;` |
|        - |  4856 | `		}` |
|      ! 0 |  4857 | `	}else{` |
|   501432 |  4858 | `		pIdx = pTos;` |
|   501432 |  4859 | `		pTos--;` |
|        - |  4860 | `	}` |
|   501432 |  4861 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4862 | `		/* String access */` |
|   387716 |  4863 | `		if( pIdx ){` |
|        - |  4864 | `			sxu32 nOfft;` |
|   387716 |  4865 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4866 | `				/* Force an int cast */` |
|      ! 0 |  4867 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4868 | `			}` |
|   387716 |  4869 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  4870 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4871 | `				/* Invalid offset,load null */` |
|      ! 0 |  4872 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4873 | `			}else{` |
|   387716 |  4874 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  4875 | `				int c = zData[nOfft];` |
|   387716 |  4876 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  4877 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  4878 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4879 | `			}` |
|   193881 |  4880 | `		}else{` |
|        - |  4881 | `			/* No available index,load NULL */` |
|      ! 0 |  4882 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4883 | `		}` |
|   387716 |  4884 | `		break;` |
|        - |  4885 | `	}` |
|   113718 |  4886 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4887 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  4888 | `		 * iP2 codes:` |
|        - |  4889 | `		 *   0 = read       → offsetGet` |
|        - |  4890 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  4891 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  4892 | `		 *   4 = isset()    → offsetExists` |
|        - |  4893 | `		 *   5 = unset()    → offsetUnset` |
|        - |  4894 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  4895 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  4896 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  4897 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  4898 | `			ph7_class_method *pMeth;` |
|        - |  4899 | `			ph7_value sResult;` |
|        - |  4900 | `			ph7_value *apArg[1];` |
|      124 |  4901 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  4902 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  4903 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4904 | `					"Cannot use [] for reading");` |
|      ! 0 |  4905 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4906 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4907 | `				break;` |
|        - |  4908 | `			}` |
|      124 |  4909 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  4910 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  4911 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  4912 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4913 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  4914 | `				apArg[0] = pIdx;` |
|       51 |  4915 | `				if( pMeth ){` |
|       51 |  4916 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  4917 | `				}` |
|       99 |  4918 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  4919 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4920 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  4921 | `				apArg[0] = pIdx;` |
|        9 |  4922 | `				if( pMeth ){` |
|        9 |  4923 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  4924 | `				}` |
|        5 |  4925 | `			}else{` |
|       66 |  4926 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4927 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  4928 | `				apArg[0] = pIdx;` |
|       66 |  4929 | `				if( pMeth ){` |
|       66 |  4930 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  4931 | `				}` |
|        - |  4932 | `			}` |
|      124 |  4933 | `			if( pInstr->iP2 == 4 ){` |
|        - |  4934 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  4935 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  4936 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  4937 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  4938 | `				PH7_MemObjRelease(pTos);` |
|       33 |  4939 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  4940 | `				if( bExists ){` |
|       17 |  4941 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  4942 | `					pTos->x.iVal = 1;` |
|        9 |  4943 | `				}else{` |
|       17 |  4944 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  4945 | `				}` |
|      108 |  4946 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  4947 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  4948 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  4949 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4950 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4951 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  4952 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  4953 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  4954 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  4955 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  4956 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  4957 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  4958 | `				PH7_MemObjRelease(pTos);` |
|       11 |  4959 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  4960 | `				if( !bExists ){` |
|        3 |  4961 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  4962 | `				}else{` |
|        9 |  4963 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4964 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  4965 | `					ph7_value sValue;` |
|        9 |  4966 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4967 | `					apArg[0] = pIdx;` |
|        9 |  4968 | `					if( pGet ){` |
|        9 |  4969 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  4970 | `					}` |
|        9 |  4971 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  4972 | `					PH7_MemObjRelease(&sValue);` |
|        - |  4973 | `				}` |
|       11 |  4974 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  4975 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  4976 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  4977 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  4978 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  4979 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  4980 | `				 *     and push NULL.` |
|        - |  4981 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  4982 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  4983 | `				int bShouldArm = !bExists;` |
|        - |  4984 | `				ph7_value sValue;` |
|        9 |  4985 | `				PH7_MemObjRelease(&sResult);` |
|        - |  4986 | `				/* Reset any prior arming defensively */` |
|        9 |  4987 | `				VmCoalesceDisarm(pVm);` |
|        9 |  4988 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4989 | `				if( bExists ){` |
|        5 |  4990 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4991 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  4992 | `					apArg[0] = pIdx;` |
|        5 |  4993 | `					if( pGet ){` |
|        5 |  4994 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  4995 | `					}` |
|        5 |  4996 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  4997 | `						bShouldArm = 1;` |
|        1 |  4998 | `					}` |
|        2 |  4999 | `				}` |
|        9 |  5000 | `				PH7_MemObjRelease(pTos);` |
|        9 |  5001 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  5002 | `				if( bShouldArm ){` |
|        - |  5003 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5004 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5005 | `					 * intervening expression evaluation. */` |
|        7 |  5006 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  5007 | `					if( pIdx ){` |
|        7 |  5008 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5009 | `					}` |
|        7 |  5010 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  5011 | `					pInst->iRef++;` |
|        7 |  5012 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  5013 | `				}else{` |
|        3 |  5014 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5015 | `				}` |
|        9 |  5016 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  5017 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  5018 | `				break;` |
|      ! 0 |  5019 | `			}else{` |
|        - |  5020 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  5021 | `				PH7_MemObjRelease(pTos);` |
|       66 |  5022 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  5023 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5024 | `			}` |
|      106 |  5025 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  5026 | `			if( pIdx ){` |
|      106 |  5027 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5028 | `			}` |
|      106 |  5029 | `			break;` |
|        - |  5030 | `		}` |
|        - |  5031 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5032 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5033 | `		if( pInst ){` |
|        - |  5034 | `			char zMsg[256];` |
|        3 |  5035 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5036 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5037 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5038 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5039 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5040 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5041 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5042 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5043 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5044 | `			break;` |
|        - |  5045 | `		}` |
|      ! 0 |  5046 | `	}` |
|   113594 |  5047 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5048 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5049 | `			ph7_value *pObj;` |
|        3 |  5050 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5051 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5052 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5053 | `			}` |
|        1 |  5054 | `		}` |
|        1 |  5055 | `	}` |
|   113594 |  5056 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   113594 |  5057 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   113594 |  5058 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5059 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5060 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5061 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5062 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5063 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5064 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  5065 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  5066 | `		}` |
|        - |  5067 | `		/* Point to the hashmap */` |
|   113594 |  5068 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   113594 |  5069 | `		if( pIdx ){` |
|        - |  5070 | `			/* Load the desired entry */` |
|   113594 |  5071 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56796 |  5072 | `		}` |
|   113594 |  5073 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5074 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5075 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5076 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5077 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5078 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5079 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5080 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5081 | `			 * correct for the outermost write. */` |
|       19 |  5082 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5083 | `			if( !needWrite && pNode ){` |
|       13 |  5084 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5085 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5086 | `					needWrite = 1;` |
|        3 |  5087 | `				}` |
|        6 |  5088 | `			}` |
|       19 |  5089 | `			if( needWrite ){` |
|       13 |  5090 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5091 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5092 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5093 | `					 * into the new map's storage. */` |
|        7 |  5094 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5095 | `					if( pIdx ){` |
|        7 |  5096 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5097 | `					}` |
|        3 |  5098 | `				}` |
|        6 |  5099 | `			}` |
|        9 |  5100 | `		}` |
|   113594 |  5101 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5102 | `			/* Create a new empty entry */` |
|      273 |  5103 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5104 | `			if( rc == SXRET_OK ){` |
|        - |  5105 | `				/* Point to the last inserted entry */` |
|      273 |  5106 | `				pNode = pMap->pLast;` |
|      136 |  5107 | `			}` |
|      136 |  5108 | `		}` |
|    56796 |  5109 | `	}` |
|   113594 |  5110 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5111 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5112 | `		char zMsg[128];` |
|      ! 0 |  5113 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5114 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5115 | `		}` |
|      ! 0 |  5116 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5117 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5118 | `	}` |
|   113594 |  5119 | `	if( pIdx ){` |
|   113594 |  5120 | `		PH7_MemObjRelease(pIdx);` |
|    56796 |  5121 | `	}` |
|   113594 |  5122 | `	if( rc == SXRET_OK ){` |
|        - |  5123 | `		/* Load entry contents */` |
|    50360 |  5124 | `		if( pMap->iRef < 2 ){` |
|        - |  5125 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5126 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5127 | `			 */` |
|       28 |  5128 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5129 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5130 | `		}else{` |
|    50334 |  5131 | `			pTos->nIdx = pNode->nValIdx;` |
|    50334 |  5132 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50334 |  5133 | `			PH7_HashmapUnref(pMap);` |
|        - |  5134 | `		}` |
|    25181 |  5135 | `	}else{` |
|        - |  5136 | `		/* No such entry,load NULL */` |
|    63236 |  5137 | `		PH7_MemObjRelease(pTos);` |
|    63236 |  5138 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5139 | `	}` |
|   113594 |  5140 | `	break;` |
|        - |  5141 | `					  }` |
|        - |  5142 | `/*` |
|        - |  5143 | ` * LOAD_CLOSURE * * P3` |
|        - |  5144 | ` *` |
|        - |  5145 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5146 | ` * name in the stack.` |
|        - |  5147 | ` */` |
|       61 |  5148 | `case PH7_OP_LOAD_CLOSURE:{` |
|      124 |  5149 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      124 |  5150 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5151 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5152 | `		ph7_vm_func *pClosure;` |
|        - |  5153 | `		char *zName;` |
|        - |  5154 | `		sxu32 mLen;` |
|        - |  5155 | `		sxu32 n;` |
|        - |  5156 | `		/* Create a new VM function */` |
|      124 |  5157 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5158 | `		/* Generate an unique closure name */` |
|      124 |  5159 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      124 |  5160 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5161 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5162 | `			goto Abort;` |
|        - |  5163 | `		}` |
|      124 |  5164 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      124 |  5165 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5166 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5167 | `		}` |
|        - |  5168 | `		/* Zero the stucture */` |
|      124 |  5169 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5170 | `		/* Perform a structure assignment on read-only items */` |
|      124 |  5171 | `		pClosure->aArgs = pFunc->aArgs;` |
|      124 |  5172 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      124 |  5173 | `		pClosure->aStatic = pFunc->aStatic;` |
|      124 |  5174 | `		pClosure->iFlags = pFunc->iFlags;` |
|      124 |  5175 | `		pClosure->pUserData = pFunc->pUserData;` |
|      124 |  5176 | `		pClosure->sSignature = pFunc->sSignature;` |
|      124 |  5177 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      124 |  5178 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      124 |  5179 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      124 |  5180 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      124 |  5181 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5182 | `		/* Register the closure */` |
|      124 |  5183 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5184 | `		/* Set up closure environment */` |
|      124 |  5185 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      124 |  5186 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      312 |  5187 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5188 | `			ph7_value *pValue;` |
|      190 |  5189 | `			pEnv = &aEnv[n];` |
|      190 |  5190 | `			sEnv.sName  = pEnv->sName;` |
|      190 |  5191 | `			sEnv.iFlags = pEnv->iFlags;` |
|      190 |  5192 | `			sEnv.nIdx = SXU32_HIGH;` |
|      190 |  5193 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      190 |  5194 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5195 | `				/* Pass by reference */` |
|      ! 0 |  5196 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5197 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5198 | `					);` |
|      ! 0 |  5199 | `			}` |
|        - |  5200 | `			/* Standard pass by value */` |
|      190 |  5201 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      190 |  5202 | `			if( pValue ){` |
|        - |  5203 | `				/* Copy imported value */` |
|       72 |  5204 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5205 | `			}` |
|        - |  5206 | `			/* Insert the imported variable */` |
|      190 |  5207 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       96 |  5208 | `		}` |
|        - |  5209 | `		/* Finally,load the closure name on the stack */` |
|      124 |  5210 | `		pTos++;` |
|      124 |  5211 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       61 |  5212 | `	}` |
|      124 |  5213 | `	break;` |
|        - |  5214 | `						 }` |
|        - |  5215 | `/*` |
|        - |  5216 | ` * STORE * P2 P3` |
|        - |  5217 | ` *` |
|        - |  5218 | ` * Perform a store (Assignment) operation.` |
|        - |  5219 | ` */` |
|   146101 |  5220 | `case PH7_OP_STORE: {` |
|        - |  5221 | `	ph7_value *pObj;` |
|        - |  5222 | `	SyString sName;` |
|        - |  5223 | `#ifdef UNTRUST` |
|        - |  5224 | `	if( pTos < pStack ){` |
|        - |  5225 | `		goto Abort;` |
|        - |  5226 | `	}` |
|        - |  5227 | `#endif` |
|   292204 |  5228 | `	if( pInstr->iP2 ){` |
|        - |  5229 | `		sxu32 nIdx;` |
|        - |  5230 | `		sxi32 rcT;` |
|        - |  5231 | `		/* Member store operation */` |
|     5270 |  5232 | `		nIdx = pTos->nIdx;` |
|     5270 |  5233 | `		VmPopOperand(&pTos,1);` |
|     5270 |  5234 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5235 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5236 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5237 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5238 | `		}else{` |
|        - |  5239 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5240 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5266 |  5241 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5266 |  5242 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5243 | `				goto Abort;` |
|        - |  5244 | `			}` |
|     5266 |  5245 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5246 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5247 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5248 | `				 * propagate out of the VM loop. */` |
|       37 |  5249 | `				VmPopOperand(&pTos,1);` |
|        - |  5250 | `				{` |
|       37 |  5251 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5252 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5253 | `						pc = pFrm2->iExceptionJump - 1;` |
|   146120 |  5254 | `						break;` |
|        - |  5255 | `					}` |
|        - |  5256 | `				}` |
|      ! 0 |  5257 | `				goto Exception;` |
|        - |  5258 | `			}` |
|        - |  5259 | `			/* Point to the desired memory object */` |
|     5230 |  5260 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5230 |  5261 | `			if( pObj ){` |
|        - |  5262 | `				/* Perform the store operation */` |
|     5230 |  5263 | `				PH7_MemObjStore(pTos,pObj);` |
|     2614 |  5264 | `			}` |
|        - |  5265 | `		}` |
|     5234 |  5266 | `		break;` |
|   286936 |  5267 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5268 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5269 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5270 | `			/* Force a string cast */` |
|      ! 0 |  5271 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5272 | `		}` |
|        7 |  5273 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5274 | `		pTos--;` |
|        - |  5275 | `#ifdef UNTRUST` |
|        - |  5276 | `		if( pTos < pStack  ){` |
|        - |  5277 | `			goto Abort;` |
|        - |  5278 | `		}` |
|        - |  5279 | `#endif` |
|        4 |  5280 | `	}else{` |
|   286930 |  5281 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5282 | `	}` |
|        - |  5283 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   286936 |  5284 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   286936 |  5285 | `	if( pObj == 0 ){` |
|      ! 0 |  5286 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5287 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5288 | `		goto Abort;` |
|        - |  5289 | `	}` |
|   286936 |  5290 | `	if( !pInstr->p3 ){` |
|        7 |  5291 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5292 | `	}` |
|        - |  5293 | `	/* Perform the store operation */` |
|   286936 |  5294 | `	PH7_MemObjStore(pTos,pObj);` |
|   286936 |  5295 | `	break;` |
|        - |  5296 | `				   }` |
|        - |  5297 | `/*` |
|        - |  5298 | ` * STORE_IDX:   P1 * P3` |
|        - |  5299 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5300 | ` *` |
|        - |  5301 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5302 | ` */` |
|    97015 |  5303 | `case PH7_OP_STORE_IDX:` |
|        - |  5304 | `case PH7_OP_STORE_IDX_REF: {` |
|   194032 |  5305 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5306 | `	ph7_value *pKey;` |
|        - |  5307 | `	sxu32 nIdx;` |
|   194032 |  5308 | `	if( pInstr->iP1 ){` |
|        - |  5309 | `		/* Key is next on stack */` |
|    63328 |  5310 | `		pKey = pTos;` |
|    63328 |  5311 | `		pTos--;` |
|    31665 |  5312 | `	}else{` |
|   130706 |  5313 | `		pKey = 0;` |
|        - |  5314 | `	}` |
|   194032 |  5315 | `	nIdx = pTos->nIdx;` |
|        - |  5316 | `	{` |
|        - |  5317 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5318 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5319 | `		 * the backing variable slot at nIdx. */` |
|   194032 |  5320 | `		ph7_class_instance *pInst = 0;` |
|   194032 |  5321 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5322 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   194016 |  5323 | `		}else if( nIdx != SXU32_HIGH ){` |
|   194000 |  5324 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   194000 |  5325 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5326 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5327 | `			}` |
|    96999 |  5328 | `		}` |
|   194032 |  5329 | `		if( pInst ){` |
|       34 |  5330 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5331 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5332 | `				ph7_class_method *pMeth;` |
|        - |  5333 | `				ph7_value sNullKey;` |
|        - |  5334 | `				ph7_value *apArg[2];` |
|       32 |  5335 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5336 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5337 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5338 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5339 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5340 | `					break;` |
|        - |  5341 | `				}` |
|       32 |  5342 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5343 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5344 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5345 | `				VmPopOperand(&pTos,1);` |
|       32 |  5346 | `				if( pKey == 0 ){` |
|        7 |  5347 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5348 | `					apArg[0] = &sNullKey;` |
|        4 |  5349 | `				}else{` |
|       26 |  5350 | `					apArg[0] = pKey;` |
|        - |  5351 | `				}` |
|       32 |  5352 | `				apArg[1] = pTos;` |
|       32 |  5353 | `				if( pMeth ){` |
|       32 |  5354 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5355 | `				}` |
|       32 |  5356 | `				if( pKey ){` |
|       26 |  5357 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5358 | `				}else{` |
|        7 |  5359 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5360 | `				}` |
|        - |  5361 | `				/* Pop the value */` |
|       32 |  5362 | `				VmPopOperand(&pTos,1);` |
|       32 |  5363 | `				break;` |
|        - |  5364 | `			}` |
|        - |  5365 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5366 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5367 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5368 | `			 * a few lines below). Match PHP. */` |
|        - |  5369 | `			{` |
|        - |  5370 | `				char zMsg[256];` |
|        3 |  5371 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5372 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5373 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5374 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5375 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5376 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5377 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5378 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5379 | `				break;` |
|        - |  5380 | `			}` |
|        - |  5381 | `		}` |
|        - |  5382 | `	}` |
|   194000 |  5383 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5384 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5385 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5386 | `		 * checking true sharing count, then re-add after separation. */` |
|   193948 |  5387 | `		if( nIdx != SXU32_HIGH ){` |
|   193948 |  5388 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   290921 |  5389 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   193948 |  5390 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5391 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5392 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5393 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5394 | `				 * refcounts if the backing array was already separated. */` |
|   193948 |  5395 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   193948 |  5396 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   193948 |  5397 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   193948 |  5398 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   193948 |  5399 | `					pTos->x.pOther = pMap;` |
|    96975 |  5400 | `				}else{` |
|        - |  5401 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5402 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5403 | `					pMap = pCur;` |
|        - |  5404 | `				}` |
|    96975 |  5405 | `			}else{` |
|      ! 0 |  5406 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5407 | `			}` |
|    96975 |  5408 | `		}else{` |
|      ! 0 |  5409 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5410 | `		}` |
|   193948 |  5411 | `		if( pMap->iRef < 2 ){` |
|        - |  5412 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5413 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5414 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5415 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5416 | `			pMap->iRef = 2;` |
|      ! 0 |  5417 | `		}` |
|    96975 |  5418 | `	}else{` |
|        - |  5419 | `		ph7_value *pObj;` |
|       53 |  5420 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5421 | `		if( pObj == 0 ){` |
|      ! 0 |  5422 | `			if( pKey ){` |
|      ! 0 |  5423 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5424 | `			}` |
|      ! 0 |  5425 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5426 | `			break;` |
|        - |  5427 | `		}` |
|        - |  5428 | `		/* Phase#1: Load the array */` |
|       53 |  5429 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5430 | `			VmPopOperand(&pTos,1);` |
|       53 |  5431 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5432 | `				/* Force a string cast */` |
|      ! 0 |  5433 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5434 | `			}` |
|       53 |  5435 | `			if( pKey == 0 ){` |
|        - |  5436 | `				/* Append string */` |
|        3 |  5437 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5438 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5439 | `				}` |
|        2 |  5440 | `			}else{` |
|        - |  5441 | `				sxu32 nOfft;` |
|       51 |  5442 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5443 | `					/* Force an int cast */` |
|       51 |  5444 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5445 | `				}` |
|       51 |  5446 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5447 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5448 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5449 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5450 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5451 | `				}else{` |
|      ! 0 |  5452 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5453 | `						/* Perform an append operation */` |
|      ! 0 |  5454 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5455 | `					}` |
|        - |  5456 | `				}` |
|        - |  5457 | `			}` |
|       53 |  5458 | `			if( pKey ){` |
|       51 |  5459 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5460 | `			}` |
|       53 |  5461 | `			break;` |
|      ! 0 |  5462 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5463 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5464 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5465 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5466 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5467 | `				goto Abort;` |
|        - |  5468 | `			}` |
|      ! 0 |  5469 | `		}` |
|        - |  5470 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5471 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5472 | `	}` |
|   193948 |  5473 | `	VmPopOperand(&pTos,1);` |
|        - |  5474 | `	/* Phase#2: Perform the insertion */` |
|   193948 |  5475 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5476 | `		/* Insertion by reference */` |
|       15 |  5477 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5478 | `	}else{` |
|   193934 |  5479 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5480 | `	}` |
|   193948 |  5481 | `	if( pKey ){` |
|    63252 |  5482 | `		PH7_MemObjRelease(pKey);` |
|    31625 |  5483 | `	}` |
|   193948 |  5484 | `	break;` |
|        - |  5485 | `					   }` |
|        - |  5486 | `/*` |
|        - |  5487 | ` * INCR: P1 * *` |
|        - |  5488 | ` *` |
|        - |  5489 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5490 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5491 | ` * the stack and increment after that.` |
|        - |  5492 | ` */` |
|   167689 |  5493 | `case PH7_OP_INCR:` |
|        - |  5494 | `#ifdef UNTRUST` |
|        - |  5495 | `	if( pTos < pStack ){` |
|        - |  5496 | `		goto Abort;` |
|        - |  5497 | `	}` |
|        - |  5498 | `#endif` |
|   335424 |  5499 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335424 |  5500 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5501 | `			ph7_value *pObj;` |
|   335424 |  5502 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335424 |  5503 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5504 | `					/* Perl-style string increment.` |
|        - |  5505 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5506 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5507 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5508 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5509 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5510 | `					}` |
|       49 |  5511 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5512 | `					if( pInstr->iP1 ){` |
|        - |  5513 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5514 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5515 | `					}` |
|       25 |  5516 | `				}else{` |
|        - |  5517 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5518 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5519 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5520 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5521 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5522 | `					 * so its old-value view survives the coercion. */` |
|   335376 |  5523 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5524 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5525 | `					}` |
|        - |  5526 | `					/* Force a numeric cast on the variable */` |
|   335376 |  5527 | `					PH7_MemObjToNumeric(pObj);` |
|   335376 |  5528 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5529 | `						pObj->rVal++;` |
|        - |  5530 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5531 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5532 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5533 | `						 * integer-valued real. */` |
|        9 |  5534 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5535 | `					}else{` |
|   335368 |  5536 | `						pObj->x.iVal++;` |
|        - |  5537 | `					}` |
|   335376 |  5538 | `					if( pInstr->iP1 ){` |
|        - |  5539 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5540 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5541 | `					}` |
|        - |  5542 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5543 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5544 | `				}` |
|   167733 |  5545 | `			}` |
|   167735 |  5546 | `		}else{` |
|      ! 0 |  5547 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5548 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5549 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5550 | `				}else{` |
|        - |  5551 | `					/* Force a numeric cast */` |
|      ! 0 |  5552 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5553 | `					/* Pre-increment */` |
|      ! 0 |  5554 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5555 | `						pTos->rVal++;` |
|        - |  5556 | `						/* Try to get an integer representation */` |
|      ! 0 |  5557 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5558 | `					}else{` |
|      ! 0 |  5559 | `						pTos->x.iVal++;` |
|      ! 0 |  5560 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5561 | `					}` |
|        - |  5562 | `				}` |
|      ! 0 |  5563 | `			}` |
|        - |  5564 | `		}` |
|   167733 |  5565 | `	}` |
|   335424 |  5566 | `	break;` |
|        - |  5567 | `/*` |
|        - |  5568 | ` * DECR: P1 * *` |
|        - |  5569 | ` *` |
|        - |  5570 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5571 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5572 | ` * and decrement after that.` |
|        - |  5573 | ` */` |
|       14 |  5574 | `case PH7_OP_DECR:` |
|        - |  5575 | `#ifdef UNTRUST` |
|        - |  5576 | `	if( pTos < pStack ){` |
|        - |  5577 | `		goto Abort;` |
|        - |  5578 | `	}` |
|        - |  5579 | `#endif` |
|        - |  5580 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5581 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5582 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5583 | `			ph7_value *pObj;` |
|       27 |  5584 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5585 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5586 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5587 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5588 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5589 | `					if( pInstr->iP1 ){` |
|        - |  5590 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5591 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5592 | `					}` |
|        - |  5593 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5594 | `				}else{` |
|        - |  5595 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5596 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5597 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5598 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5599 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5600 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5601 | `					}` |
|       21 |  5602 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5603 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5604 | `						pObj->rVal--;` |
|        - |  5605 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5606 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5607 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5608 | `						 * integer-valued real. */` |
|        9 |  5609 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5610 | `					}else{` |
|       13 |  5611 | `						pObj->x.iVal--;` |
|        - |  5612 | `					}` |
|       21 |  5613 | `					if( pInstr->iP1 ){` |
|        - |  5614 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5615 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5616 | `					}` |
|        - |  5617 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5618 | `				}` |
|       13 |  5619 | `			}` |
|       14 |  5620 | `		}else{` |
|      ! 0 |  5621 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5622 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5623 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5624 | `				}else{` |
|        - |  5625 | `					/* Force a numeric cast */` |
|      ! 0 |  5626 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5627 | `					/* Pre-decrement */` |
|      ! 0 |  5628 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5629 | `						pTos->rVal--;` |
|        - |  5630 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5631 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5632 | `					}else{` |
|      ! 0 |  5633 | `						pTos->x.iVal--;` |
|      ! 0 |  5634 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5635 | `					}` |
|        - |  5636 | `				}` |
|      ! 0 |  5637 | `			}` |
|        - |  5638 | `		}` |
|       13 |  5639 | `	}` |
|       29 |  5640 | `	break;` |
|        - |  5641 | `/*` |
|        - |  5642 | ` * UMINUS: * * *` |
|        - |  5643 | ` *` |
|        - |  5644 | ` * Perform a unary minus operation.` |
|        - |  5645 | ` */` |
|    29723 |  5646 | `case PH7_OP_UMINUS:` |
|        - |  5647 | `#ifdef UNTRUST` |
|        - |  5648 | `	if( pTos < pStack ){` |
|        - |  5649 | `		goto Abort;` |
|        - |  5650 | `	}` |
|        - |  5651 | `#endif` |
|        - |  5652 | `	/* Force a numeric (integer,real or both) cast */` |
|    59448 |  5653 | `	PH7_MemObjToNumeric(pTos);` |
|    59448 |  5654 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5655 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5656 | `	}` |
|    59448 |  5657 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59418 |  5658 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29708 |  5659 | `	}` |
|    59448 |  5660 | `	break;` |
|        - |  5661 | `/*` |
|        - |  5662 | ` * UPLUS: * * *` |
|        - |  5663 | ` *` |
|        - |  5664 | ` * Perform a unary plus operation.` |
|        - |  5665 | ` */` |
|       18 |  5666 | `case PH7_OP_UPLUS:` |
|        - |  5667 | `#ifdef UNTRUST` |
|        - |  5668 | `	if( pTos < pStack ){` |
|        - |  5669 | `		goto Abort;` |
|        - |  5670 | `	}` |
|        - |  5671 | `#endif` |
|        - |  5672 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5673 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5674 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5675 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5676 | `	}` |
|       37 |  5677 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5678 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5679 | `	}` |
|       37 |  5680 | `	break;` |
|        - |  5681 | `/*` |
|        - |  5682 | ` * OP_LNOT: * * *` |
|        - |  5683 | ` *` |
|        - |  5684 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5685 | ` * with its complement.` |
|        - |  5686 | ` */` |
|    44838 |  5687 | `case PH7_OP_LNOT:` |
|        - |  5688 | `#ifdef UNTRUST` |
|        - |  5689 | `	if( pTos < pStack ){` |
|        - |  5690 | `		goto Abort;` |
|        - |  5691 | `	}` |
|        - |  5692 | `#endif` |
|        - |  5693 | `	/* Force a boolean cast */` |
|    89722 |  5694 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5695 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5696 | `	}` |
|    89722 |  5697 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89722 |  5698 | `	break;` |
|        - |  5699 | `/*` |
|        - |  5700 | ` * OP_BITNOT: * * *` |
|        - |  5701 | ` *` |
|        - |  5702 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5703 | ` * with its ones-complement.` |
|        - |  5704 | ` */` |
|       14 |  5705 | `case PH7_OP_BITNOT:` |
|        - |  5706 | `#ifdef UNTRUST` |
|        - |  5707 | `	if( pTos < pStack ){` |
|        - |  5708 | `		goto Abort;` |
|        - |  5709 | `	}` |
|        - |  5710 | `#endif` |
|        - |  5711 | `	/* Force an integer cast */` |
|       30 |  5712 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5713 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5714 | `	}` |
|       30 |  5715 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  5716 | `	break;` |
|        - |  5717 | `/* OP_MUL * * *` |
|        - |  5718 | ` * OP_MUL_STORE * * *` |
|        - |  5719 | ` *` |
|        - |  5720 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5721 | ` * and push the result back onto the stack.` |
|        - |  5722 | ` */` |
|     1290 |  5723 | `case PH7_OP_MUL:` |
|        - |  5724 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  5725 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5726 | `	/* Force the operand to be numeric */` |
|        - |  5727 | `#ifdef UNTRUST` |
|        - |  5728 | `	if( pNos < pStack ){` |
|        - |  5729 | `		goto Abort;` |
|        - |  5730 | `	}` |
|        - |  5731 | `#endif` |
|     2582 |  5732 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  5733 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5734 | `	/* Perform the requested operation */` |
|     2582 |  5735 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5736 | `		/* Floating point arithemic */` |
|        - |  5737 | `		ph7_real a,b,r;` |
|       21 |  5738 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5739 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5740 | `		}` |
|       21 |  5741 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5742 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5743 | `		}` |
|       21 |  5744 | `		a = pNos->rVal;` |
|       21 |  5745 | `		b = pTos->rVal;` |
|       21 |  5746 | `		r = a * b;` |
|        - |  5747 | `		/* Push the result */` |
|       21 |  5748 | `		pNos->rVal = r;` |
|       21 |  5749 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5750 | `		/* Try to get an integer representation */` |
|       21 |  5751 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  5752 | `	}else{` |
|        - |  5753 | `		/* Integer arithmetic */` |
|        - |  5754 | `		sxi64 a,b,r;` |
|     2562 |  5755 | `		a = pNos->x.iVal;` |
|     2562 |  5756 | `		b = pTos->x.iVal;` |
|     2562 |  5757 | `		r = a * b;` |
|        - |  5758 | `		/* Push the result */` |
|     2562 |  5759 | `		pNos->x.iVal = r;` |
|     2562 |  5760 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5761 | `	}` |
|     2582 |  5762 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5763 | `		ph7_value *pObj;` |
|       32 |  5764 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5765 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5766 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5767 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5768 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5769 | `		}` |
|       15 |  5770 | `	}` |
|     2582 |  5771 | `	VmPopOperand(&pTos,1);` |
|     2582 |  5772 | `	break;` |
|        - |  5773 | `				 }` |
|        - |  5774 | `/* OP_POW * * *` |
|        - |  5775 | ` * OP_POW_STORE * * *` |
|        - |  5776 | ` *` |
|        - |  5777 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5778 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5779 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5780 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5781 | ` */` |
|       67 |  5782 | `case PH7_OP_POW:` |
|        - |  5783 | `case PH7_OP_POW_STORE: {` |
|      135 |  5784 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  5785 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5786 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5787 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5788 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5789 | `	 */` |
|      135 |  5790 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  5791 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5792 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5793 | `	int bBothInt;` |
|      135 |  5794 | `	int usedInt = 0;` |
|        - |  5795 | `	ph7_real a, b, r;` |
|        - |  5796 | `#endif` |
|      135 |  5797 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5798 | `#ifdef UNTRUST` |
|        - |  5799 | `	if( pNos < pStack ){` |
|        - |  5800 | `		goto Abort;` |
|        - |  5801 | `	}` |
|        - |  5802 | `#endif` |
|      135 |  5803 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  5804 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5805 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  5806 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  5807 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  5808 | `	if( bBothInt ){` |
|      123 |  5809 | `		base_i = pBase->x.iVal;` |
|      123 |  5810 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5811 | `	}` |
|      135 |  5812 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5813 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5814 | `	}` |
|      135 |  5815 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  5816 | `		PH7_MemObjToReal(pExp);` |
|       66 |  5817 | `	}` |
|      135 |  5818 | `	a = pBase->rVal;` |
|      135 |  5819 | `	b = pExp->rVal;` |
|      135 |  5820 | `	r = pow(a, b);` |
|        - |  5821 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5822 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5823 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5824 | `	 * representable as double but not as signed int64. */` |
|      135 |  5825 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5826 | `		sxi64 result_i = 1;` |
|      117 |  5827 | `		sxi64 cur_base = base_i;` |
|      117 |  5828 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5829 | `		int overflow = 0;` |
|      401 |  5830 | `		while( cur_exp > 0 ){` |
|      289 |  5831 | `			if( cur_exp & 1 ){` |
|      189 |  5832 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5833 | `					overflow = 1;` |
|        3 |  5834 | `					break;` |
|        - |  5835 | `				}` |
|       93 |  5836 | `			}` |
|      287 |  5837 | `			cur_exp >>= 1;` |
|      287 |  5838 | `			if( cur_exp > 0 ){` |
|      181 |  5839 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5840 | `					overflow = 1;` |
|        3 |  5841 | `					break;` |
|        - |  5842 | `				}` |
|       89 |  5843 | `			}` |
|        1 |  5844 | `		}` |
|      117 |  5845 | `		if( !overflow ){` |
|      113 |  5846 | `			pNos->x.iVal = result_i;` |
|      113 |  5847 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5848 | `			usedInt = 1;` |
|       56 |  5849 | `		}` |
|       58 |  5850 | `	}` |
|      135 |  5851 | `	if( !usedInt ){` |
|       23 |  5852 | `		pNos->rVal = r;` |
|       23 |  5853 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  5854 | `	}` |
|        - |  5855 | `#else` |
|        - |  5856 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5857 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5858 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5859 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5860 | `	 * represented. */` |
|        - |  5861 | `	base_i = pBase->x.iVal;` |
|        - |  5862 | `	exp_i  = pExp->x.iVal;` |
|        - |  5863 | `	{` |
|        - |  5864 | `		sxi64 result_i = 1;` |
|        - |  5865 | `		sxi64 cur_base = base_i;` |
|        - |  5866 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5867 | `		if( cur_exp < 0 ){` |
|        - |  5868 | `			result_i = 0;` |
|        - |  5869 | `		}else{` |
|        - |  5870 | `			while( cur_exp > 0 ){` |
|        - |  5871 | `				if( cur_exp & 1 ){` |
|        - |  5872 | `					result_i *= cur_base;` |
|        - |  5873 | `				}` |
|        - |  5874 | `				cur_exp >>= 1;` |
|        - |  5875 | `				if( cur_exp > 0 ){` |
|        - |  5876 | `					cur_base *= cur_base;` |
|        - |  5877 | `				}` |
|        - |  5878 | `			}` |
|        - |  5879 | `		}` |
|        - |  5880 | `		pNos->x.iVal = result_i;` |
|        - |  5881 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5882 | `	}` |
|        - |  5883 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  5884 | `	if( bStore ){` |
|        - |  5885 | `		ph7_value *pObj;` |
|       23 |  5886 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5887 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5888 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5889 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5890 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5891 | `		}` |
|       11 |  5892 | `	}` |
|      135 |  5893 | `	VmPopOperand(&pTos,1);` |
|      135 |  5894 | `	break;` |
|        - |  5895 | `				 }` |
|        - |  5896 | `/* OP_ADD * * *` |
|        - |  5897 | ` *` |
|        - |  5898 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5899 | ` * and push the result back onto the stack.` |
|        - |  5900 | ` */` |
|      528 |  5901 | `case PH7_OP_ADD:{` |
|     1058 |  5902 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5903 | `#ifdef UNTRUST` |
|        - |  5904 | `	if( pNos < pStack ){` |
|        - |  5905 | `		goto Abort;` |
|        - |  5906 | `	}` |
|        - |  5907 | `#endif` |
|        - |  5908 | `	/* Perform the addition */` |
|     1058 |  5909 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1058 |  5910 | `	VmPopOperand(&pTos,1);` |
|     1058 |  5911 | `	break;` |
|        - |  5912 | `				}` |
|        - |  5913 | `/*` |
|        - |  5914 | ` * OP_ADD_STORE * * *` |
|        - |  5915 | ` *` |
|        - |  5916 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5917 | ` * and push the result back onto the stack.` |
|        - |  5918 | ` */` |
|      502 |  5919 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5920 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5921 | `	ph7_value *pObj;` |
|        - |  5922 | `	sxu32 nIdx;` |
|        - |  5923 | `#ifdef UNTRUST` |
|        - |  5924 | `	if( pNos < pStack ){` |
|        - |  5925 | `		goto Abort;` |
|        - |  5926 | `	}` |
|        - |  5927 | `#endif` |
|        - |  5928 | `	/* Perform the addition */` |
|     1006 |  5929 | `	nIdx = pTos->nIdx;` |
|     1006 |  5930 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5931 | `	/* Peform the store operation */` |
|     1006 |  5932 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5933 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5934 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5935 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5936 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5937 | `	}` |
|        - |  5938 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5939 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5940 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5941 | `	break;` |
|        - |  5942 | `				}` |
|        - |  5943 | `/* OP_SUB * * *` |
|        - |  5944 | ` *` |
|        - |  5945 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5946 | ` * first (what was next on the stack) from the second (the` |
|        - |  5947 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5948 | ` */` |
|      349 |  5949 | `case PH7_OP_SUB: {` |
|      700 |  5950 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5951 | `#ifdef UNTRUST` |
|        - |  5952 | `	if( pNos < pStack ){` |
|        - |  5953 | `		goto Abort;` |
|        - |  5954 | `	}` |
|        - |  5955 | `#endif` |
|      700 |  5956 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5957 | `		/* Floating point arithemic */` |
|        - |  5958 | `		ph7_real a,b,r;` |
|       97 |  5959 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5960 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5961 | `		}` |
|       97 |  5962 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5963 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5964 | `		}` |
|       97 |  5965 | `		a = pNos->rVal;` |
|       97 |  5966 | `		b = pTos->rVal;` |
|       97 |  5967 | `		r = a - b;` |
|        - |  5968 | `		/* Push the result */` |
|       97 |  5969 | `		pNos->rVal = r;` |
|       97 |  5970 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5971 | `		/* Try to get an integer representation */` |
|       97 |  5972 | `		PH7_MemObjTryInteger(pNos);` |
|       49 |  5973 | `	}else{` |
|        - |  5974 | `		/* Integer arithmetic */` |
|        - |  5975 | `		sxi64 a,b,r;` |
|      604 |  5976 | `		a = pNos->x.iVal;` |
|      604 |  5977 | `		b = pTos->x.iVal;` |
|      604 |  5978 | `		r = a - b;` |
|        - |  5979 | `		/* Push the result */` |
|      604 |  5980 | `		pNos->x.iVal = r;` |
|      604 |  5981 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5982 | `	}` |
|      700 |  5983 | `	VmPopOperand(&pTos,1);` |
|      700 |  5984 | `	break;` |
|        - |  5985 | `				 }` |
|        - |  5986 | `/* OP_SUB_STORE * * *` |
|        - |  5987 | ` *` |
|        - |  5988 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5989 | ` * first (what was next on the stack) from the second (the` |
|        - |  5990 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5991 | ` */` |
|        4 |  5992 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5993 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5994 | `	ph7_value *pObj;` |
|        - |  5995 | `#ifdef UNTRUST` |
|        - |  5996 | `	if( pNos < pStack ){` |
|        - |  5997 | `		goto Abort;` |
|        - |  5998 | `	}` |
|        - |  5999 | `#endif` |
|       10 |  6000 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6001 | `		/* Floating point arithemic */` |
|        - |  6002 | `		ph7_real a,b,r;` |
|      ! 0 |  6003 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6004 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6005 | `		}` |
|      ! 0 |  6006 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6007 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6008 | `		}` |
|      ! 0 |  6009 | `		a = pTos->rVal;` |
|      ! 0 |  6010 | `		b = pNos->rVal;` |
|      ! 0 |  6011 | `		r = a - b;` |
|        - |  6012 | `		/* Push the result */` |
|      ! 0 |  6013 | `		pNos->rVal = r;` |
|      ! 0 |  6014 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6015 | `		/* Try to get an integer representation */` |
|      ! 0 |  6016 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6017 | `	}else{` |
|        - |  6018 | `		/* Integer arithmetic */` |
|        - |  6019 | `		sxi64 a,b,r;` |
|       10 |  6020 | `		a = pTos->x.iVal;` |
|       10 |  6021 | `		b = pNos->x.iVal;` |
|       10 |  6022 | `		r = a - b;` |
|        - |  6023 | `		/* Push the result */` |
|       10 |  6024 | `		pNos->x.iVal = r;` |
|       10 |  6025 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6026 | `	}` |
|       10 |  6027 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6028 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6029 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6030 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6031 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6032 | `	}` |
|       10 |  6033 | `	VmPopOperand(&pTos,1);` |
|       10 |  6034 | `	break;` |
|        - |  6035 | `				 }` |
|        - |  6036 |  |
|        - |  6037 | `/*` |
|        - |  6038 | ` * OP_MOD * * *` |
|        - |  6039 | ` *` |
|        - |  6040 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6041 | ` * first (what was next on the stack) from the second (the` |
|        - |  6042 | ` * top of the stack) and push the remainder after division` |
|        - |  6043 | ` * onto the stack.` |
|        - |  6044 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6045 | ` */` |
|      308 |  6046 | `case PH7_OP_MOD:{` |
|      618 |  6047 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6048 | `	sxi64 a,b,r;` |
|        - |  6049 | `#ifdef UNTRUST` |
|        - |  6050 | `	if( pNos < pStack ){` |
|        - |  6051 | `		goto Abort;` |
|        - |  6052 | `	}` |
|        - |  6053 | `#endif` |
|        - |  6054 | `	/* Force the operands to be integer */` |
|      618 |  6055 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6056 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6057 | `	}` |
|      618 |  6058 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6059 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6060 | `	}` |
|        - |  6061 | `	/* Perform the requested operation */` |
|      618 |  6062 | `	a = pNos->x.iVal;` |
|      618 |  6063 | `	b = pTos->x.iVal;` |
|      618 |  6064 | `	if( b == 0 ){` |
|        3 |  6065 | `		r = 0;` |
|        3 |  6066 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6067 | `		/* goto Abort; */` |
|        2 |  6068 | `	}else{` |
|      615 |  6069 | `		r = a%b;` |
|        - |  6070 | `	}` |
|        - |  6071 | `	/* Push the result */` |
|      618 |  6072 | `	pNos->x.iVal = r;` |
|      618 |  6073 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  6074 | `	VmPopOperand(&pTos,1);` |
|      618 |  6075 | `	break;` |
|        - |  6076 | `				}` |
|        - |  6077 | `/*` |
|        - |  6078 | ` * OP_MOD_STORE * * *` |
|        - |  6079 | ` *` |
|        - |  6080 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6081 | ` * first (what was next on the stack) from the second (the` |
|        - |  6082 | ` * top of the stack) and push the remainder after division` |
|        - |  6083 | ` * onto the stack.` |
|        - |  6084 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6085 | ` */` |
|        1 |  6086 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6087 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6088 | `	ph7_value *pObj;` |
|        - |  6089 | `	sxi64 a,b,r;` |
|        - |  6090 | `#ifdef UNTRUST` |
|        - |  6091 | `	if( pNos < pStack ){` |
|        - |  6092 | `		goto Abort;` |
|        - |  6093 | `	}` |
|        - |  6094 | `#endif` |
|        - |  6095 | `	/* Force the operands to be integer */` |
|        3 |  6096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6098 | `	}` |
|        3 |  6099 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6100 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6101 | `	}` |
|        - |  6102 | `	/* Perform the requested operation */` |
|        3 |  6103 | `	a = pTos->x.iVal;` |
|        3 |  6104 | `	b = pNos->x.iVal;` |
|        3 |  6105 | `	if( b == 0 ){` |
|      ! 0 |  6106 | `		r = 0;` |
|      ! 0 |  6107 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6108 | `		/* goto Abort; */` |
|      ! 0 |  6109 | `	}else{` |
|        3 |  6110 | `		r = a%b;` |
|        - |  6111 | `	}` |
|        - |  6112 | `	/* Push the result */` |
|        3 |  6113 | `	pNos->x.iVal = r;` |
|        3 |  6114 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6115 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6116 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6117 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6118 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6119 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6120 | `	}` |
|        3 |  6121 | `	VmPopOperand(&pTos,1);` |
|        3 |  6122 | `	break;` |
|        - |  6123 | `				}` |
|        - |  6124 | `/*` |
|        - |  6125 | ` * OP_DIV * * *` |
|        - |  6126 | ` *` |
|        - |  6127 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6128 | ` * first (what was next on the stack) from the second (the` |
|        - |  6129 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6130 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6131 | ` */` |
|       33 |  6132 | `case PH7_OP_DIV:{` |
|       68 |  6133 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6134 | `	ph7_real a,b,r;` |
|        - |  6135 | `#ifdef UNTRUST` |
|        - |  6136 | `	if( pNos < pStack ){` |
|        - |  6137 | `		goto Abort;` |
|        - |  6138 | `	}` |
|        - |  6139 | `#endif` |
|        - |  6140 | `	/* Force the operands to be real */` |
|       68 |  6141 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6142 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6143 | `	}` |
|       68 |  6144 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6145 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6146 | `	}` |
|        - |  6147 | `	/* Perform the requested operation */` |
|       68 |  6148 | `	a = pNos->rVal;` |
|       68 |  6149 | `	b = pTos->rVal;` |
|       68 |  6150 | `	if( b == 0 ){` |
|        - |  6151 | `		/* Division by zero */` |
|        3 |  6152 | `		pNos->rVal = 0;` |
|        3 |  6153 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6154 | `		/* goto Abort; */` |
|        2 |  6155 | `	}else{` |
|       65 |  6156 | `		r = a/b;` |
|        - |  6157 | `		/* Push the result */` |
|       65 |  6158 | `		pNos->rVal = r;` |
|       65 |  6159 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6160 | `		/* Try to get an integer representation */` |
|       65 |  6161 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6162 | `	}` |
|       68 |  6163 | `	VmPopOperand(&pTos,1);` |
|       68 |  6164 | `	break;` |
|        - |  6165 | `				}` |
|        - |  6166 | `/*` |
|        - |  6167 | ` * OP_DIV_STORE * * *` |
|        - |  6168 | ` *` |
|        - |  6169 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6170 | ` * first (what was next on the stack) from the second (the` |
|        - |  6171 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6172 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6173 | ` */` |
|        2 |  6174 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6175 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6176 | `	ph7_value *pObj;` |
|        - |  6177 | `	ph7_real a,b,r;` |
|        - |  6178 | `#ifdef UNTRUST` |
|        - |  6179 | `	if( pNos < pStack ){` |
|        - |  6180 | `		goto Abort;` |
|        - |  6181 | `	}` |
|        - |  6182 | `#endif` |
|        - |  6183 | `	/* Force the operands to be real */` |
|        5 |  6184 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6185 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6186 | `	}` |
|        5 |  6187 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6188 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6189 | `	}` |
|        - |  6190 | `	/* Perform the requested operation */` |
|        5 |  6191 | `	a = pTos->rVal;` |
|        5 |  6192 | `	b = pNos->rVal;` |
|        5 |  6193 | `	if( b == 0 ){` |
|        - |  6194 | `		/* Division by zero */` |
|      ! 0 |  6195 | `		r = 0;` |
|      ! 0 |  6196 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6197 | `		/* goto Abort; */` |
|      ! 0 |  6198 | `	}else{` |
|        5 |  6199 | `		r = a/b;` |
|        - |  6200 | `		/* Push the result */` |
|        5 |  6201 | `		pNos->rVal = r;` |
|        5 |  6202 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6203 | `		/* Try to get an integer representation */` |
|        5 |  6204 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6205 | `	}` |
|        5 |  6206 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6207 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6208 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6209 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6210 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6211 | `	}` |
|        5 |  6212 | `	VmPopOperand(&pTos,1);` |
|        5 |  6213 | `	break;` |
|        - |  6214 | `				}` |
|        - |  6215 | `/* OP_BAND * * *` |
|        - |  6216 | ` *` |
|        - |  6217 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6218 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6219 | ` * two elements.` |
|        - |  6220 | `*/` |
|        - |  6221 | `/* OP_BOR * * *` |
|        - |  6222 | ` *` |
|        - |  6223 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6224 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6225 | ` * two elements.` |
|        - |  6226 | ` */` |
|        - |  6227 | `/* OP_BXOR * * *` |
|        - |  6228 | ` *` |
|        - |  6229 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6230 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6231 | ` * two elements.` |
|        - |  6232 | ` */` |
|       43 |  6233 | `case PH7_OP_BAND:` |
|        - |  6234 | `case PH7_OP_BOR:` |
|        - |  6235 | `case PH7_OP_BXOR:{` |
|       88 |  6236 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6237 | `	sxi64 a,b,r;` |
|        - |  6238 | `#ifdef UNTRUST` |
|        - |  6239 | `	if( pNos < pStack ){` |
|        - |  6240 | `		goto Abort;` |
|        - |  6241 | `	}` |
|        - |  6242 | `#endif` |
|        - |  6243 | `	/* Force the operands to be integer */` |
|       88 |  6244 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6245 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6246 | `	}` |
|       88 |  6247 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6248 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6249 | `	}` |
|        - |  6250 | `	/* Perform the requested operation */` |
|       88 |  6251 | `	a = pNos->x.iVal;` |
|       88 |  6252 | `	b = pTos->x.iVal;` |
|       88 |  6253 | `	switch(pInstr->iOp){` |
|        7 |  6254 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6255 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6256 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6257 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6258 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6259 | `	case PH7_OP_BAND:` |
|       60 |  6260 | `	default:          r = a&b; break;` |
|        - |  6261 | `	}` |
|        - |  6262 | `	/* Push the result */` |
|       88 |  6263 | `	pNos->x.iVal = r;` |
|       88 |  6264 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6265 | `	VmPopOperand(&pTos,1);` |
|       88 |  6266 | `	break;` |
|        - |  6267 | `				 }` |
|        - |  6268 | `/* OP_BAND_STORE * * *` |
|        - |  6269 | ` *` |
|        - |  6270 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6271 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6272 | ` * two elements.` |
|        - |  6273 | `*/` |
|        - |  6274 | `/* OP_BOR_STORE * * *` |
|        - |  6275 | ` *` |
|        - |  6276 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6277 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6278 | ` * two elements.` |
|        - |  6279 | ` */` |
|        - |  6280 | `/* OP_BXOR_STORE * * *` |
|        - |  6281 | ` *` |
|        - |  6282 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6283 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6284 | ` * two elements.` |
|        - |  6285 | ` */` |
|       10 |  6286 | `case PH7_OP_BAND_STORE:` |
|        - |  6287 | `case PH7_OP_BOR_STORE:` |
|        - |  6288 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6289 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6290 | `	ph7_value *pObj;` |
|        - |  6291 | `	sxi64 a,b,r;` |
|        - |  6292 | `#ifdef UNTRUST` |
|        - |  6293 | `	if( pNos < pStack ){` |
|        - |  6294 | `		goto Abort;` |
|        - |  6295 | `	}` |
|        - |  6296 | `#endif` |
|        - |  6297 | `	/* Force the operands to be integer */` |
|       21 |  6298 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6299 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6300 | `	}` |
|       21 |  6301 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6302 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6303 | `	}` |
|        - |  6304 | `	/* Perform the requested operation */` |
|       21 |  6305 | `	a = pTos->x.iVal;` |
|       21 |  6306 | `	b = pNos->x.iVal;` |
|       21 |  6307 | `	switch(pInstr->iOp){` |
|        3 |  6308 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6309 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6310 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6311 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6312 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6313 | `	case PH7_OP_BAND:` |
|        7 |  6314 | `	default:          r = a&b; break;` |
|        - |  6315 | `	}` |
|        - |  6316 | `	/* Push the result */` |
|       21 |  6317 | `	pNos->x.iVal = r;` |
|       21 |  6318 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6319 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6320 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6321 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6322 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6323 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6324 | `	}` |
|       21 |  6325 | `	VmPopOperand(&pTos,1);` |
|       21 |  6326 | `	break;` |
|        - |  6327 | `				 }` |
|        - |  6328 | `/* OP_SHL * * *` |
|        - |  6329 | ` *` |
|        - |  6330 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6331 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6332 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6333 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6334 | ` */` |
|        - |  6335 | `/* OP_SHR * * *` |
|        - |  6336 | ` *` |
|        - |  6337 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6338 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6339 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6340 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6341 | ` */` |
|       12 |  6342 | `case PH7_OP_SHL:` |
|        - |  6343 | `case PH7_OP_SHR: {` |
|       25 |  6344 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6345 | `	sxi64 a,r;` |
|        - |  6346 | `	sxi32 b;` |
|        - |  6347 | `#ifdef UNTRUST` |
|        - |  6348 | `	if( pNos < pStack ){` |
|        - |  6349 | `		goto Abort;` |
|        - |  6350 | `	}` |
|        - |  6351 | `#endif` |
|        - |  6352 | `	/* Force the operands to be integer */` |
|       25 |  6353 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6354 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6355 | `	}` |
|       25 |  6356 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6357 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6358 | `	}` |
|        - |  6359 | `	/* Perform the requested operation */` |
|       25 |  6360 | `	a = pNos->x.iVal;` |
|       25 |  6361 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6362 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6363 | `		r = a << b;` |
|        8 |  6364 | `	}else{` |
|       11 |  6365 | `		r = a >> b;` |
|        - |  6366 | `	}` |
|        - |  6367 | `	/* Push the result */` |
|       25 |  6368 | `	pNos->x.iVal = r;` |
|       25 |  6369 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6370 | `	VmPopOperand(&pTos,1);` |
|       25 |  6371 | `	break;` |
|        - |  6372 | `				 }` |
|        - |  6373 | `/*  OP_SHL_STORE * * *` |
|        - |  6374 | ` *` |
|        - |  6375 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6376 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6377 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6378 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6379 | ` */` |
|        - |  6380 | `/* OP_SHR_STORE * * *` |
|        - |  6381 | ` *` |
|        - |  6382 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6383 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6384 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6385 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6386 | ` */` |
|        9 |  6387 | `case PH7_OP_SHL_STORE:` |
|        - |  6388 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6389 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6390 | `	ph7_value *pObj;` |
|        - |  6391 | `	sxi64 a,r;` |
|        - |  6392 | `	sxi32 b;` |
|        - |  6393 | `#ifdef UNTRUST` |
|        - |  6394 | `	if( pNos < pStack ){` |
|        - |  6395 | `		goto Abort;` |
|        - |  6396 | `	}` |
|        - |  6397 | `#endif` |
|        - |  6398 | `	/* Force the operands to be integer */` |
|       19 |  6399 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6400 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6401 | `	}` |
|       19 |  6402 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6403 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6404 | `	}` |
|        - |  6405 | `	/* Perform the requested operation */` |
|       19 |  6406 | `	a = pTos->x.iVal;` |
|       19 |  6407 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6408 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6409 | `		r = a << b;` |
|        5 |  6410 | `	}else{` |
|       11 |  6411 | `		r = a >> b;` |
|        - |  6412 | `	}` |
|        - |  6413 | `	/* Push the result */` |
|       19 |  6414 | `	pNos->x.iVal = r;` |
|       19 |  6415 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6416 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6417 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6418 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6419 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6420 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6421 | `	}` |
|       19 |  6422 | `	VmPopOperand(&pTos,1);` |
|       19 |  6423 | `	break;` |
|        - |  6424 | `				 }` |
|        - |  6425 | `/* CAT:  P1 * *` |
|        - |  6426 | ` *` |
|        - |  6427 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6428 | ` * back.` |
|        - |  6429 | ` */` |
|    71787 |  6430 | `case PH7_OP_CAT:{` |
|        - |  6431 | `	ph7_value *pNos,*pCur;` |
|   143576 |  6432 | `	if( pInstr->iP1 < 1 ){` |
|   116096 |  6433 | `		pNos = &pTos[-1];` |
|    58049 |  6434 | `	}else{` |
|    27482 |  6435 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6436 | `	}` |
|        - |  6437 | `#ifdef UNTRUST` |
|        - |  6438 | `	if( pNos < pStack ){` |
|        - |  6439 | `		goto Abort;` |
|        - |  6440 | `	}` |
|        - |  6441 | `#endif` |
|        - |  6442 | `	/* Force a string cast */` |
|   143576 |  6443 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6444 | `		PH7_MemObjToString(pNos);` |
|      835 |  6445 | `	}` |
|   143576 |  6446 | `	pCur = &pNos[1];` |
|   289874 |  6447 | `	while( pCur <= pTos ){` |
|   146300 |  6448 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50918 |  6449 | `			PH7_MemObjToString(pCur);` |
|    25458 |  6450 | `		}` |
|        - |  6451 | `		/* Perform the concatenation */` |
|   146300 |  6452 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146256 |  6453 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6454 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6455 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6456 | `				goto Abort;` |
|        - |  6457 | `			}` |
|    73127 |  6458 | `		}` |
|   146300 |  6459 | `		SyBlobRelease(&pCur->sBlob);` |
|   146300 |  6460 | `		pCur++;` |
|        2 |  6461 | `	}` |
|   143576 |  6462 | `	pTos = pNos;` |
|   143576 |  6463 | `	break;` |
|        - |  6464 | `				}` |
|        - |  6465 | `/*  CAT_STORE: * * *` |
|        - |  6466 | ` *` |
|        - |  6467 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6468 | ` * back.` |
|        - |  6469 | ` */` |
|     4112 |  6470 | `case PH7_OP_CAT_STORE:{` |
|     8226 |  6471 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6472 | `	ph7_value *pObj;` |
|        - |  6473 | `#ifdef UNTRUST` |
|        - |  6474 | `	if( pNos < pStack ){` |
|        - |  6475 | `		goto Abort;` |
|        - |  6476 | `	}` |
|        - |  6477 | `#endif` |
|     8226 |  6478 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6479 | `		/* Force a string cast */` |
|        3 |  6480 | `		PH7_MemObjToString(pTos);` |
|        1 |  6481 | `	}` |
|     8226 |  6482 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6483 | `		/* Force a string cast */` |
|      ! 0 |  6484 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6485 | `	}` |
|        - |  6486 | `	/* Perform the concatenation (Reverse order) */` |
|     8226 |  6487 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8226 |  6488 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6489 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  6490 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  6491 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6492 | `			goto Abort;` |
|        - |  6493 | `		}` |
|     4112 |  6494 | `	}` |
|        - |  6495 | `	/* Perform the store operation */` |
|     8226 |  6496 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6497 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8226 |  6498 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8226 |  6499 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8224 |  6500 | `		PH7_MemObjStore(pTos,pObj);` |
|     4111 |  6501 | `	}` |
|     8224 |  6502 | `	PH7_MemObjStore(pTos,pNos);` |
|     8224 |  6503 | `	VmPopOperand(&pTos,1);` |
|     8224 |  6504 | `	break;` |
|        - |  6505 | `				}` |
|        - |  6506 | `/* OP_AND: * * *` |
|        - |  6507 | ` *` |
|        - |  6508 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6509 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6510 | ` * stack.` |
|        - |  6511 | ` */` |
|        - |  6512 | `/* OP_OR: * * *` |
|        - |  6513 | ` *` |
|        - |  6514 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6515 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6516 | ` * stack.` |
|        - |  6517 | ` */` |
|   108240 |  6518 | `case PH7_OP_LAND:` |
|        - |  6519 | `case PH7_OP_LOR: {` |
|   216526 |  6520 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6521 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6522 | `#ifdef UNTRUST` |
|        - |  6523 | `	if( pNos < pStack ){` |
|        - |  6524 | `		goto Abort;` |
|        - |  6525 | `	}` |
|        - |  6526 | `#endif` |
|        - |  6527 | `	/* Force a boolean cast */` |
|   216526 |  6528 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6529 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6530 | `	}` |
|   216526 |  6531 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6532 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6533 | `	}` |
|   216526 |  6534 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216526 |  6535 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216526 |  6536 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6537 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99392 |  6538 | `		v1 = and_logic[v1*3+v2];` |
|    49719 |  6539 | `	}else{` |
|        - |  6540 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117136 |  6541 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6542 | `	}` |
|   216526 |  6543 | `	if( v1 == 2 ){` |
|      ! 0 |  6544 | `		v1 = 1;` |
|      ! 0 |  6545 | `	}` |
|   216526 |  6546 | `	VmPopOperand(&pTos,1);` |
|   216526 |  6547 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216526 |  6548 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216526 |  6549 | `	break;` |
|        - |  6550 | `				 }` |
|        - |  6551 | `/*` |
|        - |  6552 | ` * OP_NULLC: * * *` |
|        - |  6553 | ` * Null coalescing operator '??'.` |
|        - |  6554 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6555 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6556 | ` */` |
|        - |  6557 | `/*` |
|        - |  6558 | ` * OP_NULLC: * P2 *` |
|        - |  6559 | ` * Short-circuit null coalescing '??'.` |
|        - |  6560 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6561 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6562 | ` */` |
|       93 |  6563 | `case PH7_OP_NULLC: {` |
|        - |  6564 | `#ifdef UNTRUST` |
|        - |  6565 | `	if( pTos < pStack ){` |
|        - |  6566 | `		goto Abort;` |
|        - |  6567 | `	}` |
|        - |  6568 | `#endif` |
|      188 |  6569 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6570 | `		/* Left is not null — keep it and skip the RHS */` |
|      114 |  6571 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       58 |  6572 | `	}else{` |
|        - |  6573 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       76 |  6574 | `		VmPopOperand(&pTos, 1);` |
|        - |  6575 | `	}` |
|      188 |  6576 | `	break;` |
|        - |  6577 |  |
|        - |  6578 | `/*` |
|        - |  6579 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6580 | ` * Null coalescing assignment short-circuit.` |
|        - |  6581 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6582 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6583 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6584 | ` */` |
|       28 |  6585 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6586 | `#ifdef UNTRUST` |
|        - |  6587 | `	if( pTos < pStack ){` |
|        - |  6588 | `		goto Abort;` |
|        - |  6589 | `	}` |
|        - |  6590 | `#endif` |
|       58 |  6591 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6592 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6593 | `	}` |
|       58 |  6594 | `	break;` |
|        - |  6595 |  |
|        - |  6596 | `/*` |
|        - |  6597 | ` * OP_NULLC_STORE: * * *` |
|        - |  6598 | ` * Null coalescing assignment store.` |
|        - |  6599 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6600 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6601 | ` * expression result.` |
|        - |  6602 | ` */` |
|        - |  6603 | `/*` |
|        - |  6604 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6605 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6606 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6607 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6608 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6609 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6610 | ` */` |
|       51 |  6611 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6612 | `#ifdef UNTRUST` |
|        - |  6613 | `	if( pTos < pStack ){` |
|        - |  6614 | `		goto Abort;` |
|        - |  6615 | `	}` |
|        - |  6616 | `#endif` |
|      104 |  6617 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6618 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6619 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6620 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6621 | `	}` |
|      104 |  6622 | `	break;` |
|        - |  6623 |  |
|       17 |  6624 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6625 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6626 | `	ph7_value *pObj;` |
|        - |  6627 | `	sxu32 nIdx;` |
|        - |  6628 | `#ifdef UNTRUST` |
|        - |  6629 | `	if( pNos < pStack ){` |
|        - |  6630 | `		goto Abort;` |
|        - |  6631 | `	}` |
|        - |  6632 | `#endif` |
|        - |  6633 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6634 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6635 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6636 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6637 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6638 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6639 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6640 | `		ph7_value *apArg[2];` |
|        5 |  6641 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6642 | `		apArg[1] = pTos;` |
|        5 |  6643 | `		if( pSet ){` |
|        5 |  6644 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6645 | `		}` |
|        - |  6646 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6647 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6648 | `		VmPopOperand(&pTos,1);` |
|        - |  6649 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6650 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6651 | `		break;` |
|        - |  6652 | `	}` |
|       32 |  6653 | `	nIdx = pNos->nIdx;` |
|       32 |  6654 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6655 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6656 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6657 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6658 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6659 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6660 | `	}` |
|       32 |  6661 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6662 | `	VmPopOperand(&pTos,1);` |
|       32 |  6663 | `	break;` |
|        - |  6664 |  |
|        - |  6665 | `/*` |
|        - |  6666 | ` * OP_SPREAD: * * *` |
|        - |  6667 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6668 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6669 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6670 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6671 | ` */` |
|        9 |  6672 | `case PH7_OP_SPREAD: {` |
|        - |  6673 | `#ifdef UNTRUST` |
|        - |  6674 | `	if( pTos < pStack ){` |
|        - |  6675 | `		goto Abort;` |
|        - |  6676 | `	}` |
|        - |  6677 | `#endif` |
|       20 |  6678 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6679 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6680 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6681 | `		if( nEntry == 0 ){` |
|        - |  6682 | `			/* Empty array — remove from stack */` |
|        3 |  6683 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6684 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6685 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6686 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6687 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6688 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6689 | `				VM_STACK_GUARD);` |
|      ! 0 |  6690 | `		}else{` |
|        - |  6691 | `			ph7_hashmap_node *pNode2;` |
|        - |  6692 | `			ph7_value *pElem;` |
|        - |  6693 | `			sxu32 i;` |
|        - |  6694 | `			/* Overwrite TOS with first element */` |
|       18 |  6695 | `			pNode2 = pMap->pFirst;` |
|       18 |  6696 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6697 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6698 | `			if( pElem ){` |
|       18 |  6699 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6700 | `			}` |
|       18 |  6701 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6702 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6703 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6704 | `			pNode2 = pNode2->pPrev;` |
|        - |  6705 | `			/* Push remaining elements */` |
|       44 |  6706 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6707 | `				pTos++;` |
|       28 |  6708 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6709 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6710 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6711 | `				if( pElem ){` |
|       28 |  6712 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6713 | `				}` |
|       28 |  6714 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6715 | `			}` |
|       18 |  6716 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6717 | `		}` |
|        9 |  6718 | `	}` |
|        - |  6719 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6720 | `	break;` |
|        - |  6721 |  |
|        - |  6722 | `/*` |
|        - |  6723 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6724 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6725 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6726 | ` */` |
|       34 |  6727 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6728 | `#ifdef UNTRUST` |
|        - |  6729 | `	if( pTos < pStack ){` |
|        - |  6730 | `		goto Abort;` |
|        - |  6731 | `	}` |
|        - |  6732 | `#endif` |
|       70 |  6733 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  6734 | `	break;` |
|        - |  6735 |  |
|        - |  6736 | `/* OP_LXOR: * * *` |
|        - |  6737 | ` *` |
|        - |  6738 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6739 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6740 | ` * stack.` |
|        - |  6741 | ` * According to the PHP language reference manual:` |
|        - |  6742 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6743 | ` *  TRUE,but not both.` |
|        - |  6744 | ` */` |
|        5 |  6745 | `case PH7_OP_LXOR:{` |
|       11 |  6746 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6747 | `	sxi32 v = 0;` |
|        - |  6748 | `#ifdef UNTRUST` |
|        - |  6749 | `	if( pNos < pStack ){` |
|        - |  6750 | `		goto Abort;` |
|        - |  6751 | `	}` |
|        - |  6752 | `#endif` |
|        - |  6753 | `	/* Force a boolean cast */` |
|       11 |  6754 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6755 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6756 | `	}` |
|       11 |  6757 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6758 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6759 | `	}` |
|       11 |  6760 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6761 | `		v = 1;` |
|        3 |  6762 | `	}` |
|       11 |  6763 | `	VmPopOperand(&pTos,1);` |
|       11 |  6764 | `	pTos->x.iVal = v;` |
|       11 |  6765 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6766 | `	break;` |
|        - |  6767 | `				 }` |
|        - |  6768 | `/* OP_EQ P1 P2 P3` |
|        - |  6769 | ` *` |
|        - |  6770 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6771 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6772 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6773 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6774 | ` */` |
|        - |  6775 | `/* OP_NEQ P1 P2 P3` |
|        - |  6776 | ` *` |
|        - |  6777 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6778 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6779 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6780 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6781 | ` */` |
|     4581 |  6782 | `case PH7_OP_EQ:` |
|        - |  6783 | `case PH7_OP_NEQ: {` |
|     9164 |  6784 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6785 | `	/* Perform the comparison and act accordingly */` |
|        - |  6786 | `#ifdef UNTRUST` |
|        - |  6787 | `	if( pNos < pStack ){` |
|        - |  6788 | `		goto Abort;` |
|        - |  6789 | `	}` |
|        - |  6790 | `#endif` |
|     9164 |  6791 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9164 |  6792 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6793 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9155 |  6794 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9120 |  6795 | `		rc = rc == 0;` |
|     4561 |  6796 | `	}else{` |
|       28 |  6797 | `		rc = rc != 0;` |
|        - |  6798 | `	}` |
|     9164 |  6799 | `	VmPopOperand(&pTos,1);` |
|     9164 |  6800 | `	if( !pInstr->iP2 ){` |
|        - |  6801 | `		/* Push comparison result without taking the jump */` |
|     9164 |  6802 | `		PH7_MemObjRelease(pTos);` |
|     9164 |  6803 | `		pTos->x.iVal = rc;` |
|        - |  6804 | `		/* Invalidate any prior representation */` |
|     9164 |  6805 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4583 |  6806 | `	}else{` |
|      ! 0 |  6807 | `		if( rc ){` |
|        - |  6808 | `			/* Jump to the desired location */` |
|      ! 0 |  6809 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6810 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6811 | `		}` |
|        - |  6812 | `	}` |
|     9164 |  6813 | `	break;` |
|        - |  6814 | `				 }` |
|        - |  6815 | `/* OP_TEQ P1 P2 *` |
|        - |  6816 | ` *` |
|        - |  6817 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6818 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6819 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6820 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6821 | ` */` |
|   161481 |  6822 | `case PH7_OP_TEQ: {` |
|   322964 |  6823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6824 | `	/* Perform the comparison and act accordingly */` |
|        - |  6825 | `#ifdef UNTRUST` |
|        - |  6826 | `	if( pNos < pStack ){` |
|        - |  6827 | `		goto Abort;` |
|        - |  6828 | `	}` |
|        - |  6829 | `#endif` |
|   322964 |  6830 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   322964 |  6831 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6832 | `		rc = 0;` |
|        2 |  6833 | `	}else{` |
|   322962 |  6834 | `		rc = rc == 0;` |
|        - |  6835 | `	}` |
|   322964 |  6836 | `	VmPopOperand(&pTos,1);` |
|   322964 |  6837 | `	if( !pInstr->iP2 ){` |
|        - |  6838 | `		/* Push comparison result without taking the jump */` |
|   322964 |  6839 | `		PH7_MemObjRelease(pTos);` |
|   322964 |  6840 | `		pTos->x.iVal = rc;` |
|        - |  6841 | `		/* Invalidate any prior representation */` |
|   322964 |  6842 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   161483 |  6843 | `	}else{` |
|      ! 0 |  6844 | `		if( rc ){` |
|        - |  6845 | `			/* Jump to the desired location */` |
|      ! 0 |  6846 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6847 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6848 | `		}` |
|        - |  6849 | `	}` |
|   322964 |  6850 | `	break;` |
|        - |  6851 | `				 }` |
|        - |  6852 | `/* OP_TNE P1 P2 *` |
|        - |  6853 | ` *` |
|        - |  6854 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6855 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6856 | ` * instruction.` |
|        - |  6857 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6858 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6859 | ` *` |
|        - |  6860 | ` */` |
|   124230 |  6861 | `case PH7_OP_TNE: {` |
|   248462 |  6862 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6863 | `	/* Perform the comparison and act accordingly */` |
|        - |  6864 | `#ifdef UNTRUST` |
|        - |  6865 | `	if( pNos < pStack ){` |
|        - |  6866 | `		goto Abort;` |
|        - |  6867 | `	}` |
|        - |  6868 | `#endif` |
|   248462 |  6869 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   248462 |  6870 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6871 | `		rc = 1;` |
|        2 |  6872 | `	}else{` |
|   248460 |  6873 | `		rc = rc != 0;` |
|        - |  6874 | `	}` |
|   248462 |  6875 | `	VmPopOperand(&pTos,1);` |
|   248462 |  6876 | `	if( !pInstr->iP2 ){` |
|        - |  6877 | `		/* Push comparison result without taking the jump */` |
|   248462 |  6878 | `		PH7_MemObjRelease(pTos);` |
|   248462 |  6879 | `		pTos->x.iVal = rc;` |
|        - |  6880 | `		/* Invalidate any prior representation */` |
|   248462 |  6881 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124232 |  6882 | `	}else{` |
|      ! 0 |  6883 | `		if( rc ){` |
|        - |  6884 | `			/* Jump to the desired location */` |
|      ! 0 |  6885 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6886 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6887 | `		}` |
|        - |  6888 | `	}` |
|   248462 |  6889 | `	break;` |
|        - |  6890 | `				 }` |
|        - |  6891 | `/* OP_LT P1 P2 P3` |
|        - |  6892 | ` *` |
|        - |  6893 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6894 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6895 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6896 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6897 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6898 | ` *` |
|        - |  6899 | ` */` |
|        - |  6900 | `/* OP_LE P1 P2 P3` |
|        - |  6901 | ` *` |
|        - |  6902 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6903 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6904 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6905 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6906 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6907 | ` *` |
|        - |  6908 | ` */` |
|   112423 |  6909 | `case PH7_OP_LT:` |
|        - |  6910 | `case PH7_OP_LE: {` |
|   224892 |  6911 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6912 | `	/* Perform the comparison and act accordingly */` |
|        - |  6913 | `#ifdef UNTRUST` |
|        - |  6914 | `	if( pNos < pStack ){` |
|        - |  6915 | `		goto Abort;` |
|        - |  6916 | `	}` |
|        - |  6917 | `#endif` |
|   224892 |  6918 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224892 |  6919 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6920 | `		rc = 0;` |
|   224888 |  6921 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  6922 | `		rc = rc < 1;` |
|      805 |  6923 | `	}else{` |
|   223278 |  6924 | `		rc = rc < 0;` |
|        - |  6925 | `	}` |
|   224892 |  6926 | `	VmPopOperand(&pTos,1);` |
|   224892 |  6927 | `	if( !pInstr->iP2 ){` |
|        - |  6928 | `		/* Push comparison result without taking the jump */` |
|   224892 |  6929 | `		PH7_MemObjRelease(pTos);` |
|   224892 |  6930 | `		pTos->x.iVal = rc;` |
|        - |  6931 | `		/* Invalidate any prior representation */` |
|   224892 |  6932 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112469 |  6933 | `	}else{` |
|      ! 0 |  6934 | `		if( rc ){` |
|        - |  6935 | `			/* Jump to the desired location */` |
|      ! 0 |  6936 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6937 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6938 | `		}` |
|        - |  6939 | `	}` |
|   224892 |  6940 | `	break;` |
|        - |  6941 | `				}` |
|        - |  6942 | `/* OP_GT P1 P2 P3` |
|        - |  6943 | ` *` |
|        - |  6944 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6945 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6946 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6947 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6948 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6949 | ` *` |
|        - |  6950 | ` */` |
|        - |  6951 | `/* OP_GE P1 P2 P3` |
|        - |  6952 | ` *` |
|        - |  6953 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6954 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6955 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6956 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6957 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6958 | ` *` |
|        - |  6959 | ` */` |
|    55654 |  6960 | `case PH7_OP_GT:` |
|        - |  6961 | `case PH7_OP_GE: {` |
|   111310 |  6962 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6963 | `	/* Perform the comparison and act accordingly */` |
|        - |  6964 | `#ifdef UNTRUST` |
|        - |  6965 | `	if( pNos < pStack ){` |
|        - |  6966 | `		goto Abort;` |
|        - |  6967 | `	}` |
|        - |  6968 | `#endif` |
|   111310 |  6969 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111310 |  6970 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6971 | `		rc = 0;` |
|   111306 |  6972 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110878 |  6973 | `		rc = rc >= 0;` |
|    55440 |  6974 | `	}else{` |
|      426 |  6975 | `		rc = rc > 0;` |
|        - |  6976 | `	}` |
|   111310 |  6977 | `	VmPopOperand(&pTos,1);` |
|   111310 |  6978 | `	if( !pInstr->iP2 ){` |
|        - |  6979 | `		/* Push comparison result without taking the jump */` |
|   111310 |  6980 | `		PH7_MemObjRelease(pTos);` |
|   111310 |  6981 | `		pTos->x.iVal = rc;` |
|        - |  6982 | `		/* Invalidate any prior representation */` |
|   111310 |  6983 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55656 |  6984 | `	}else{` |
|      ! 0 |  6985 | `		if( rc ){` |
|        - |  6986 | `			/* Jump to the desired location */` |
|      ! 0 |  6987 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6988 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6989 | `		}` |
|        - |  6990 | `	}` |
|   111310 |  6991 | `	break;` |
|        - |  6992 | `				}` |
|        - |  6993 | `/* OP_SPACESHIP * * *` |
|        - |  6994 | ` *` |
|        - |  6995 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6996 | ` *   -1 if left < right` |
|        - |  6997 | ` *    0 if left == right` |
|        - |  6998 | ` *    1 if left > right` |
|        - |  6999 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7000 | ` */` |
|       25 |  7001 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7002 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7003 | `#ifdef UNTRUST` |
|        - |  7004 | `	if( pNos < pStack ){` |
|        - |  7005 | `		goto Abort;` |
|        - |  7006 | `	}` |
|        - |  7007 | `#endif` |
|       51 |  7008 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7009 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7010 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7011 | `		rc = 1;` |
|        4 |  7012 | `	}else{` |
|        - |  7013 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7014 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7015 | `	}` |
|       51 |  7016 | `	VmPopOperand(&pTos,1);` |
|       51 |  7017 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7018 | `	pTos->x.iVal = rc;` |
|       51 |  7019 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7020 | `	break;` |
|        - |  7021 | `				}` |
|        - |  7022 | `/* OP_SEQ P1 P2 *` |
|        - |  7023 | ` * Strict string comparison.` |
|        - |  7024 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7025 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7026 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7027 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7028 | ` * use PH7_OP_EQ.` |
|        - |  7029 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7030 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7031 | ` */` |
|        - |  7032 | `/* OP_SNE P1 P2 *` |
|        - |  7033 | ` * Strict string comparison.` |
|        - |  7034 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7035 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7036 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7037 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7038 | ` * use PH7_OP_EQ.` |
|        - |  7039 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7040 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7041 | ` */` |
|       18 |  7042 | `case PH7_OP_SEQ:` |
|        - |  7043 | `case PH7_OP_SNE: {` |
|       38 |  7044 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7045 | `	SyString s1,s2;` |
|        - |  7046 | `	/* Perform the comparison and act accordingly */` |
|        - |  7047 | `#ifdef UNTRUST` |
|        - |  7048 | `	if( pNos < pStack ){` |
|        - |  7049 | `		goto Abort;` |
|        - |  7050 | `	}` |
|        - |  7051 | `#endif` |
|        - |  7052 | `	/* Force a string cast */` |
|       38 |  7053 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7054 | `		PH7_MemObjToString(pTos);` |
|        2 |  7055 | `	}` |
|       38 |  7056 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7057 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7058 | `	}` |
|       38 |  7059 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7060 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7061 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7062 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7063 | `		rc = rc != 0;` |
|      ! 0 |  7064 | `	}else{` |
|       38 |  7065 | `		rc = rc == 0;` |
|        - |  7066 | `	}` |
|       38 |  7067 | `	VmPopOperand(&pTos,1);` |
|       38 |  7068 | `	if( !pInstr->iP2 ){` |
|        - |  7069 | `		/* Push comparison result without taking the jump */` |
|       38 |  7070 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7071 | `		pTos->x.iVal = rc;` |
|        - |  7072 | `		/* Invalidate any prior representation */` |
|       38 |  7073 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7074 | `	}else{` |
|      ! 0 |  7075 | `		if( rc ){` |
|        - |  7076 | `			/* Jump to the desired location */` |
|      ! 0 |  7077 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7078 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7079 | `		}` |
|        - |  7080 | `	}` |
|       38 |  7081 | `	break;` |
|        - |  7082 | `				 }` |
|        - |  7083 | `/*` |
|        - |  7084 | ` * OP_LOAD_REF * * *` |
|        - |  7085 | ` * Push the index of a referenced object on the stack.` |
|        - |  7086 | ` */` |
|       60 |  7087 | `case PH7_OP_LOAD_REF: {` |
|        - |  7088 | `	sxu32 nIdx;` |
|        - |  7089 | `#ifdef UNTRUST` |
|        - |  7090 | `	if( pTos < pStack ){` |
|        - |  7091 | `		goto Abort;` |
|        - |  7092 | `	}` |
|        - |  7093 | `#endif` |
|        - |  7094 | `	/* Extract memory object index */` |
|      121 |  7095 | `	nIdx = pTos->nIdx;` |
|      121 |  7096 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7097 | `		/* Nullify the object */` |
|      101 |  7098 | `		PH7_MemObjRelease(pTos);` |
|        - |  7099 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7100 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7101 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7102 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7103 | `	}` |
|      121 |  7104 | `	break;` |
|        - |  7105 | `					  }` |
|        - |  7106 | `/*` |
|        - |  7107 | ` * OP_STORE_REF * * P3` |
|        - |  7108 | ` * Perform an assignment operation by reference.` |
|        - |  7109 | ` */` |
|       16 |  7110 | ` case PH7_OP_STORE_REF: {` |
|       34 |  7111 | `	 SyString sName = { 0 , 0 };` |
|        - |  7112 | `	 VmFrame *pFrameLocal;` |
|        - |  7113 | `	SyHashEntry *pEntry;` |
|        - |  7114 | `	sxu32 nIdx;` |
|        - |  7115 | `#ifdef UNTRUST` |
|        - |  7116 | `	if( pTos < pStack ){` |
|        - |  7117 | `		goto Abort;` |
|        - |  7118 | `	}` |
|        - |  7119 | `#endif` |
|       34 |  7120 | `	if( pInstr->p3 == 0 ){` |
|        - |  7121 | `		char *zName;` |
|        - |  7122 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7123 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7124 | `			/* Force a string cast */` |
|      ! 0 |  7125 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7126 | `		}` |
|      ! 0 |  7127 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7128 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7129 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7130 | `			if( zName ){` |
|      ! 0 |  7131 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7132 | `			}` |
|      ! 0 |  7133 | `		}` |
|      ! 0 |  7134 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7135 | `		pTos--;` |
|      ! 0 |  7136 | `	}else{` |
|       34 |  7137 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7138 | `	}` |
|       34 |  7139 | `	nIdx = pTos->nIdx;` |
|       34 |  7140 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7141 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7142 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7143 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7144 | `		}else{` |
|        - |  7145 | `			ph7_value *pObj;` |
|        - |  7146 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7147 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7148 | `			if( pObj == 0 ){` |
|      ! 0 |  7149 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7150 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7151 | `				goto Abort;` |
|        - |  7152 | `			}` |
|        - |  7153 | `			/* Perform the store operation */` |
|      ! 0 |  7154 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7155 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7156 | `		}` |
|       34 |  7157 | `	}else if( sName.nByte > 0){` |
|       34 |  7158 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7159 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7160 | `		}else{` |
|       34 |  7161 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7162 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7163 | `			/* Query the local frame */` |
|       34 |  7164 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7165 | `			if( pEntry ){` |
|      ! 0 |  7166 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7167 | `			}else{` |
|       34 |  7168 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7169 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7170 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7171 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7172 | `				}` |
|       34 |  7173 | `				if( rc == SXRET_OK ){` |
|       34 |  7174 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7175 | `				}` |
|        - |  7176 | `			}` |
|        - |  7177 | `		}` |
|       16 |  7178 | `	}` |
|       34 |  7179 | `	break;` |
|        - |  7180 | `				 }` |
|        - |  7181 | `/*` |
|        - |  7182 | ` * OP_UPLINK P1 * *` |
|        - |  7183 | ` * Link a variable to the top active VM frame.` |
|        - |  7184 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7185 | ` */` |
|       30 |  7186 | `case PH7_OP_UPLINK: {` |
|       62 |  7187 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7188 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7189 | `		SyString sName;` |
|        - |  7190 | `		/* Perform the link */` |
|      132 |  7191 | `		while( pLink <= pTos ){` |
|       72 |  7192 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7193 | `				/* Force a string cast */` |
|      ! 0 |  7194 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7195 | `			}` |
|       72 |  7196 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7197 | `			if( sName.nByte > 0 ){` |
|       72 |  7198 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7199 | `			}` |
|       72 |  7200 | `			pLink++;` |
|        2 |  7201 | `		}` |
|       30 |  7202 | `	}` |
|       62 |  7203 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7204 | `	break;` |
|        - |  7205 | `					}` |
|        - |  7206 | `/*` |
|        - |  7207 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7208 | ` * Push an exception in the corresponding container so that` |
|        - |  7209 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7210 | ` */` |
|      183 |  7211 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      368 |  7212 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7213 | `	VmFrame *pFrameLocal;` |
|        - |  7214 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      368 |  7215 | `	pException->iFinallyDone = 0;` |
|      368 |  7216 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7217 | `	/* Create the exception frame */` |
|      368 |  7218 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      368 |  7219 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7220 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7221 | `		goto Abort;` |
|        - |  7222 | `	}` |
|        - |  7223 | `	/* Mark the special frame */` |
|      368 |  7224 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      368 |  7225 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7226 | `	/* Point to the frame that trigger the exception */` |
|      368 |  7227 | `	pFrameLocal = pFrameLocal->pParent;` |
|      368 |  7228 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      368 |  7229 | `	pException->pFrame = pFrameLocal;` |
|      368 |  7230 | `	break;` |
|        - |  7231 | `							}` |
|        - |  7232 | `/*` |
|        - |  7233 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7234 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7235 | ` */` |
|      182 |  7236 | `case PH7_OP_POP_EXCEPTION: {` |
|      366 |  7237 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      366 |  7238 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7239 | `		ph7_exception **apException;` |
|        - |  7240 | `		/* Pop the loaded exception */` |
|       32 |  7241 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7242 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7243 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7244 | `		}` |
|       15 |  7245 | `	}` |
|      366 |  7246 | `	pException->pFrame = 0;` |
|        - |  7247 | `	/* Leave the exception frame */` |
|      366 |  7248 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7249 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      366 |  7250 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7251 | `		sxi32 rcFinally;` |
|       20 |  7252 | `		pException->iFinallyDone = 1;` |
|       20 |  7253 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7254 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7255 | `			goto Abort;` |
|        - |  7256 | `		}` |
|        9 |  7257 | `	}` |
|      366 |  7258 | `	break;` |
|        - |  7259 | `							}` |
|        - |  7260 |  |
|        - |  7261 | `/*` |
|        - |  7262 | ` * OP_THROW * P2 *` |
|        - |  7263 | ` * Throw an user exception.` |
|        - |  7264 | ` */` |
|       78 |  7265 | `case PH7_OP_THROW: {` |
|      158 |  7266 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      158 |  7267 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7268 | `#ifdef UNTRUST` |
|        - |  7269 | `	if( pTos < pStack ){` |
|        - |  7270 | `		goto Abort;` |
|        - |  7271 | `	}` |
|        - |  7272 | `#endif` |
|      158 |  7273 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7274 | `	/* Tell the upper layer that an exception was thrown */` |
|      158 |  7275 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      158 |  7276 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      158 |  7277 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7278 | `		ph7_class *pThrowable;` |
|        - |  7279 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      158 |  7280 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      159 |  7281 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7282 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7283 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7284 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7285 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7286 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7287 | `			if( pErrorClass ){` |
|        3 |  7288 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7289 | `			}` |
|        3 |  7290 | `			if( pErrInst ){` |
|        - |  7291 | `				ph7_class_method *pCons;` |
|        3 |  7292 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7293 | `				if( pCons ){` |
|        - |  7294 | `					ph7_value sArg;` |
|        - |  7295 | `					ph7_value *apArg[1];` |
|        - |  7296 | `					SyString sMsgStr;` |
|        - |  7297 | `					static const char zErrMsg[] =` |
|        - |  7298 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7299 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7300 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7301 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7302 | `					apArg[0] = &sArg;` |
|        3 |  7303 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7304 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7305 | `				}` |
|        3 |  7306 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7307 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7308 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7309 | `					goto Abort;` |
|        - |  7310 | `				}` |
|        2 |  7311 | `			}else{` |
|        - |  7312 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7313 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7314 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7315 | `					goto Abort;` |
|        - |  7316 | `				}` |
|        - |  7317 | `			}` |
|        2 |  7318 | `		}else{` |
|        - |  7319 | `			/* Throw the exception */` |
|      156 |  7320 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      156 |  7321 | `			if( rc == SXERR_ABORT ){` |
|        - |  7322 | `				/* Abort processing immediately */` |
|       11 |  7323 | `				goto Abort;` |
|        - |  7324 | `			}` |
|        - |  7325 | `		}` |
|       75 |  7326 | `	}else{` |
|        - |  7327 | `		/* Expecting a class instance */` |
|      ! 0 |  7328 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7329 | `		if( rc == SXERR_ABORT ){` |
|        - |  7330 | `			/* Abort processing immediately */` |
|      ! 0 |  7331 | `			goto Abort;` |
|        - |  7332 | `		}` |
|        - |  7333 | `	}` |
|        - |  7334 | `	/* Pop the top entry */` |
|      148 |  7335 | `	VmPopOperand(&pTos,1);` |
|        - |  7336 | `	/* Perform an unconditional jump */` |
|      148 |  7337 | `	pc = nJump - 1;` |
|      148 |  7338 | `	break;` |
|        - |  7339 | `				   }` |
|        - |  7340 | `/*` |
|        - |  7341 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7342 | ` * Prepare a foreach step.` |
|        - |  7343 | ` */` |
|     6176 |  7344 | `case PH7_OP_FOREACH_INIT: {` |
|    12354 |  7345 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7346 | `	void *pName;` |
|        - |  7347 | `#ifdef UNTRUST` |
|        - |  7348 | `	if( pTos < pStack ){` |
|        - |  7349 | `		goto Abort;` |
|        - |  7350 | `	}` |
|        - |  7351 | `#endif` |
|    12354 |  7352 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7353 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7354 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7355 | `			/* Force a string cast */` |
|      ! 0 |  7356 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7357 | `		}` |
|        - |  7358 | `		/* Duplicate name */` |
|      ! 0 |  7359 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7360 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7361 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7362 | `		}` |
|      ! 0 |  7363 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7364 | `	}` |
|    12354 |  7365 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7366 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7367 | `			/* Force a string cast */` |
|      ! 0 |  7368 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7369 | `		}` |
|        - |  7370 | `		/* Duplicate name */` |
|      ! 0 |  7371 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7372 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7373 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7374 | `		}` |
|      ! 0 |  7375 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7376 | `	}` |
|        - |  7377 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12354 |  7378 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7379 | `		/* Jump out of the loop */` |
|      ! 0 |  7380 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7381 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7382 | `		}` |
|      ! 0 |  7383 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7384 | `	}else{` |
|        - |  7385 | `		ph7_foreach_step *pStep;` |
|    12354 |  7386 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12354 |  7387 | `		if( pStep == 0 ){` |
|      ! 0 |  7388 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7389 | `			/* Jump out of the loop */` |
|      ! 0 |  7390 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7391 | `		}else{` |
|        - |  7392 | `			/* Zero the structure */` |
|    12354 |  7393 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7394 | `			/* Prepare the step */` |
|    12354 |  7395 | `			pStep->iFlags = pInfo->iFlags;` |
|    12354 |  7396 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7397 | `				ph7_hashmap *pMap;` |
|        - |  7398 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7399 | `				 * source array so mutations don't affect other sharers. */` |
|    12320 |  7400 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7401 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7402 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7403 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7404 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7405 | `						 * variable still points at the same hashmap as` |
|        - |  7406 | `						 * the stack value. */` |
|        9 |  7407 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7408 | `							pCur->iRef--;` |
|        9 |  7409 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7410 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7411 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7412 | `						}` |
|        4 |  7413 | `					}` |
|        4 |  7414 | `				}` |
|    12320 |  7415 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7416 | `				/* Reset the internal loop cursor */` |
|    12320 |  7417 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7418 | `				/* Mark the step */` |
|    12320 |  7419 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12320 |  7420 | `				pStep->xIter.pMap = pMap;` |
|    12320 |  7421 | `				pMap->iRef++;` |
|     6161 |  7422 | `			}else{` |
|       36 |  7423 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7424 | `				ph7_class *pIteratorClass;` |
|        - |  7425 | `				/* Check if the object implements Iterator */` |
|       36 |  7426 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7427 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7428 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7429 | `					ph7_class_method *pRewind;` |
|       24 |  7430 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7431 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7432 | `					pThis->iRef++;` |
|       24 |  7433 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7434 | `					if( pRewind ){` |
|       24 |  7435 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7436 | `					}` |
|       13 |  7437 | `				}else{` |
|        - |  7438 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7439 | `					ph7_class *pIterAggClass;` |
|       14 |  7440 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7441 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7442 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7443 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7444 | `						ph7_class_method *pGetIter;` |
|        3 |  7445 | `						int iterAggOk = 0;` |
|        3 |  7446 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7447 | `						if( pGetIter ){` |
|        - |  7448 | `							ph7_value sResult;` |
|        3 |  7449 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7450 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7451 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7452 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7453 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7454 | `									ph7_class_method *pRewind;` |
|        3 |  7455 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7456 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7457 | `									pIterObj->iRef++;` |
|        - |  7458 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7459 | `									pStep->pOwner = pThis;` |
|        3 |  7460 | `									pThis->iRef++;` |
|        3 |  7461 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7462 | `									if( pRewind ){` |
|        3 |  7463 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7464 | `									}` |
|        3 |  7465 | `									iterAggOk = 1;` |
|        1 |  7466 | `								}` |
|        1 |  7467 | `							}` |
|        3 |  7468 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7469 | `						}` |
|        3 |  7470 | `						if( !iterAggOk ){` |
|        - |  7471 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7472 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7473 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7474 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7475 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7476 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7477 | `						}` |
|        2 |  7478 | `					}else{` |
|        - |  7479 | `						/* Plain object iteration via hAttr */` |
|       12 |  7480 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7481 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7482 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7483 | `						pThis->iRef++;` |
|        - |  7484 | `					}` |
|        - |  7485 | `				}` |
|        - |  7486 | `			}` |
|        - |  7487 | `		}` |
|    12354 |  7488 | `		if( pStep ){` |
|    12354 |  7489 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7490 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7491 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7492 | `				/* Jump out of the loop */` |
|      ! 0 |  7493 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7494 | `			}` |
|     6176 |  7495 | `		}` |
|        - |  7496 | `	}` |
|    12354 |  7497 | `	VmPopOperand(&pTos,1);` |
|    12354 |  7498 | `	break;` |
|        - |  7499 | `						  }` |
|        - |  7500 | `/*` |
|        - |  7501 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7502 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7503 | ` */` |
|   101391 |  7504 | `case PH7_OP_FOREACH_STEP: {` |
|   202784 |  7505 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7506 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7507 | `	ph7_value *pValue;` |
|        - |  7508 | `	VmFrame *pFrameLocal;` |
|        - |  7509 | `	/* Peek the last step */` |
|   202784 |  7510 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   202784 |  7511 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   202784 |  7512 | `	pFrameLocal = pVm->pFrame;` |
|   202784 |  7513 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   202784 |  7514 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   202650 |  7515 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7516 | `		ph7_hashmap_node *pNode;` |
|        - |  7517 | `		/* Extract the current node value */` |
|   202650 |  7518 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   202650 |  7519 | `		if( pNode == 0 ){` |
|        - |  7520 | `			/* No more entry to process */` |
|    12318 |  7521 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12318 |  7522 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7523 | `				/* Break the reference with the last element */` |
|        7 |  7524 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7525 | `			}` |
|        - |  7526 | `			/* Automatically reset the loop cursor */` |
|    12318 |  7527 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7528 | `			/* Cleanup the mess left behind */` |
|    12318 |  7529 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12318 |  7530 | `			SySetPop(&pInfo->aStep);` |
|    12318 |  7531 | `			PH7_HashmapUnref(pMap);` |
|     6160 |  7532 | `		}else{` |
|   190334 |  7533 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7534 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7535 | `				if( pKey ){` |
|      528 |  7536 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7537 | `				}` |
|      263 |  7538 | `			}` |
|   190334 |  7539 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7540 | `				SyHashEntry *pEntry;` |
|        - |  7541 | `				/* Pass by reference */` |
|       23 |  7542 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7543 | `				if( pEntry ){` |
|       21 |  7544 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7545 | `				}else{` |
|        4 |  7546 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7547 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7548 | `				}` |
|       12 |  7549 | `			}else{` |
|        - |  7550 | `				/* Make a copy of the entry value */` |
|   190312 |  7551 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   190312 |  7552 | `				if( pValue ){` |
|   190312 |  7553 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95155 |  7554 | `				}` |
|        - |  7555 | `			}` |
|        2 |  7556 | `		}` |
|   101460 |  7557 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7558 | `		/* Iterator-based iteration.` |
|        - |  7559 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7560 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7561 | `		 */` |
|      106 |  7562 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7563 | `		ph7_class_method *pMethod;` |
|        - |  7564 | `		ph7_value sResult;` |
|      106 |  7565 | `		int isValid = 0;` |
|        - |  7566 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7567 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7568 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7569 | `		}else{` |
|       82 |  7570 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7571 | `			if( pMethod ){` |
|       82 |  7572 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7573 | `			}` |
|        - |  7574 | `		}` |
|        - |  7575 | `		/* Call valid() */` |
|      106 |  7576 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7577 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7578 | `		if( pMethod ){` |
|      106 |  7579 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7580 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7581 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7582 | `		}` |
|      106 |  7583 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7584 | `		if( !isValid ){` |
|        - |  7585 | `			/* Iterator exhausted */` |
|       24 |  7586 | `			pc = pInstr->iP2 - 1;` |
|        - |  7587 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7588 | `			if( pStep->pOwner ){` |
|        3 |  7589 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7590 | `			}` |
|       24 |  7591 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7592 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7593 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7594 | `		}else{` |
|        - |  7595 | `			/* Call current() to get value */` |
|       84 |  7596 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7597 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7598 | `			if( pMethod ){` |
|       84 |  7599 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7600 | `			}` |
|       84 |  7601 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7602 | `			if( pValue ){` |
|       84 |  7603 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7604 | `			}` |
|       84 |  7605 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7606 | `			/* Call key() if needed */` |
|       84 |  7607 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7608 | `				ph7_value sKey;` |
|       35 |  7609 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7610 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7611 | `				if( pMethod ){` |
|       35 |  7612 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7613 | `				}` |
|       35 |  7614 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7615 | `				if( pValue ){` |
|       35 |  7616 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7617 | `				}` |
|       35 |  7618 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7619 | `			}` |
|        - |  7620 | `		}` |
|       54 |  7621 | `	}else{` |
|       32 |  7622 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7623 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7624 | `		SyHashEntry *pEntry;` |
|        - |  7625 | `		/* Point to the next attribute */` |
|       36 |  7626 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7627 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7628 | `			/* Check access permission */` |
|       38 |  7629 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7630 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7631 | `					break; /* Access is granted */` |
|        - |  7632 | `			}` |
|        1 |  7633 | `		}` |
|       32 |  7634 | `		if( pEntry == 0 ){` |
|        - |  7635 | `			/* Clean up the mess left behind */` |
|       12 |  7636 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7637 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7638 | `				/* Break the reference with the last element */` |
|        3 |  7639 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7640 | `			}` |
|       12 |  7641 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7642 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7643 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7644 | `		}else{` |
|       22 |  7645 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7646 | `			ph7_value *pAttrValue;` |
|       22 |  7647 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7648 | `				/* Fill with the current attribute name */` |
|       22 |  7649 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7650 | `				if( pKey ){` |
|       22 |  7651 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7652 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7653 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7654 | `				}` |
|       10 |  7655 | `			}` |
|        - |  7656 | `			/* Extract attribute value */` |
|       22 |  7657 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7658 | `			if( pAttrValue ){` |
|       22 |  7659 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7660 | `					/* Pass by reference */` |
|        3 |  7661 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7662 | `					if( pEntry ){` |
|        3 |  7663 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7664 | `					}else{` |
|      ! 0 |  7665 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7666 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7667 | `					}` |
|        2 |  7668 | `				}else{` |
|        - |  7669 | `					/* Make a copy of the attribute value */` |
|       20 |  7670 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7671 | `					if( pValue ){` |
|       20 |  7672 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7673 | `					}` |
|        - |  7674 | `				}` |
|       10 |  7675 | `			}` |
|        - |  7676 | `		}` |
|        - |  7677 | `	}` |
|   202784 |  7678 | `	break;` |
|        - |  7679 | `						  }` |
|        - |  7680 | `/*` |
|        - |  7681 | ` * OP_MEMBER P1 P2` |
|        - |  7682 | ` * Load class attribute/method on the stack.` |
|        - |  7683 | ` */` |
|     4035 |  7684 | `case PH7_OP_MEMBER: {` |
|        - |  7685 | `	ph7_class_instance *pThis;` |
|        - |  7686 | `	ph7_value *pNos;` |
|        - |  7687 | `	SyString sName;` |
|     8072 |  7688 | `	if( !pInstr->iP1 ){` |
|     7844 |  7689 | `		pNos = &pTos[-1];` |
|        - |  7690 | `#ifdef UNTRUST` |
|        - |  7691 | `		if( pNos < pStack ){` |
|        - |  7692 | `			goto Abort;` |
|        - |  7693 | `		}` |
|        - |  7694 | `#endif` |
|     7844 |  7695 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7696 | `			ph7_class *pClass;` |
|        - |  7697 | `			/* Class already instantiated */` |
|     7842 |  7698 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7699 | `			/* Point to the instantiated class */` |
|     7842 |  7700 | `			pClass = pThis->pClass;` |
|        - |  7701 | `			/* Extract attribute name first */` |
|     7842 |  7702 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7842 |  7703 | `			if( pInstr->iP2 ){` |
|        - |  7704 | `				/* Method call */` |
|      786 |  7705 | `				ph7_class_method *pMeth = 0;` |
|      786 |  7706 | `				if( sName.nByte > 0 ){` |
|        - |  7707 | `					/* Extract the target method */` |
|      786 |  7708 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      392 |  7709 | `				}` |
|      786 |  7710 | `				if( pMeth == 0 ){` |
|      ! 0 |  7711 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7712 | `						&pClass->sName,&sName` |
|        - |  7713 | `						);` |
|        - |  7714 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7715 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7716 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7717 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7718 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7719 | `				}else{` |
|        - |  7720 | `					/* Push method name on the stack */` |
|      786 |  7721 | `					PH7_MemObjRelease(pTos);` |
|      786 |  7722 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      786 |  7723 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7724 | `				}` |
|      786 |  7725 | `				pTos->nIdx = SXU32_HIGH;` |
|      394 |  7726 | `			}else{` |
|        - |  7727 | `				/* Attribute access */` |
|     7058 |  7728 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7729 | `				SyHashEntry *pEntry;` |
|        - |  7730 | `				/* Extract the target attribute */` |
|     7058 |  7731 | `				if( sName.nByte > 0 ){` |
|     7058 |  7732 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7058 |  7733 | `					if( pEntry ){` |
|        - |  7734 | `						/* Point to the attribute value */` |
|     7056 |  7735 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3527 |  7736 | `					}` |
|     3528 |  7737 | `				}` |
|     7058 |  7738 | `				if( pObjAttr == 0 ){` |
|        - |  7739 | `					/* No such attribute,load null */` |
|        4 |  7740 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7741 | `						&pClass->sName,&sName);` |
|        - |  7742 | `					/* Call the __get magic method if available */` |
|        3 |  7743 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7744 | `				}` |
|     7058 |  7745 | `				VmPopOperand(&pTos,1);` |
|        - |  7746 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7747 | `				 * This is due to the following case:` |
|        - |  7748 | `				 *     (new TestClass())->foo;` |
|        - |  7749 | `				 */` |
|     7058 |  7750 | `				pThis->iRef++;` |
|     7058 |  7751 | `				PH7_MemObjRelease(pTos);` |
|     7058 |  7752 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7058 |  7753 | `				if( pObjAttr ){` |
|     7056 |  7754 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7755 | `					/* Check attribute access */` |
|     7056 |  7756 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7757 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7758 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7759 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7760 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7761 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7054 |  7762 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3569 |  7763 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  7764 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  7765 | `							int bIsLhs = 0;` |
|       82 |  7766 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  7767 | `								bIsLhs = 1;` |
|       39 |  7768 | `							}` |
|       82 |  7769 | `							if( !bIsLhs ){` |
|        3 |  7770 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7771 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7772 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7773 | `									goto Abort;` |
|        - |  7774 | `								}` |
|        - |  7775 | `								{` |
|        3 |  7776 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7777 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7778 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4035 |  7779 | `										break;` |
|        - |  7780 | `									}` |
|        - |  7781 | `								}` |
|      ! 0 |  7782 | `								goto Exception;` |
|        - |  7783 | `							}` |
|       39 |  7784 | `						}` |
|        - |  7785 | `						/* Load attribute */` |
|     7054 |  7786 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7054 |  7787 | `						if( pValue ){` |
|     7054 |  7788 | `							if( pThis->iRef < 2 ){` |
|        - |  7789 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7790 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7791 | `								 */` |
|        7 |  7792 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7793 | `							}else{` |
|        - |  7794 | `								/* Simple load */` |
|     7048 |  7795 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7796 | `							}` |
|     7054 |  7797 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7052 |  7798 | `								if( pThis->iRef > 1 ){` |
|        - |  7799 | `									/* Load attribute index */` |
|     7046 |  7800 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3522 |  7801 | `								}` |
|     3525 |  7802 | `							}` |
|     3526 |  7803 | `						}` |
|     3528 |  7804 | `					}else{` |
|        - |  7805 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7806 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7807 | `						char zMsg[256];` |
|      ! 0 |  7808 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7809 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7810 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7811 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7812 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7813 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7814 | `						goto Abort;` |
|        - |  7815 | `					}` |
|     3526 |  7816 | `				}` |
|        - |  7817 | `				/* Safely unreference the object */` |
|     7056 |  7818 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7819 | `			}` |
|     3921 |  7820 | `		}else{` |
|        3 |  7821 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7822 | `			VmPopOperand(&pTos,1);` |
|        3 |  7823 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7824 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7825 | `		}` |
|     3922 |  7826 | `	}else{` |
|        - |  7827 | `		/* Static member access using class name */` |
|      230 |  7828 | `		pNos = pTos;` |
|      230 |  7829 | `		pThis = 0;` |
|      230 |  7830 | `		if( !pInstr->p3 ){` |
|      192 |  7831 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  7832 | `			pNos--;` |
|        - |  7833 | `#ifdef UNTRUST` |
|        - |  7834 | `			if( pNos < pStack ){` |
|        - |  7835 | `				goto Abort;` |
|        - |  7836 | `			}` |
|        - |  7837 | `#endif` |
|       97 |  7838 | `		}else{` |
|        - |  7839 | `			/* Attribute name already computed */` |
|       40 |  7840 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7841 | `		}` |
|      230 |  7842 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      230 |  7843 | `			ph7_class *pClass = 0;` |
|      230 |  7844 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7845 | `				/* Class already instantiated */` |
|        5 |  7846 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7847 | `				pClass = pThis->pClass;` |
|        5 |  7848 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7849 | `			}else{` |
|        - |  7850 | `				/* Try to extract the target class */` |
|      226 |  7851 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      226 |  7852 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      226 |  7853 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7854 | `					/* Handle self/static/parent keywords */` |
|      226 |  7855 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7856 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7857 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7858 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7859 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7860 | `						}` |
|      196 |  7861 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7862 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      166 |  7863 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7864 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7865 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7866 | `							pClass = pSelf->pBase;` |
|       13 |  7867 | `						}` |
|       15 |  7868 | `					}else{` |
|      114 |  7869 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7870 | `					}` |
|      112 |  7871 | `				}` |
|        - |  7872 | `			}` |
|      230 |  7873 | `			if( pClass == 0 ){` |
|        - |  7874 | `				/* Undefined class */` |
|      ! 0 |  7875 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7876 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7877 | `					);` |
|      ! 0 |  7878 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7879 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7880 | `				}` |
|      ! 0 |  7881 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7882 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7883 | `			}else{` |
|      230 |  7884 | `				if( pInstr->iP2 ){` |
|        - |  7885 | `					/* Method call */` |
|       86 |  7886 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7887 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7888 | `						/* Extract the target method */` |
|       86 |  7889 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7890 | `					}` |
|       86 |  7891 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7892 | `						if( pMeth ){` |
|      ! 0 |  7893 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7894 | `								&pClass->sName,&sName` |
|        - |  7895 | `								);` |
|      ! 0 |  7896 | `						}else{` |
|      ! 0 |  7897 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7898 | `								&pClass->sName,&sName` |
|        - |  7899 | `								);` |
|        - |  7900 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7901 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7902 | `						}` |
|        - |  7903 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7904 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7905 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7906 | `						}` |
|      ! 0 |  7907 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7908 | `					}else{` |
|        - |  7909 | `						/* Push method name on the stack */` |
|       86 |  7910 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7911 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7912 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7913 | `					}` |
|       86 |  7914 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7915 | `				}else{` |
|        - |  7916 | `					/* Attribute access */` |
|      146 |  7917 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7918 | `					/* Check for special ::class pseudo-constant */` |
|      192 |  7919 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7920 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7921 | `						/* ::class returns the fully qualified class name */` |
|        - |  7922 | `						/* Pop the attribute name from the stack */` |
|       60 |  7923 | `						if( !pInstr->p3 ){` |
|       60 |  7924 | `							VmPopOperand(&pTos,1);` |
|       29 |  7925 | `						}` |
|       60 |  7926 | `						PH7_MemObjRelease(pTos);` |
|        - |  7927 | `						/* Load the class name */` |
|       60 |  7928 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7929 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7930 | `					}else{` |
|        - |  7931 | `						/* Extract the target attribute */` |
|       88 |  7932 | `						if( sName.nByte > 0 ){` |
|       88 |  7933 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       43 |  7934 | `						}` |
|       88 |  7935 | `						if( pAttr == 0 ){` |
|        - |  7936 | `							/* No such attribute,load null */` |
|      ! 0 |  7937 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7938 | `								&pClass->sName,&sName);` |
|        - |  7939 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7940 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7941 | `						}` |
|        - |  7942 | `						/* Pop the attribute name from the stack */` |
|       88 |  7943 | `						if( !pInstr->p3 ){` |
|       50 |  7944 | `							VmPopOperand(&pTos,1);` |
|       24 |  7945 | `						}` |
|       88 |  7946 | `						PH7_MemObjRelease(pTos);` |
|       88 |  7947 | `						pTos->nIdx = SXU32_HIGH;` |
|       88 |  7948 | `						if( pAttr ){` |
|       88 |  7949 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7950 | `								/* Access to a non static attribute */` |
|      ! 0 |  7951 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7952 | `									&pClass->sName,&pAttr->sName` |
|        - |  7953 | `									);` |
|      ! 0 |  7954 | `							}else{` |
|        - |  7955 | `								ph7_value *pValue;` |
|        - |  7956 | `								/* Check if the access to the attribute is allowed */` |
|       88 |  7957 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7958 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7959 | `									 * Same LHS-of-store peek as the instance path. */` |
|       82 |  7960 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       56 |  7961 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7962 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7963 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7964 | `										if( pS ){` |
|       28 |  7965 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7966 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7967 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7968 | `												int bIsLhs = 0;` |
|        8 |  7969 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7970 | `													bIsLhs = 1;` |
|        2 |  7971 | `												}` |
|        8 |  7972 | `												if( !bIsLhs ){` |
|        3 |  7973 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7974 | `													if( pThis ){` |
|      ! 0 |  7975 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7976 | `													}` |
|        3 |  7977 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7978 | `														goto Abort;` |
|        - |  7979 | `													}` |
|        - |  7980 | `													{` |
|        3 |  7981 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7982 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7983 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7984 | `															break;` |
|        - |  7985 | `														}` |
|        - |  7986 | `													}` |
|      ! 0 |  7987 | `													goto Exception;` |
|        - |  7988 | `												}` |
|        2 |  7989 | `											}` |
|       12 |  7990 | `										}` |
|       12 |  7991 | `									}` |
|        - |  7992 | `									/* Load the desired attribute */` |
|       82 |  7993 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       82 |  7994 | `									if( pValue ){` |
|       82 |  7995 | `										PH7_MemObjLoad(pValue,pTos);` |
|       82 |  7996 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7997 | `											/* Load index number */` |
|       38 |  7998 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7999 | `										}` |
|       40 |  8000 | `									}` |
|       42 |  8001 | `								}else{` |
|        - |  8002 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8003 | `									char zMsg[256];` |
|        5 |  8004 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8005 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8006 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8007 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8008 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8009 | `									}else{` |
|      ! 0 |  8010 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8011 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8012 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8013 | `									}` |
|        5 |  8014 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8015 | `									goto Abort;` |
|        - |  8016 | `								}` |
|        - |  8017 | `							}` |
|       40 |  8018 | `						}` |
|        - |  8019 | `					}` |
|        - |  8020 | `				}` |
|      224 |  8021 | `				if( pThis ){` |
|        - |  8022 | `					/* Safely unreference the object */` |
|        5 |  8023 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8024 | `				}` |
|        - |  8025 | `			}` |
|      113 |  8026 | `		}else{` |
|        - |  8027 | `			/* Pop operands */` |
|      ! 0 |  8028 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8029 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8030 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8031 | `			}` |
|      ! 0 |  8032 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8033 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8034 | `		}` |
|        - |  8035 | `	}` |
|     8064 |  8036 | `	break;` |
|        - |  8037 | `					}` |
|        - |  8038 | `/*` |
|        - |  8039 | ` * OP_NEW P1 * * *` |
|        - |  8040 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8041 | ` */` |
|      661 |  8042 | `case PH7_OP_NEW: {` |
|     1324 |  8043 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1324 |  8044 | `	ph7_class *pClass = 0;` |
|        - |  8045 | `	ph7_class_instance *pNew;` |
|     1324 |  8046 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8047 | `		/* Try to extract the desired class */` |
|     1985 |  8048 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1322 |  8049 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      661 |  8050 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8051 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8052 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8053 | `	}` |
|     1324 |  8054 | `	if( pClass == 0 ){` |
|        - |  8055 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8056 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8057 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8058 | `			);` |
|        - |  8059 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8060 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8061 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8062 | `			/* Pop given arguments */` |
|      ! 0 |  8063 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8064 | `		}` |
|      ! 0 |  8065 | `		goto Abort;` |
|      ! 0 |  8066 | `	}else{` |
|        - |  8067 | `		ph7_class_method *pCons;` |
|        - |  8068 | `		/* Create a new class instance */` |
|     1324 |  8069 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1324 |  8070 | `		if( pNew == 0 ){` |
|      ! 0 |  8071 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8072 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8073 | `				&pClass->sName` |
|        - |  8074 | `			);` |
|      ! 0 |  8075 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8076 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8077 | `				/* Pop given arguments */` |
|      ! 0 |  8078 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8079 | `			}` |
|      ! 0 |  8080 | `			break;` |
|        - |  8081 | `		}` |
|        - |  8082 | `		/* Check if a constructor is available */` |
|     1324 |  8083 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1324 |  8084 | `		if( pCons == 0 ){` |
|      928 |  8085 | `			SyString *pName = &pClass->sName;` |
|        - |  8086 | `			/* Check for a constructor with the same base class name */` |
|      928 |  8087 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      463 |  8088 | `		}` |
|     1324 |  8089 | `		if( pCons ){` |
|        - |  8090 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8091 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8092 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8093 | `			 * (including variadic string-key packing). */` |
|      398 |  8094 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8095 | `			sxi32 rcCons;` |
|      398 |  8096 | `			SySetReset(&aArg);` |
|      778 |  8097 | `			while( pArg < pTos ){` |
|      382 |  8098 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      382 |  8099 | `				pArg++;` |
|        2 |  8100 | `			}` |
|      398 |  8101 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8102 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8103 | `				sxu32 n;` |
|      114 |  8104 | `				n = SySetUsed(&aArg);` |
|        - |  8105 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8106 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8107 | `				 * after resolution). */` |
|      222 |  8108 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8109 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8110 | `					if( pFuncArg ){` |
|      110 |  8111 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8112 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8113 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8114 | `						}` |
|       54 |  8115 | `					}` |
|      110 |  8116 | `					n++;` |
|        2 |  8117 | `				}` |
|       56 |  8118 | `			}` |
|      398 |  8119 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8120 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      398 |  8121 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8122 | `				pNew->iRef = 1;` |
|      ! 0 |  8123 | `			}` |
|      398 |  8124 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8125 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8126 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8127 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8128 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8129 | `				sxi32 iResumePc;` |
|        5 |  8130 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8131 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8132 | `					goto Abort;` |
|        - |  8133 | `				}` |
|        5 |  8134 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8135 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8136 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8137 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8138 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8139 | `					}` |
|        5 |  8140 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8141 | `					pc = iResumePc;` |
|        5 |  8142 | `					break;` |
|        - |  8143 | `				}` |
|      ! 0 |  8144 | `				goto Exception;` |
|        - |  8145 | `			}` |
|      196 |  8146 | `		}` |
|     1320 |  8147 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8148 | `			/* Pop given arguments */` |
|      312 |  8149 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      155 |  8150 | `		}` |
|     1320 |  8151 | `		PH7_MemObjRelease(pTos);` |
|     1320 |  8152 | `		pTos->x.pOther = pNew;` |
|     1320 |  8153 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8154 | `	}` |
|     1320 |  8155 | `	break;` |
|        - |  8156 | `				 }` |
|        - |  8157 | `/*` |
|        - |  8158 | ` * OP_CLONE * * *` |
|        - |  8159 | ` * Perfome a clone operation.` |
|        - |  8160 | ` */` |
|       24 |  8161 | `case PH7_OP_CLONE: {` |
|        - |  8162 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8163 | `#ifdef UNTRUST` |
|        - |  8164 | `	if( pTos < pStack ){` |
|        - |  8165 | `		goto Abort;` |
|        - |  8166 | `	}` |
|        - |  8167 | `#endif` |
|        - |  8168 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8169 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8170 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8171 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8172 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8173 | `		break;` |
|        - |  8174 | `	}` |
|        - |  8175 | `	/* Point to the source */` |
|       46 |  8176 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8177 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8178 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8179 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8180 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8181 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8182 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8183 | `		break;` |
|        - |  8184 | `	}` |
|        - |  8185 | `	/* Perform the clone operation */` |
|       46 |  8186 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8187 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8188 | `	if( pClone == 0 ){` |
|      ! 0 |  8189 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8190 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8191 | `	}else{` |
|        - |  8192 | `		/* Load the cloned object */` |
|       46 |  8193 | `		pTos->x.pOther = pClone;` |
|       46 |  8194 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8195 | `	}` |
|       46 |  8196 | `	break;` |
|        - |  8197 | `				   }` |
|        - |  8198 | `/*` |
|        - |  8199 | ` * OP_SWITCH * * P3` |
|        - |  8200 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8201 | ` */` |
|       26 |  8202 | `case PH7_OP_SWITCH: {` |
|       54 |  8203 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8204 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8205 | `	ph7_value sValue,sCaseValue;` |
|        - |  8206 | `	sxu32 n,nEntry;` |
|        - |  8207 | `#ifdef UNTRUST` |
|        - |  8208 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8209 | `		goto Abort;` |
|        - |  8210 | `	}` |
|        - |  8211 | `#endif` |
|        - |  8212 | `	/* Point to the case table  */` |
|       54 |  8213 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8214 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8215 | `	/* Select the appropriate case block to execute */` |
|       54 |  8216 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8217 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8218 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8219 | `		pCase = &aCase[n];` |
|      130 |  8220 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8221 | `		/* Execute the case expression first */` |
|      130 |  8222 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8223 | `		/* Compare the two expression */` |
|      130 |  8224 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8225 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8226 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8227 | `		if( rc == 0 ){` |
|        - |  8228 | `			/* Value match,jump to this block */` |
|       52 |  8229 | `			pc = pCase->nStart - 1;` |
|       52 |  8230 | `			break;` |
|        - |  8231 | `		}` |
|       41 |  8232 | `	}` |
|       54 |  8233 | `	VmPopOperand(&pTos,1);` |
|       54 |  8234 | `	if( n >= nEntry ){` |
|        - |  8235 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8236 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8237 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8238 | `		}else{` |
|        - |  8239 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8240 | `			pc = pSwitch->nOut - 1;` |
|        - |  8241 | `		}` |
|        1 |  8242 | `	}` |
|       54 |  8243 | `	break;` |
|        - |  8244 | `					}` |
|        - |  8245 | `/*` |
|        - |  8246 | ` * OP_MATCH * * P3` |
|        - |  8247 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8248 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8249 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8250 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8251 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8252 | ` */` |
|       54 |  8253 | `case PH7_OP_MATCH: {` |
|      110 |  8254 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8255 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8256 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8257 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8258 | `	int matched = 0;` |
|        - |  8259 | `#ifdef UNTRUST` |
|        - |  8260 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8261 | `		goto Abort;` |
|        - |  8262 | `	}` |
|        - |  8263 | `#endif` |
|      110 |  8264 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8265 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8266 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8267 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8268 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8269 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8270 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8271 | `		pArm = &aArm[i];` |
|      240 |  8272 | `		if( pArm->bDefault ){` |
|       13 |  8273 | `			pDefault = pArm;` |
|       13 |  8274 | `			continue;` |
|        - |  8275 | `		}` |
|      228 |  8276 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8277 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8278 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8279 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8280 | `				continue;` |
|        - |  8281 | `			}` |
|      260 |  8282 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8283 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8284 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8285 | `			if( rc == 0 ){` |
|       93 |  8286 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8287 | `				matched = 1;` |
|       93 |  8288 | `				break;` |
|        - |  8289 | `			}` |
|       85 |  8290 | `		}` |
|      115 |  8291 | `	}` |
|      110 |  8292 | `	if( !matched && pDefault ){` |
|       13 |  8293 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8294 | `		matched = 1;` |
|        6 |  8295 | `	}` |
|      110 |  8296 | `	if( !matched ){` |
|        5 |  8297 | `		const char *zType = "unknown";` |
|        - |  8298 | `		char zMsg[128];` |
|        - |  8299 | `		sxu32 nMsg;` |
|        5 |  8300 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8301 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8302 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8303 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8304 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8305 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8306 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8307 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8308 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8309 | `		default: break;` |
|        - |  8310 | `		}` |
|        7 |  8311 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8312 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8313 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8314 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8315 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8316 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8317 | `		goto Abort;` |
|        - |  8318 | `	}` |
|      105 |  8319 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8320 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8321 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8322 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8323 | `	break;` |
|        - |  8324 | `					}` |
|        - |  8325 | `/*` |
|        - |  8326 | ` * OP_YIELD P1 P2 *` |
|        - |  8327 | ` *  Yield a value from a generator function.` |
|        - |  8328 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8329 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8330 | ` */` |
|       34 |  8331 | `case PH7_OP_YIELD: {` |
|        - |  8332 | `	ph7_generator *pGen;` |
|       70 |  8333 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8334 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8335 | `		goto Abort;` |
|        - |  8336 | `	}` |
|       70 |  8337 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8338 | `	if( pInstr->iP2 ){` |
|        - |  8339 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8340 | `#ifdef UNTRUST` |
|        - |  8341 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8342 | `#endif` |
|        7 |  8343 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8344 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8345 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8346 | `		VmPopOperand(&pTos, 1);` |
|        - |  8347 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8348 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8349 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8350 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8351 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8352 | `			}` |
|        1 |  8353 | `		}` |
|       67 |  8354 | `	}else if( pInstr->iP1 ){` |
|        - |  8355 | `		/* yield $value */` |
|        - |  8356 | `#ifdef UNTRUST` |
|        - |  8357 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8358 | `#endif` |
|       64 |  8359 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8360 | `		VmPopOperand(&pTos, 1);` |
|        - |  8361 | `		/* Auto-increment key */` |
|       64 |  8362 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8363 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8364 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8365 | `	}else{` |
|        - |  8366 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8367 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8368 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8369 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8370 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8371 | `	}` |
|        - |  8372 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8373 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8374 | `	goto Suspend;` |
|        - |  8375 |  |
|        - |  8376 | `/*` |
|        - |  8377 | ` * OP_CALL P1 * *` |
|        - |  8378 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8379 | ` *  function on the stack.` |
|        - |  8380 | ` */` |
|   357384 |  8381 | `case PH7_OP_CALL: {` |
|   714814 |  8382 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8383 | `	ph7_value *pArg;` |
|   714814 |  8384 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   714814 |  8385 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8386 | `	SyHashEntry *pEntry;` |
|        - |  8387 | `	SyString sName;` |
|        - |  8388 | `	/* Extract function name */` |
|   714814 |  8389 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8390 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8391 | `			ph7_value sResult;` |
|        - |  8392 | `			sxi32 rcArr;` |
|        3 |  8393 | `			SySetReset(&aArg);` |
|        3 |  8394 | `			while( pArg < pTos ){` |
|      ! 0 |  8395 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8396 | `				pArg++;` |
|      ! 0 |  8397 | `			}` |
|        3 |  8398 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8399 | `			/* May be a class instance and it's static method */` |
|        3 |  8400 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8401 | `			SySetReset(&aArg);` |
|        - |  8402 | `			/* Pop given arguments */` |
|        3 |  8403 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8404 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8405 | `			}` |
|        3 |  8406 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8407 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8408 | `				goto Abort;` |
|        - |  8409 | `			}` |
|        3 |  8410 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8411 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8412 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8413 | `				sxi32 iResumePc;` |
|        3 |  8414 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8415 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8416 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8417 | `					pc = iResumePc;` |
|        3 |  8418 | `					break;` |
|        - |  8419 | `				}` |
|      ! 0 |  8420 | `				goto Exception;` |
|        - |  8421 | `			}` |
|        - |  8422 | `			/* Copy result */` |
|      ! 0 |  8423 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8424 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8425 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8426 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8427 | `			ph7_value sResult;` |
|        - |  8428 | `			sxi32 rcInv;` |
|       84 |  8429 | `			SySetReset(&aArg);` |
|      200 |  8430 | `			while( pArg < pTos ){` |
|      118 |  8431 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8432 | `				pArg++;` |
|        2 |  8433 | `			}` |
|       84 |  8434 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8435 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8436 | `				(int)SySetUsed(&aArg),` |
|       82 |  8437 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8438 | `				&sResult,` |
|       82 |  8439 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8440 | `			SySetReset(&aArg);` |
|       84 |  8441 | `			if( nCallArgs > 0 ){` |
|       76 |  8442 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8443 | `			}` |
|       84 |  8444 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8445 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8446 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8447 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8448 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8449 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8450 | `				pThis->iRef++;` |
|       13 |  8451 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8452 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8453 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8454 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8455 | `					goto Abort;` |
|        - |  8456 | `				}` |
|        - |  8457 | `				{` |
|       13 |  8458 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8459 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8460 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8461 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8462 | `						break;` |
|        - |  8463 | `					}` |
|        - |  8464 | `				}` |
|      ! 0 |  8465 | `				goto Exception;` |
|        - |  8466 | `			}` |
|       72 |  8467 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8468 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8469 | `				goto Abort;` |
|        - |  8470 | `			}` |
|       72 |  8471 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8472 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8473 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8474 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8475 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8476 | `				sxi32 iResumePc;` |
|        7 |  8477 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8478 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8479 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8480 | `					pc = iResumePc;` |
|        5 |  8481 | `					break;` |
|        - |  8482 | `				}` |
|        3 |  8483 | `				goto Exception;` |
|        - |  8484 | `			}` |
|       66 |  8485 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8486 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8487 | `		}else{` |
|        - |  8488 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8489 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8490 | `			/* Pop given arguments */` |
|      ! 0 |  8491 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8492 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8493 | `			}` |
|        - |  8494 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8495 | `			PH7_MemObjRelease(pTos);` |
|        - |  8496 | `		}` |
|       66 |  8497 | `		break;` |
|        - |  8498 | `	}` |
|   714730 |  8499 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8500 | `	/* Check for a compiled function first.` |
|        - |  8501 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8502 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   714730 |  8503 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8504 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8505 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8506 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8507 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8508 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8509 | `	{` |
|   714730 |  8510 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   714730 |  8511 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8512 | `		const char *zFunc;` |
|        - |  8513 | `		const char *zEnd;` |
|        - |  8514 | `		const char *z;` |
|        - |  8515 | `		SyString sGlobal;` |
|       22 |  8516 | `		zFunc = sName.zString;` |
|       22 |  8517 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8518 | `		z = zEnd;` |
|        - |  8519 | `		/* Find last namespace separator */` |
|      194 |  8520 | `		while( z > zFunc ){` |
|      194 |  8521 | `			if( z[-1] == '\\' ){` |
|       22 |  8522 | `				break;` |
|        - |  8523 | `			}` |
|      174 |  8524 | `			z--;` |
|        2 |  8525 | `		}` |
|       22 |  8526 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8527 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8528 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8529 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8530 | `		}` |
|       10 |  8531 | `	}` |
|        - |  8532 | `	} /* end VmCallArgMap namespace scope */` |
|   714730 |  8533 | `	if( pEntry ){` |
|        - |  8534 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8535 | `		ph7_class_instance *pThis;` |
|        - |  8536 | `		ph7_value *pFrameStack;` |
|        - |  8537 | `		ph7_vm_func *pVmFunc;` |
|        - |  8538 | `		ph7_class *pSelf;` |
|        - |  8539 | `		VmFrame *pFrame;` |
|        - |  8540 | `		ph7_value *pObj;` |
|        - |  8541 | `		VmSlot sArg;` |
|        - |  8542 | `		sxu32 n;` |
|        - |  8543 | `		/* initialize fields */` |
|    18494 |  8544 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18494 |  8545 | `		pThis = 0;` |
|    18494 |  8546 | `		pSelf = 0;` |
|    18494 |  8547 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8548 | `			ph7_class_method *pMeth;` |
|        - |  8549 | `			/* Class method call */` |
|     3350 |  8550 | `			ph7_value *pTarget = &pTos[-1];` |
|     3350 |  8551 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8552 | `				/* Extract the 'this' pointer */` |
|     3350 |  8553 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8554 | `					/* Instance already loaded */` |
|     3260 |  8555 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3260 |  8556 | `					pThis->iRef++;` |
|     3260 |  8557 | `					pSelf = pThis->pClass;` |
|     1629 |  8558 | `				}` |
|     3350 |  8559 | `				if( pSelf == 0 ){` |
|       92 |  8560 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8561 | `						/* "Late Static Binding" class name */` |
|      128 |  8562 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8563 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8564 | `					}` |
|       92 |  8565 | `					if( pSelf == 0 ){` |
|       21 |  8566 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8567 | `					}` |
|       45 |  8568 | `				}` |
|     3350 |  8569 | `				if( pThis == 0  ){` |
|       92 |  8570 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8571 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8572 | `					if( pFrameLocal->pParent ){` |
|        - |  8573 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8574 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8575 | `						if( pThis ){` |
|       21 |  8576 | `							pThis->iRef++;` |
|       10 |  8577 | `						}` |
|       32 |  8578 | `					}` |
|       45 |  8579 | `				}` |
|     3350 |  8580 | `				VmPopOperand(&pTos,1);` |
|     3350 |  8581 | `				PH7_MemObjRelease(pTos);` |
|        - |  8582 | `				/* Synchronize pointers */` |
|     3350 |  8583 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8584 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8585 | `				 * user have already computed the random generated unique class method name` |
|        - |  8586 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8587 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8588 | `				 */` |
|     3350 |  8589 | `				while( pArg < pStack ){` |
|      ! 0 |  8590 | `					pArg++;` |
|      ! 0 |  8591 | `				}` |
|     3350 |  8592 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8593 | `					/* Check if the call is allowed */` |
|     3350 |  8594 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3350 |  8595 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8596 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8597 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8598 | `							char zMsg[256];` |
|      ! 0 |  8599 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8600 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8601 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8602 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8603 | `							/* Pop given arguments */` |
|      ! 0 |  8604 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8605 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8606 | `							}` |
|      ! 0 |  8607 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8608 | `							goto Abort;` |
|        - |  8609 | `						}` |
|        6 |  8610 | `					}` |
|     1674 |  8611 | `				}` |
|     1674 |  8612 | `			}` |
|     1674 |  8613 | `		}` |
|        - |  8614 | `		/* Check The recursion limit */` |
|    18494 |  8615 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8616 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8617 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8618 | `				&pVmFunc->sName);` |
|        - |  8619 | `			/* Pop given arguments */` |
|        3 |  8620 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8621 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8622 | `			}` |
|        - |  8623 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8624 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8625 | `			break;` |
|        - |  8626 | `		}` |
|    18492 |  8627 | `		if( pVmFunc->pNextName ){` |
|        - |  8628 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8629 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8630 | `		}` |
|    18492 |  8631 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8632 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8633 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8634 | `			ph7_generator *pGenerator;` |
|        - |  8635 | `			ph7_class_instance *pGenObj;` |
|        - |  8636 | `			ph7_value *pCtxAttr;` |
|        - |  8637 | `			SyString sAttrName;` |
|        - |  8638 | `			ph7_value **apCallArgs;` |
|        - |  8639 | `			int nGenArgs, iArg;` |
|        - |  8640 | `			/* Collect arguments from the operand stack */` |
|       24 |  8641 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8642 | `			apCallArgs = 0;` |
|       24 |  8643 | `			if( nGenArgs > 0 ){` |
|       14 |  8644 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8645 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8646 | `				if( apCallArgs == 0 ){` |
|        - |  8647 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8648 | `					nGenArgs = 0;` |
|      ! 0 |  8649 | `				}else{` |
|       10 |  8650 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8651 | `					int didReorder = 0;` |
|       10 |  8652 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8653 | `						/* Named-argument reordering for generator */` |
|        5 |  8654 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8655 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8656 | `						sxu32 nNV = nF;` |
|        5 |  8657 | `						sxi32 iVIdx = -1;` |
|        - |  8658 | `						sxi32 *aGSlot;` |
|        - |  8659 | `						sxu8 *aGUsed;` |
|        - |  8660 | `						sxu32 gi;` |
|       13 |  8661 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8662 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8663 | `						}` |
|        7 |  8664 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8665 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8666 | `						if( aGSlot ){` |
|        5 |  8667 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8668 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8669 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8670 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8671 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8672 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8673 | `								goto Abort;` |
|        - |  8674 | `							}` |
|        - |  8675 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8676 | `							 * append overflow (variadic / positional beyond` |
|        - |  8677 | `							 * formals) so downstream sees every argument. */` |
|        - |  8678 | `							{` |
|        5 |  8679 | `								int nOut = 0;` |
|       13 |  8680 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8681 | `									sxu32 gj;` |
|       13 |  8682 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8683 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8684 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8685 | `											break;` |
|        - |  8686 | `										}` |
|        3 |  8687 | `									}` |
|        5 |  8688 | `								}` |
|       13 |  8689 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8690 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8691 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8692 | `									}` |
|        5 |  8693 | `								}` |
|        5 |  8694 | `								nGenArgs = nOut;` |
|        - |  8695 | `							}` |
|        5 |  8696 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8697 | `							didReorder = 1;` |
|        2 |  8698 | `						}` |
|        - |  8699 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8700 | `						 * positional fill below — preserves arg order rather` |
|        - |  8701 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8702 | `					}` |
|       10 |  8703 | `					if( !didReorder ){` |
|       12 |  8704 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8705 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8706 | `						}` |
|        2 |  8707 | `					}` |
|        - |  8708 | `				}` |
|        4 |  8709 | `			}` |
|        - |  8710 | `			/* Create execution context and generator wrapper */` |
|       24 |  8711 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8712 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8713 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8714 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8715 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8716 | `				break;` |
|        - |  8717 | `			}` |
|       24 |  8718 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8719 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8720 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8721 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8722 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8723 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8724 | `				break;` |
|        - |  8725 | `			}` |
|        - |  8726 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8727 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8728 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8729 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8730 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8731 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8732 | `			if( apCallArgs ){` |
|       10 |  8733 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8734 | `			}` |
|       24 |  8735 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8736 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8737 | `				if( pThis ){` |
|      ! 0 |  8738 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8739 | `				}` |
|      ! 0 |  8740 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8741 | `					goto Abort;` |
|        - |  8742 | `				}` |
|      ! 0 |  8743 | `				break;` |
|        - |  8744 | `			}` |
|        - |  8745 | `			/* Create Generator class instance */` |
|       24 |  8746 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8747 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8748 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8749 | `				break;` |
|        - |  8750 | `			}` |
|        - |  8751 | `			/* Store generator in __ctx attribute */` |
|       24 |  8752 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8753 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8754 | `			if( pCtxAttr ){` |
|       24 |  8755 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8756 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8757 | `			}` |
|        - |  8758 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8759 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8760 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8761 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8762 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8763 | `			pGenObj->iRef++;` |
|       24 |  8764 | `			if( pThis ){` |
|      ! 0 |  8765 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8766 | `			}` |
|       24 |  8767 | `			break;` |
|        - |  8768 | `		}` |
|        - |  8769 | `		/* Extract the formal argument set */` |
|    18470 |  8770 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8771 | `		/* Create a new VM frame  */` |
|    18470 |  8772 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18470 |  8773 | `		if( rc != SXRET_OK ){` |
|        - |  8774 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8775 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8776 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8777 | `				&pVmFunc->sName);` |
|        - |  8778 | `			/* Pop given arguments */` |
|      ! 0 |  8779 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8780 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8781 | `			}` |
|        - |  8782 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8783 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8784 | `			break;` |
|        - |  8785 | `		}` |
|    18470 |  8786 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8787 | `			/* Install the '$this' variable */` |
|        - |  8788 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3278 |  8789 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3278 |  8790 | `			if( pObj ){` |
|        - |  8791 | `				/* Reflect the change */` |
|     3278 |  8792 | `				pObj->x.pOther = pThis;` |
|     3278 |  8793 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1638 |  8794 | `			}` |
|     1638 |  8795 | `		}` |
|    18470 |  8796 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8797 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8798 | `			/* Install static variables */` |
|      ! 0 |  8799 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8800 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8801 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8802 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8803 | `					/* Initialize the static variables */` |
|      ! 0 |  8804 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8805 | `					if( pObj ){` |
|        - |  8806 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8807 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8808 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8809 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8810 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8811 | `						}` |
|      ! 0 |  8812 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8813 | `					}else{` |
|      ! 0 |  8814 | `						continue;` |
|        - |  8815 | `					}` |
|      ! 0 |  8816 | `				}` |
|        - |  8817 | `				/* Install in the current frame */` |
|      ! 0 |  8818 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8819 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8820 | `			}` |
|      ! 0 |  8821 | `		}` |
|        - |  8822 | `		/* Push arguments in the local frame */` |
|        - |  8823 | `		{` |
|    18470 |  8824 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8825 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8826 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18470 |  8827 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18470 |  8828 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8829 | `			/* ============================================================` |
|        - |  8830 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8831 | `			 *` |
|        - |  8832 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8833 | `			 * or position, then install them in the frame.` |
|        - |  8834 | `			 * ============================================================ */` |
|       96 |  8835 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8836 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8837 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8838 | `			sxu32 nNonVariadic;` |
|        - |  8839 | `			sxi32 *aSlot;` |
|        - |  8840 | `			sxu8  *aUsed;` |
|        - |  8841 | `			sxu32 i;` |
|        - |  8842 | `			/* Find variadic parameter index */` |
|      292 |  8843 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8844 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8845 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8846 | `					break;` |
|        - |  8847 | `				}` |
|      100 |  8848 | `			}` |
|       96 |  8849 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8850 | `			/* Allocate mapping arrays */` |
|      143 |  8851 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8852 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8853 | `			if( aSlot == 0 ){` |
|      ! 0 |  8854 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8855 | `				goto Abort;` |
|        - |  8856 | `			}` |
|       96 |  8857 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8858 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8859 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8860 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8861 | `			if( rc == PH7_ABORT ){` |
|        7 |  8862 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8863 | `				goto Abort;` |
|        - |  8864 | `			}` |
|        - |  8865 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8866 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8867 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8868 | `				sxi32 iSrc = -1;` |
|      309 |  8869 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8870 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8871 | `						iSrc = (sxi32)i;` |
|      169 |  8872 | `						break;` |
|        - |  8873 | `					}` |
|       62 |  8874 | `				}` |
|      187 |  8875 | `				if( iSrc >= 0 ){` |
|        - |  8876 | `					/* Argument was provided — install with type checking */` |
|      169 |  8877 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8878 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8879 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8880 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8881 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8882 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8883 | `					}` |
|        - |  8884 | `					/* Type checking: union types */` |
|      169 |  8885 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8886 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8887 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8888 | `							bCallIsStrict);` |
|       13 |  8889 | `						if( rcU != SXRET_OK ){` |
|        - |  8890 | `							const char *zGiven;` |
|      ! 0 |  8891 | `							const char *zExpected = "union";` |
|        - |  8892 | `							char zBuf[128];` |
|        - |  8893 | `							char zTypeBuf[128];` |
|      ! 0 |  8894 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8895 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8896 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8897 | `								zGiven = "null";` |
|      ! 0 |  8898 | `							}else{` |
|      ! 0 |  8899 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8900 | `							}` |
|      ! 0 |  8901 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8902 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8903 | `							}` |
|      ! 0 |  8904 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8905 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8906 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8907 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8908 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8909 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8910 | `							pFrameStack = 0;` |
|      ! 0 |  8911 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8912 | `							goto SkipFuncBody;` |
|        - |  8913 | `						}` |
|      171 |  8914 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8915 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8916 | `						/* Scalar/class type checking */` |
|       17 |  8917 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8918 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8919 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8920 | `							if( pClass ){` |
|      ! 0 |  8921 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8922 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8923 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8924 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8925 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8926 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8927 | `									}` |
|      ! 0 |  8928 | `								}else{` |
|      ! 0 |  8929 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8930 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8931 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8932 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8933 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8934 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8935 | `									}` |
|        - |  8936 | `								}` |
|      ! 0 |  8937 | `							}` |
|       17 |  8938 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8939 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8940 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8941 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8942 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8943 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8944 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8945 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8946 | `								pFrameStack = 0;` |
|      ! 0 |  8947 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8948 | `								goto SkipFuncBody;` |
|        7 |  8949 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8950 | `								char zTypeBuf[128];` |
|      ! 0 |  8951 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8952 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8953 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8954 | `									ph7_type_name(pVal));` |
|      ! 0 |  8955 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8956 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8957 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8958 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8959 | `								pFrameStack = 0;` |
|      ! 0 |  8960 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8961 | `								goto SkipFuncBody;` |
|        - |  8962 | `							}` |
|        3 |  8963 | `						}` |
|        8 |  8964 | `					}` |
|        - |  8965 | `					/* Install: by reference or by value */` |
|      169 |  8966 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8967 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8968 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8969 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8970 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8971 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8972 | `							}` |
|      ! 0 |  8973 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8974 | `						}else{` |
|        7 |  8975 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8976 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8977 | `							if( pRefEntry == 0 ){` |
|        7 |  8978 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8979 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8980 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8981 | `								sArg.pUserData = 0;` |
|        5 |  8982 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8983 | `							}` |
|        5 |  8984 | `							pObj = 0;` |
|        - |  8985 | `						}` |
|        3 |  8986 | `					}else{` |
|      165 |  8987 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8988 | `					}` |
|      169 |  8989 | `					if( pObj ){` |
|      165 |  8990 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8991 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8992 | `						sArg.pUserData = 0;` |
|      165 |  8993 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8994 | `					}` |
|       85 |  8995 | `				}else{` |
|        - |  8996 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8997 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8998 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8999 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9000 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9001 | `						if( pObj ){` |
|       19 |  9002 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9003 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9004 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9005 | `							sArg.pUserData = 0;` |
|       19 |  9006 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9007 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9008 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9009 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9010 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9011 | `							}` |
|        9 |  9012 | `						}` |
|        9 |  9013 | `					}` |
|        - |  9014 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9015 | `				}` |
|       94 |  9016 | `			}` |
|        - |  9017 | `			/* Handle variadic parameter */` |
|       89 |  9018 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9019 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9020 | `				if( pObj ){` |
|        9 |  9021 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9022 | `					{` |
|        9 |  9023 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9024 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9025 | `							if( aSlot[i] == -1 ){` |
|       16 |  9026 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9027 | `									/* Named variadic entry: insert with string key */` |
|        - |  9028 | `									ph7_value sKey;` |
|       11 |  9029 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9030 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9031 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9032 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9033 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9034 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9035 | `								}else{` |
|        - |  9036 | `									/* Positional variadic entry */` |
|      ! 0 |  9037 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9038 | `								}` |
|        5 |  9039 | `							}` |
|       12 |  9040 | `						}` |
|        - |  9041 | `					}` |
|        9 |  9042 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9043 | `					sArg.pUserData = 0;` |
|        9 |  9044 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9045 | `				}` |
|        5 |  9046 | `			}else{` |
|        - |  9047 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9048 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9049 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9050 | `				 * the positional-only path's behavior. */` |
|       81 |  9051 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9052 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9053 | `					if( aSlot[i] == -2 ){` |
|        - |  9054 | `						char zAnonBuf[32];` |
|        - |  9055 | `						SyString sAnonName;` |
|      ! 0 |  9056 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9057 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9058 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9059 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9060 | `						if( pObj ){` |
|      ! 0 |  9061 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9062 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9063 | `							sArg.pUserData = 0;` |
|      ! 0 |  9064 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9065 | `						}` |
|      ! 0 |  9066 | `						nAnon++;` |
|      ! 0 |  9067 | `					}` |
|       79 |  9068 | `				}` |
|        - |  9069 | `			}` |
|        - |  9070 | `			/* Release all stack arguments */` |
|      267 |  9071 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9072 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9073 | `			}` |
|       89 |  9074 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9075 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9076 | `			n = nFormal;` |
|       45 |  9077 | `		}else{` |
|        - |  9078 | `		/* ============================================================` |
|        - |  9079 | `		 * Positional-only matching path (original)` |
|        - |  9080 | `		 * ============================================================ */` |
|    18376 |  9081 | `		n = 0;` |
|    48924 |  9082 | `		while( pArg < pTos ){` |
|    30622 |  9083 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9084 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9085 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9086 | `				if( pObj ){` |
|        - |  9087 | `					/* Initialize as empty array */` |
|       40 |  9088 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9089 | `					{` |
|       40 |  9090 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9091 | `						while( pArg < pTos ){` |
|        - |  9092 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9093 | `							 *` |
|        - |  9094 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9095 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9096 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9097 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9098 | `							 * fixing both wants a separate counter for elements` |
|        - |  9099 | `							 * already packed into the variadic array. */` |
|      114 |  9100 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9101 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9102 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9103 | `									bCallIsStrict);` |
|       16 |  9104 | `								if( rcU != SXRET_OK ){` |
|        - |  9105 | `									const char *zGiven;` |
|        3 |  9106 | `									const char *zExpected = "union";` |
|        - |  9107 | `									char zBuf[128];` |
|        - |  9108 | `									char zTypeBuf[128];` |
|        3 |  9109 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9110 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9111 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9112 | `										zGiven = "null";` |
|      ! 0 |  9113 | `									}else{` |
|        3 |  9114 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9115 | `									}` |
|        3 |  9116 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9117 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9118 | `									}` |
|        4 |  9119 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9120 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9121 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9122 | `										goto Abort;` |
|        - |  9123 | `									}` |
|        3 |  9124 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9125 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9126 | `									pFrameStack = 0;` |
|        3 |  9127 | `									rc = PH7_EXCEPTION;` |
|        3 |  9128 | `									goto SkipFuncBody;` |
|        - |  9129 | `								}` |
|       14 |  9130 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9131 | `								pArg++;` |
|       14 |  9132 | `								continue;` |
|        - |  9133 | `							}` |
|        - |  9134 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9135 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9136 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9137 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9138 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9139 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9140 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9141 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9142 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9143 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9144 | `										goto Abort;` |
|        - |  9145 | `									}` |
|        - |  9146 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9147 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9148 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9149 | `									pFrameStack = 0;` |
|      ! 0 |  9150 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9151 | `									goto SkipFuncBody;` |
|       13 |  9152 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9153 | `									char zTypeBuf[128];` |
|      ! 0 |  9154 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9155 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9156 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9157 | `										ph7_type_name(pArg));` |
|      ! 0 |  9158 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9159 | `										goto Abort;` |
|        - |  9160 | `									}` |
|      ! 0 |  9161 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9162 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9163 | `									pFrameStack = 0;` |
|      ! 0 |  9164 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9165 | `									goto SkipFuncBody;` |
|        - |  9166 | `								}` |
|        6 |  9167 | `							}` |
|      100 |  9168 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9169 | `							pArg++;` |
|        2 |  9170 | `						}` |
|        - |  9171 | `					}` |
|       38 |  9172 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9173 | `					sArg.pUserData = 0;` |
|       38 |  9174 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9175 | `				}` |
|       38 |  9176 | `				break; /* All remaining args consumed */` |
|        - |  9177 | `			}` |
|    30584 |  9178 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30366 |  9179 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9180 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9181 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9182 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9183 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9184 | `						goto Abort;` |
|        - |  9185 | `					}` |
|      ! 0 |  9186 | `				}` |
|        - |  9187 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30368 |  9188 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9189 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9190 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9191 | `						bCallIsStrict);` |
|       60 |  9192 | `					if( rcU != SXRET_OK ){` |
|        - |  9193 | `						const char *zGiven;` |
|       19 |  9194 | `						const char *zExpected = "union";` |
|        - |  9195 | `						char zBuf[128];` |
|        - |  9196 | `						char zTypeBuf[128];` |
|       19 |  9197 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9198 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9199 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9200 | `							zGiven = "null";` |
|        5 |  9201 | `						}else{` |
|        5 |  9202 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9203 | `						}` |
|       19 |  9204 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9205 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9206 | `						}` |
|       28 |  9207 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9208 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9209 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9210 | `							goto Abort;` |
|        - |  9211 | `						}` |
|       19 |  9212 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9213 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9214 | `						pFrameStack = 0;` |
|       19 |  9215 | `						rc = PH7_EXCEPTION;` |
|       19 |  9216 | `						goto SkipFuncBody;` |
|        - |  9217 | `					}` |
|       21 |  9218 | `				}else` |
|        - |  9219 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9220 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30334 |  9221 | `				if( aFormalArg[n].nType > 0` |
|    15871 |  9222 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9223 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9224 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9225 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9226 | `						ph7_class *pClass;` |
|        - |  9227 | `						/* Try to extract the desired class */` |
|       26 |  9228 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9229 | `						if( pClass ){` |
|       22 |  9230 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9231 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9232 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9233 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9234 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9235 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9236 | `								}` |
|      ! 0 |  9237 | `							}else{` |
|        - |  9238 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9239 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9240 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9241 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9242 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9243 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9244 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9245 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9246 | `								}` |
|        - |  9247 | `							}` |
|       12 |  9248 | `						}` |
|     1394 |  9249 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9250 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9251 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9252 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9253 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9254 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9255 | `								goto Abort;` |
|        - |  9256 | `							}` |
|        - |  9257 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9258 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9259 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9260 | `							pFrameStack = 0;` |
|       11 |  9261 | `							rc = PH7_EXCEPTION;` |
|       11 |  9262 | `							goto SkipFuncBody;` |
|       16 |  9263 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9264 | `							char zTypeBuf[128];` |
|       11 |  9265 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9266 | `								&aFormalArg[n].sName,` |
|        6 |  9267 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9268 | `								ph7_type_name(pArg));` |
|        8 |  9269 | `							if( rc == PH7_ABORT ){` |
|        5 |  9270 | `								goto Abort;` |
|        - |  9271 | `							}` |
|        3 |  9272 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9273 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9274 | `							pFrameStack = 0;` |
|        3 |  9275 | `							rc = PH7_EXCEPTION;` |
|        3 |  9276 | `							goto SkipFuncBody;` |
|        - |  9277 | `						}` |
|        4 |  9278 | `					}` |
|      694 |  9279 | `				}` |
|    30334 |  9280 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9281 | `					/* Pass by reference */` |
|       58 |  9282 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9283 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9284 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9285 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9286 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9287 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9288 | `						}` |
|        - |  9289 | `						/* Switch to pass by value */` |
|      ! 0 |  9290 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9291 | `					}else{` |
|        - |  9292 | `						SyHashEntry *pRefEntry;` |
|        - |  9293 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9294 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9295 | `						if( pRefEntry == 0 ){` |
|       86 |  9296 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9297 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9298 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9299 | `							sArg.pUserData = 0;` |
|       58 |  9300 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9301 | `						}` |
|       58 |  9302 | `						pObj = 0;` |
|        - |  9303 | `					}` |
|       30 |  9304 | `				}else{` |
|        - |  9305 | `					/* Pass by value,make a copy of the given argument */` |
|    30278 |  9306 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9307 | `				}` |
|    15168 |  9308 | `			}else{` |
|        - |  9309 | `				char zName[32];` |
|        - |  9310 | `				SyString sArgName;` |
|        - |  9311 | `				/* Set a dummy name */` |
|      218 |  9312 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9313 | `				sArgName.zString = zName;` |
|        - |  9314 | `				/* Annonymous argument */` |
|      218 |  9315 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9316 | `			}` |
|    30550 |  9317 | `			if( pObj ){` |
|    30494 |  9318 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9319 | `				/* Insert argument index  */` |
|    30494 |  9320 | `				sArg.nIdx = pObj->nIdx;` |
|    30494 |  9321 | `				sArg.pUserData = 0;` |
|    30494 |  9322 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15246 |  9323 | `			}` |
|    30550 |  9324 | `			PH7_MemObjRelease(pArg);` |
|    30550 |  9325 | `			pArg++;` |
|    30550 |  9326 | `			++n;` |
|        2 |  9327 | `		}` |
|        - |  9328 | `		} /* end named vs positional branch */` |
|        - |  9329 | `		/* Set up closure environment */` |
|    18428 |  9330 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9331 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9332 | `			ph7_value *pValue;` |
|        - |  9333 | `			sxu32 iEnv;` |
|      178 |  9334 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      422 |  9335 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      246 |  9336 | `				pEnv = &aEnv[iEnv];` |
|      246 |  9337 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9338 | `					/* Do not install null value */` |
|      172 |  9339 | `					continue;` |
|        - |  9340 | `				}` |
|       76 |  9341 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9342 | `				if( pValue == 0 ){` |
|      ! 0 |  9343 | `					continue;` |
|        - |  9344 | `				}` |
|        - |  9345 | `				/* Invalidate any prior representation */` |
|       76 |  9346 | `				PH7_MemObjRelease(pValue);` |
|        - |  9347 | `				/* Duplicate bound variable value */` |
|       76 |  9348 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9349 | `			}` |
|       88 |  9350 | `		}` |
|        - |  9351 | `		/* Process default values for remaining formal parameters */` |
|    21316 |  9352 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2936 |  9353 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9354 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9355 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9356 | `				if( pObj ){` |
|       48 |  9357 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9358 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9359 | `					sArg.pUserData = 0;` |
|       48 |  9360 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9361 | `				}` |
|       48 |  9362 | `				n++;` |
|       48 |  9363 | `				break; /* Variadic is always last */` |
|        - |  9364 | `			}` |
|     2890 |  9365 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2884 |  9366 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2884 |  9367 | `				if( pObj ){` |
|        - |  9368 | `					/* Evaluate the default value and extract it's result */` |
|     2884 |  9369 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2884 |  9370 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9371 | `						goto Abort;` |
|        - |  9372 | `					}` |
|        - |  9373 | `					/* Insert argument index */` |
|     2884 |  9374 | `					sArg.nIdx = pObj->nIdx;` |
|     2884 |  9375 | `					sArg.pUserData = 0;` |
|     2884 |  9376 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9377 | `					/* Make sure the default argument is of the correct type */` |
|     2882 |  9378 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1864 |  9379 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9380 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9381 | `						/* Cast to the desired type */` |
|        3 |  9382 | `						xCast(pObj);` |
|        1 |  9383 | `					}` |
|     1441 |  9384 | `				}` |
|     1441 |  9385 | `			}` |
|     2890 |  9386 | `			++n;` |
|        2 |  9387 | `		}` |
|        - |  9388 | `		} /* end VmCallArgMap scope */` |
|        - |  9389 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9390 | `		 * does not return anything.` |
|        - |  9391 | `		 */` |
|    18428 |  9392 | `		PH7_MemObjRelease(pTos);` |
|    18428 |  9393 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9394 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18428 |  9395 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18428 |  9396 | `		if( pFrameStack == 0 ){` |
|        - |  9397 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9398 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9399 | `				&pVmFunc->sName);` |
|      ! 0 |  9400 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9401 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9402 | `			}` |
|      ! 0 |  9403 | `			break;` |
|        - |  9404 | `		}` |
|     9213 |  9405 | `SkipFuncBody:` |
|    18460 |  9406 | `		if( pSelf ){` |
|        - |  9407 | `			/* Push class name */` |
|     3348 |  9408 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1673 |  9409 | `		}` |
|        - |  9410 | `		/* Increment nesting level */` |
|    18460 |  9411 | `		pVm->nRecursionDepth++;` |
|    18460 |  9412 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9413 | `			/* Execute function body */` |
|    27641 |  9414 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18426 |  9415 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9213 |  9416 | `		}` |
|        - |  9417 | `		/* Decrement nesting level */` |
|    18460 |  9418 | `		pVm->nRecursionDepth--;` |
|    18460 |  9419 | `		if( pSelf ){` |
|        - |  9420 | `			/* Pop class name */` |
|     3348 |  9421 | `			(void)SySetPop(&pVm->aSelf);` |
|     1673 |  9422 | `		}` |
|        - |  9423 | `		/* Cleanup the mess left behind */` |
|    18460 |  9424 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9425 | `			/* Return by reference,reflect that */` |
|        9 |  9426 | `			if( n != SXU32_HIGH ){` |
|        9 |  9427 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9428 | `				sxu32 i;` |
|        - |  9429 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9430 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9431 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9432 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9433 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9434 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9435 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9436 | `								&pVmFunc->sName);` |
|      ! 0 |  9437 | `						}` |
|      ! 0 |  9438 | `						n = SXU32_HIGH;` |
|      ! 0 |  9439 | `						break;` |
|        - |  9440 | `					}` |
|        3 |  9441 | `				}` |
|        5 |  9442 | `			}else{` |
|      ! 0 |  9443 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9444 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9445 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9446 | `						&pVmFunc->sName);` |
|      ! 0 |  9447 | `				}` |
|        - |  9448 | `			}` |
|        9 |  9449 | `			pTos->nIdx = n;` |
|        4 |  9450 | `		}` |
|        - |  9451 | `		/* Cleanup the mess left behind */` |
|    18460 |  9452 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9453 | `			/* An exception was throw in this frame */` |
|      100 |  9454 | `			pFrame = pFrame->pParent;` |
|      100 |  9455 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9456 | `				/* Pop the resutlt */` |
|       62 |  9457 | `				VmPopOperand(&pTos,1);` |
|        - |  9458 | `				/* Jump to this destination */` |
|       62 |  9459 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9460 | `				rc = PH7_OK;` |
|       32 |  9461 | `			}else{` |
|       39 |  9462 | `				if( pFrame->pParent ){` |
|       39 |  9463 | `					rc = PH7_EXCEPTION;` |
|       20 |  9464 | `				}else{` |
|        - |  9465 | `					/* Continue normal execution */` |
|      ! 0 |  9466 | `					rc = PH7_OK;` |
|        - |  9467 | `				}` |
|        - |  9468 | `			}` |
|       49 |  9469 | `		}` |
|        - |  9470 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18460 |  9471 | `		if( pFrameStack ){` |
|    18428 |  9472 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9213 |  9473 | `		}` |
|        - |  9474 | `		/* Leave the frame */` |
|    18460 |  9475 | `		VmLeaveFrame(&(*pVm));` |
|    18460 |  9476 | `		if( rc == PH7_ABORT ){` |
|        - |  9477 | `			/* Abort processing immeditaley */` |
|       17 |  9478 | `			goto Abort;` |
|    18444 |  9479 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9480 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9481 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9482 | `			 * overwriting the state saved by the inner level.` |
|        - |  9483 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9484 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9485 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9486 | `			goto Suspend;` |
|    18406 |  9487 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9488 | `			goto Exception;` |
|        - |  9489 | `		}` |
|     9185 |  9490 | `	}else{` |
|        - |  9491 | `		ph7_user_func *pFunc;` |
|        - |  9492 | `		ph7_context sCtx;` |
|        - |  9493 | `		ph7_value sRet;` |
|        - |  9494 | `		/* Look for an installed foreign function.` |
|        - |  9495 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9496 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9497 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9498 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   696238 |  9499 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9500 | `		{` |
|   696238 |  9501 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   696238 |  9502 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9503 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9504 | `			const char *zShort = sName.zString;` |
|        - |  9505 | `			sxu32 i;` |
|      334 |  9506 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9507 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9508 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9509 | `				}` |
|      158 |  9510 | `			}` |
|       22 |  9511 | `			if( zShort != sName.zString ){` |
|       22 |  9512 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9513 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9514 | `			}` |
|       10 |  9515 | `		}` |
|        - |  9516 | `		} /* end VmCallArgMap namespace scope */` |
|   696238 |  9517 | `		if( pEntry == 0 ){` |
|        - |  9518 | `			/* Call to undefined function */` |
|        5 |  9519 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9520 | `			/* Pop given arguments */` |
|        5 |  9521 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9522 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9523 | `			}` |
|        - |  9524 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9525 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9526 | `			break;` |
|        - |  9527 | `		}` |
|   696234 |  9528 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9529 | `		/* Start collecting function arguments */` |
|   696234 |  9530 | `		SySetReset(&aArg);` |
|  1877068 |  9531 | `		while( pArg < pTos ){` |
|  1180836 |  9532 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1180836 |  9533 | `			pArg++;` |
|        2 |  9534 | `		}` |
|        - |  9535 | `		/* Assume a null return value */` |
|   696234 |  9536 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9537 | `		/* Init the call context */` |
|   696234 |  9538 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9539 | `		/* Call the foreign function */` |
|   696234 |  9540 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9541 | `		/* Release the call context */` |
|   696234 |  9542 | `		VmReleaseCallContext(&sCtx);` |
|   696234 |  9543 | `		if( rc == PH7_ABORT ){` |
|        - |  9544 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - |  9545 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - |  9546 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      497 |  9547 | `			PH7_MemObjRelease(&sRet);` |
|      497 |  9548 | `			goto Abort;` |
|   695738 |  9549 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9550 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9551 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9552 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9553 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9554 | `				goto Exception;` |
|        - |  9555 | `			}` |
|        - |  9556 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9557 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9558 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9559 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9560 | `			}` |
|        - |  9561 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9562 | `			VmPopOperand(&pTos,1);` |
|        - |  9563 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9564 | `			pFrm = pVm->pFrame;` |
|      108 |  9565 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9566 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9567 | `			}` |
|      108 |  9568 | `			break;` |
|        - |  9569 | `		}` |
|   695628 |  9570 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9571 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9572 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9573 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9574 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9575 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9576 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9577 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9578 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9579 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9580 | `			}` |
|        - |  9581 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9582 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9583 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9584 | `			goto Suspend;` |
|        - |  9585 | `		}` |
|   695590 |  9586 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9587 | `			/* Pop function name and arguments */` |
|   673610 |  9588 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   336826 |  9589 | `		}` |
|        - |  9590 | `		/* Save foreign function return value */` |
|   695590 |  9591 | `		PH7_MemObjStore(&sRet,pTos);` |
|   695590 |  9592 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9593 | `	}` |
|   713956 |  9594 | `	break;` |
|        - |  9595 | `				  }` |
|        - |  9596 | `/*` |
|        - |  9597 | ` * OP_CONSUME: P1 * *` |
|        - |  9598 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9599 | ` */` |
|    15924 |  9600 | `case PH7_OP_CONSUME: {` |
|    31850 |  9601 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    31850 |  9602 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9603 |  |
|    31850 |  9604 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    31850 |  9605 | `	pCur = pOut;` |
|        - |  9606 | `	/* Start the consume process  */` |
|    63740 |  9607 | `	while( pOut <= pTos ){` |
|        - |  9608 | `		/* Force a string cast */` |
|    31892 |  9609 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1052 |  9610 | `			PH7_MemObjToString(pOut);` |
|      525 |  9611 | `		}` |
|    31892 |  9612 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9613 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9614 | `			/* Invoke the output consumer callback */` |
|    19492 |  9615 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19492 |  9616 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19492 |  9617 | `			SyBlobRelease(&pOut->sBlob);` |
|    19492 |  9618 | `			if( rc == SXERR_ABORT ){` |
|        - |  9619 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9620 | `				goto Abort;` |
|        - |  9621 | `			}` |
|     9745 |  9622 | `		}` |
|    31892 |  9623 | `		pOut++;` |
|        2 |  9624 | `	}` |
|    31850 |  9625 | `	pTos = &pCur[-1];` |
|    31848 |  9626 | `	break;` |
|        - |  9627 | `					 }` |
|        - |  9628 |  |
|        - |  9629 | `		} /* Switch() */` |
| 11758700 |  9630 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9631 | `	} /* For(;;) */` |
|    22133 |  9632 | `Done:` |
|    44268 |  9633 | `	SySetRelease(&aArg);` |
|    44268 |  9634 | `	return SXRET_OK;` |
|       72 |  9635 | `Suspend:` |
|      146 |  9636 | `	SySetRelease(&aArg);` |
|      146 |  9637 | `	return PH7_SUSPEND;` |
|      280 |  9638 | `Abort:` |
|      561 |  9639 | `	SySetRelease(&aArg);` |
|     1875 |  9640 | `	while( pTos >= pStack ){` |
|     1315 |  9641 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9642 | `		pTos--;` |
|        1 |  9643 | `	}` |
|      561 |  9644 | `	return PH7_ABORT;` |
|       29 |  9645 | `Exception:` |
|       60 |  9646 | `	SySetRelease(&aArg);` |
|      112 |  9647 | `	while( pTos >= pStack ){` |
|       54 |  9648 | `		PH7_MemObjRelease(pTos);` |
|       54 |  9649 | `		pTos--;` |
|        2 |  9650 | `	}` |
|       60 |  9651 | `	return PH7_EXCEPTION;` |
|    22516 |  9652 |  |
|        - |  9653 | `/*` |
|        - |  9654 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9655 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9656 | ` * See block-comment on that function for additional information.` |
|        - |  9657 | ` */` |
|    20562 |  9658 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9659 |  |
|        - |  9660 | `	ph7_value *pStack;` |
|        - |  9661 | `	sxi32 rc;` |
|        - |  9662 | `	/* Allocate a new operand stack */` |
|    20564 |  9663 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20564 |  9664 | `	if( pStack == 0 ){` |
|      ! 0 |  9665 | `		return SXERR_MEM;` |
|        - |  9666 | `	}` |
|        - |  9667 | `	/* Execute the program */` |
|    20564 |  9668 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9669 | `	/* Free the operand stack */` |
|    20564 |  9670 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9671 | `	/* Execution result */` |
|    20564 |  9672 | `	return rc;` |
|    10283 |  9673 |  |
|        - |  9674 | `/*` |
|        - |  9675 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9676 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9677 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9678 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9679 | ` * execution ends.` |
|        - |  9680 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9681 | ` * additional information.` |
|        - |  9682 | ` */` |
|     2820 |  9683 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9684 |  |
|        - |  9685 | `	VmShutdownCB *pEntry;` |
|        - |  9686 | `	ph7_value *apArg[10];` |
|        - |  9687 | `	sxu32 n,nEntry;` |
|        - |  9688 | `	int i;` |
|        - |  9689 | `	/* Point to the stack of registered callbacks */` |
|     2822 |  9690 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31022 |  9691 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28202 |  9692 | `		apArg[i] = 0;` |
|    14102 |  9693 | `	}` |
|        - |  9694 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - |  9695 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - |  9696 | `	 * callbacks, mirroring PHP.` |
|        - |  9697 | `	 */` |
|     2822 |  9698 | `	pVm->bHaltRequested = 0;` |
|     2832 |  9699 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 |  9700 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9701 | `		if( pEntry ){` |
|        - |  9702 | `			/* Prepare callback arguments if any */` |
|       12 |  9703 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9704 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9705 | `					break;` |
|        - |  9706 | `				}` |
|      ! 0 |  9707 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9708 | `			}` |
|        - |  9709 | `			/* Invoke the callback */` |
|       12 |  9710 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9711 | `			/*` |
|        - |  9712 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9713 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9714 | `			 */` |
|       12 |  9715 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9716 | `			if( pEntry ){` |
|       12 |  9717 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 |  9718 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9719 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9720 | `				}` |
|        5 |  9721 | `			}` |
|       12 |  9722 | `			if( pVm->bHaltRequested ){` |
|        - |  9723 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 |  9724 | `				break;` |
|        - |  9725 | `			}` |
|        5 |  9726 | `		}` |
|        7 |  9727 | `	}` |
|     2822 |  9728 | `	SySetReset(&pVm->aShutdown);` |
|     2822 |  9729 |  |
|        - |  9730 | `/*` |
|        - |  9731 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9732 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9733 | ` * See block-comment on that function for additional information.` |
|        - |  9734 | ` */` |
|     2820 |  9735 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9736 |  |
|        - |  9737 | `	/* Make sure we are ready to execute this program */` |
|     2822 |  9738 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9739 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9740 | `	}` |
|        - |  9741 | `	/* Set the execution magic number  */` |
|     2822 |  9742 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9743 | `	/* Execute the program */` |
|     2822 |  9744 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9745 | `	/* Invoke any shutdown callbacks */` |
|     2822 |  9746 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9747 | `	/*` |
|        - |  9748 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9749 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9750 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9751 | `	 */` |
|     2822 |  9752 | `	return SXRET_OK;` |
|     1412 |  9753 |  |
|        - |  9754 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9755 | `/*` |
|        - |  9756 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9757 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9758 | ` */` |
|       46 |  9759 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9760 |  |
|        - |  9761 | `	ph7_exec_ctx *pCtx;` |
|        - |  9762 | `	ph7_value *pStack;` |
|        - |  9763 | `	VmFrame *pFrame;` |
|       48 |  9764 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9765 | `	if( pCtx == 0 ){` |
|      ! 0 |  9766 | `		return 0;` |
|        - |  9767 | `	}` |
|       48 |  9768 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9769 | `	pCtx->pVm = pVm;` |
|       48 |  9770 | `	pCtx->pFunc = pFunc;` |
|       48 |  9771 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9772 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9773 | `	pCtx->pc = 0;` |
|       48 |  9774 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9775 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9776 | `	/* Allocate a private operand stack */` |
|       48 |  9777 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9778 | `	if( pStack == 0 ){` |
|      ! 0 |  9779 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9780 | `		return 0;` |
|        - |  9781 | `	}` |
|       48 |  9782 | `	pCtx->pStack = pStack;` |
|        - |  9783 | `	/* Create a detached frame for the fiber */` |
|       48 |  9784 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9785 | `	if( pFrame == 0 ){` |
|      ! 0 |  9786 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9787 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9788 | `		return 0;` |
|        - |  9789 | `	}` |
|       48 |  9790 | `	pCtx->pFrame = pFrame;` |
|       48 |  9791 | `	return pCtx;` |
|       25 |  9792 |  |
|        - |  9793 | `/*` |
|        - |  9794 | ` * Start executing a fiber context for the first time.` |
|        - |  9795 | ` */` |
|       46 |  9796 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9797 |  |
|        - |  9798 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9799 | `	sxi32 rc;` |
|       48 |  9800 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9801 | `		return SXERR_INVALID;` |
|        - |  9802 | `	}` |
|        - |  9803 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9804 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9805 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9806 | `	/* Save and set the active context */` |
|       48 |  9807 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9808 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9809 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9810 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9811 | `	pVm->nRecursionDepth++;` |
|        - |  9812 | `	/* Execute from the beginning */` |
|       48 |  9813 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9814 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9815 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9816 | `	pVm->nRecursionDepth--;` |
|        - |  9817 | `	/* Restore the previous context */` |
|       48 |  9818 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9819 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9820 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9821 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9822 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9823 | `		if( pResult ){` |
|       24 |  9824 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9825 | `		}` |
|       46 |  9826 | `		return SXRET_OK;` |
|        - |  9827 | `	}` |
|        - |  9828 | `	/* Detach frame */` |
|        3 |  9829 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9830 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9831 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9832 | `	}` |
|        3 |  9833 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9834 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9835 | `		return PH7_ABORT;` |
|        - |  9836 | `	}` |
|        3 |  9837 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9838 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9839 | `		return PH7_EXCEPTION;` |
|        - |  9840 | `	}` |
|        - |  9841 | `	/* Normal completion */` |
|        3 |  9842 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9843 | `	if( pResult ){` |
|        3 |  9844 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9845 | `	}` |
|        3 |  9846 | `	return SXRET_OK;` |
|       25 |  9847 |  |
|        - |  9848 | `/*` |
|        - |  9849 | ` * Resume a suspended fiber context.` |
|        - |  9850 | ` */` |
|       98 |  9851 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9852 |  |
|        - |  9853 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9854 | `	sxi32 rc;` |
|      100 |  9855 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9856 | `		return SXERR_INVALID;` |
|        - |  9857 | `	}` |
|        - |  9858 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9859 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9860 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9861 | `	if( pResumeValue ){` |
|       40 |  9862 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9863 | `	}else{` |
|       62 |  9864 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9865 | `	}` |
|      100 |  9866 | `	pCtx->nTos++;` |
|        - |  9867 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9868 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9869 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9870 | `	/* Save and set the active context */` |
|      100 |  9871 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9872 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9873 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9874 | `	pVm->nRecursionDepth++;` |
|        - |  9875 | `	/* Resume execution from saved PC */` |
|      100 |  9876 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9877 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9878 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9879 | `	pVm->nRecursionDepth--;` |
|        - |  9880 | `	/* Restore the previous context */` |
|      100 |  9881 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9882 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9883 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9884 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9885 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9886 | `		if( pResult ){` |
|       18 |  9887 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9888 | `		}` |
|       64 |  9889 | `		return SXRET_OK;` |
|        - |  9890 | `	}` |
|        - |  9891 | `	/* Detach frame */` |
|       38 |  9892 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9893 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9894 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9895 | `	}` |
|       38 |  9896 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9897 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9898 | `		return PH7_ABORT;` |
|        - |  9899 | `	}` |
|       38 |  9900 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9901 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9902 | `		return PH7_EXCEPTION;` |
|        - |  9903 | `	}` |
|        - |  9904 | `	/* Normal completion */` |
|       38 |  9905 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9906 | `	if( pResult ){` |
|       20 |  9907 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9908 | `	}` |
|       38 |  9909 | `	return SXRET_OK;` |
|       51 |  9910 |  |
|        - |  9911 | `/*` |
|        - |  9912 | ` * Release an execution context and all its resources.` |
|        - |  9913 | ` */` |
|        4 |  9914 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9915 |  |
|        5 |  9916 | `	if( pCtx == 0 ){` |
|      ! 0 |  9917 | `		return;` |
|        - |  9918 | `	}` |
|        5 |  9919 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9920 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9921 | `		return;` |
|        - |  9922 | `	}` |
|        5 |  9923 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9924 | `	/* Release values */` |
|        5 |  9925 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9926 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9927 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9928 | `	if( pCtx->pFrame ){` |
|        - |  9929 | `		VmSlot *aSlot;` |
|        - |  9930 | `		sxu32 n;` |
|        - |  9931 | `		/* Free local variables */` |
|        5 |  9932 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9933 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9934 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9935 | `		}` |
|        - |  9936 | `		/* Remove local references */` |
|        5 |  9937 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9938 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9939 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9940 | `		}` |
|        5 |  9941 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9942 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9943 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9944 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9945 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9946 | `		pCtx->pFrame = 0;` |
|        2 |  9947 | `	}` |
|        - |  9948 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9949 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9950 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9951 | `	if( pCtx->pStack ){` |
|        5 |  9952 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9953 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9954 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9955 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9956 | `				pTos--;` |
|        1 |  9957 | `			}` |
|        2 |  9958 | `		}` |
|        5 |  9959 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9960 | `		pCtx->pStack = 0;` |
|        2 |  9961 | `	}` |
|        - |  9962 | `	/* Free the context itself */` |
|        5 |  9963 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9964 |  |
|        - |  9965 | `/*` |
|        - |  9966 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9967 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9968 | ` */` |
|       90 |  9969 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9970 |  |
|        - |  9971 | `	ph7_class_instance *pThis;` |
|        - |  9972 | `	SyString sAttr;` |
|        - |  9973 | `	ph7_value *pAttr;` |
|       92 |  9974 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9975 | `		return 0;` |
|        - |  9976 | `	}` |
|       92 |  9977 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9978 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9979 | `		return 0;` |
|        - |  9980 | `	}` |
|       92 |  9981 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9982 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9983 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9984 | `		return 0;` |
|        - |  9985 | `	}` |
|       62 |  9986 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9987 |  |
|        - |  9988 | `/*` |
|        - |  9989 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9990 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9991 | ` */` |
|       38 |  9992 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9993 |  |
|       40 |  9994 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9995 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9996 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9997 | `			"Cannot suspend outside of a fiber");` |
|        - |  9998 | `	}` |
|       40 |  9999 | `	if( nArg > 0 ){` |
|       40 | 10000 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10001 | `	}else{` |
|      ! 0 | 10002 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10003 | `	}` |
|       40 | 10004 | `	return PH7_SUSPEND;` |
|       21 | 10005 |  |
|        - | 10006 | `/*` |
|        - | 10007 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10008 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10009 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10010 | ` */` |
|       24 | 10011 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10012 |  |
|        - | 10013 | `	ph7_class_instance *pThis;` |
|        - | 10014 | `	ph7_value *pAttr;` |
|        - | 10015 | `	SyString sAttrName;` |
|       26 | 10016 | `	if( nArg < 2 ){` |
|      ! 0 | 10017 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10018 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10019 | `	}` |
|       26 | 10020 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10021 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10022 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10023 | `	}` |
|       26 | 10024 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10025 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10026 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10027 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10028 | `	}` |
|        - | 10029 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10030 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10031 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10032 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10033 | `	}` |
|        - | 10034 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10035 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10036 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10037 | `	if( pAttr ){` |
|       26 | 10038 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10039 | `	}` |
|       26 | 10040 | `	return PH7_OK;` |
|       14 | 10041 |  |
|        - | 10042 | `/*` |
|        - | 10043 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10044 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10045 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10046 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10047 | ` */` |
|       24 | 10048 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10049 | `	ph7_class_instance **ppThis)` |
|        2 | 10050 |  |
|       26 | 10051 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10052 | `	ph7_value *pCallable;` |
|        - | 10053 | `	SyString sAttrName;` |
|       26 | 10054 | `	*ppThis = 0;` |
|       26 | 10055 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10056 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10057 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10058 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10059 | `		return 0;` |
|        - | 10060 | `	}` |
|       26 | 10061 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10062 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10063 | `		SyString sName;` |
|        - | 10064 | `		SyHashEntry *pEntry;` |
|        - | 10065 | `		ph7_vm_func *pFunc;` |
|       26 | 10066 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10067 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10068 | `		if( pEntry == 0 ){` |
|      ! 0 | 10069 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10070 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10071 | `			return 0;` |
|        - | 10072 | `		}` |
|       26 | 10073 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10074 | `		return pFunc;` |
|      ! 0 | 10075 | `	}else{` |
|        - | 10076 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10077 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10078 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10079 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10080 | `		if( pMethod == 0 ){` |
|      ! 0 | 10081 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10082 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10083 | `			return 0;` |
|        - | 10084 | `		}` |
|      ! 0 | 10085 | `		*ppThis = pClosure;` |
|      ! 0 | 10086 | `		return &pMethod->sFunc;` |
|        - | 10087 | `	}` |
|       14 | 10088 |  |
|        - | 10089 | `/*` |
|        - | 10090 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10091 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10092 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10093 | ` */` |
|       46 | 10094 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10095 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10096 |  |
|       48 | 10097 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10098 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10099 | `	sxu32 nFormal, n;` |
|        - | 10100 | `	VmSlot sSlot;` |
|        - | 10101 | `	sxi32 rc;` |
|        - | 10102 | `	/* Install $this for closure/method callables */` |
|       48 | 10103 | `	if( pClosureThis ){` |
|        - | 10104 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10105 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10106 | `		if( pObj ){` |
|      ! 0 | 10107 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10108 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10109 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10110 | `		}` |
|      ! 0 | 10111 | `	}` |
|        - | 10112 | `	/* Install static variables */` |
|       48 | 10113 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10114 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10115 | `		ph7_value *pVal;` |
|      ! 0 | 10116 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10117 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10118 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10119 | `			if( pVal ){` |
|      ! 0 | 10120 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10121 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10122 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10123 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10124 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10125 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10126 | `				}` |
|      ! 0 | 10127 | `			}` |
|      ! 0 | 10128 | `		}` |
|      ! 0 | 10129 | `	}` |
|        - | 10130 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10131 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10132 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10133 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10134 | `		ph7_value *pObj;` |
|       20 | 10135 | `		if( n < (sxu32)nArg ){` |
|        - | 10136 | `			/* Argument provided — install with type casting */` |
|       20 | 10137 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10138 | `			if( pObj ){` |
|       20 | 10139 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10140 | `				/* Type casting */` |
|       20 | 10141 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10142 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10143 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10144 | `						if( xCast ){` |
|      ! 0 | 10145 | `							xCast(pObj);` |
|      ! 0 | 10146 | `						}` |
|      ! 0 | 10147 | `					}` |
|      ! 0 | 10148 | `				}` |
|       20 | 10149 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10150 | `				sSlot.pUserData = 0;` |
|       20 | 10151 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10152 | `			}` |
|        9 | 10153 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10154 | `			/* Default value */` |
|      ! 0 | 10155 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10156 | `			if( pObj ){` |
|      ! 0 | 10157 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10158 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10159 | `					return rc;` |
|        - | 10160 | `				}` |
|      ! 0 | 10161 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10162 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10163 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10164 | `						if( xCast ){` |
|      ! 0 | 10165 | `							xCast(pObj);` |
|      ! 0 | 10166 | `						}` |
|      ! 0 | 10167 | `					}` |
|      ! 0 | 10168 | `				}` |
|      ! 0 | 10169 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10170 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10171 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10172 | `			}` |
|      ! 0 | 10173 | `		}` |
|       11 | 10174 | `	}` |
|        - | 10175 | `	/* Install closure environment (captured variables) */` |
|       48 | 10176 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10177 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10178 | `		ph7_value *pValue;` |
|        - | 10179 | `		sxu32 iEnv;` |
|        3 | 10180 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10181 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10182 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10183 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10184 | `				continue;` |
|        - | 10185 | `			}` |
|        5 | 10186 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10187 | `			if( pValue == 0 ){` |
|      ! 0 | 10188 | `				continue;` |
|        - | 10189 | `			}` |
|        5 | 10190 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10191 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10192 | `		}` |
|        1 | 10193 | `	}` |
|       48 | 10194 | `	return SXRET_OK;` |
|       25 | 10195 |  |
|        - | 10196 | `/*` |
|        - | 10197 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10198 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10199 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10200 | ` */` |
|       26 | 10201 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10202 |  |
|       28 | 10203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10204 | `	ph7_class_instance *pThis;` |
|        - | 10205 | `	ph7_class_instance *pClosureThis;` |
|        - | 10206 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10207 | `	ph7_vm_func *pFunc;` |
|        - | 10208 | `	ph7_value sResult;` |
|        - | 10209 | `	ph7_value *pCtxAttr;` |
|        - | 10210 | `	SyString sAttrName;` |
|        - | 10211 | `	sxi32 rc;` |
|       28 | 10212 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10213 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10214 | `	}` |
|       28 | 10215 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10216 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10217 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10218 | `	if( pExecCtx != 0 ){` |
|        3 | 10219 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10220 | `			"Cannot start a fiber that has already been started");` |
|        - | 10221 | `	}` |
|        - | 10222 | `	/* Resolve callable */` |
|       26 | 10223 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10224 | `	if( pFunc == 0 ){` |
|      ! 0 | 10225 | `		return PH7_EXCEPTION;` |
|        - | 10226 | `	}` |
|        - | 10227 | `	/* Create execution context now that we know the function */` |
|       26 | 10228 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10229 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10230 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10231 | `			"Fiber::start(): out of memory");` |
|        - | 10232 | `	}` |
|        - | 10233 | `	/* Store context in $this->__ctx */` |
|       26 | 10234 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10235 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10236 | `	if( pCtxAttr ){` |
|       26 | 10237 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10238 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10239 | `	}` |
|        - | 10240 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10241 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10242 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10243 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10244 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10245 | `	/* Unpack the args array and install into the frame */` |
|        - | 10246 | `	{` |
|       26 | 10247 | `		ph7_value **apValues = 0;` |
|       26 | 10248 | `		int nActual = 0;` |
|       26 | 10249 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10250 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10251 | `			ph7_hashmap_node *pNode;` |
|       26 | 10252 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10253 | `			if( nCount > 0 ){` |
|        3 | 10254 | `				sxu32 idx = 0;` |
|        4 | 10255 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10256 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10257 | `				if( apValues ){` |
|        3 | 10258 | `					pNode = pMap->pFirst;` |
|        7 | 10259 | `					while( pNode && idx < nCount ){` |
|        5 | 10260 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10261 | `						idx++;` |
|        5 | 10262 | `						pNode = pNode->pPrev;` |
|        1 | 10263 | `					}` |
|        3 | 10264 | `					nActual = (int)idx;` |
|        1 | 10265 | `				}` |
|        1 | 10266 | `			}` |
|       12 | 10267 | `		}` |
|       26 | 10268 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10269 | `		if( apValues ){` |
|        3 | 10270 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10271 | `		}` |
|        - | 10272 | `	}` |
|        - | 10273 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10274 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10275 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10276 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10277 | `		return PH7_ABORT;` |
|        - | 10278 | `	}` |
|       26 | 10279 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10280 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10281 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10282 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10283 | `		return PH7_ABORT;` |
|        - | 10284 | `	}` |
|       26 | 10285 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10286 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10287 | `		return PH7_EXCEPTION;` |
|        - | 10288 | `	}` |
|       26 | 10289 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10290 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10291 | `	return PH7_OK;` |
|       15 | 10292 |  |
|        - | 10293 | `/*` |
|        - | 10294 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10295 | ` */` |
|       36 | 10296 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10297 |  |
|       38 | 10298 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10299 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10300 | `	ph7_value sResult;` |
|        - | 10301 | `	ph7_value *pResumeVal;` |
|        - | 10302 | `	sxi32 rc;` |
|       38 | 10303 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10304 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10305 | `		return PH7_OK;` |
|        - | 10306 | `	}` |
|       38 | 10307 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10308 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10309 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10310 | `		return PH7_OK;` |
|        - | 10311 | `	}` |
|       38 | 10312 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10313 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10314 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10315 | `	}` |
|       36 | 10316 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10317 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10318 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10319 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10320 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10321 | `		return PH7_ABORT;` |
|        - | 10322 | `	}` |
|       36 | 10323 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10324 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10325 | `		return PH7_EXCEPTION;` |
|        - | 10326 | `	}` |
|       36 | 10327 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10328 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10329 | `	return PH7_OK;` |
|       20 | 10330 |  |
|        - | 10331 | `/*` |
|        - | 10332 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10333 | ` */` |
|        6 | 10334 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10335 |  |
|        8 | 10336 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10337 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10338 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10339 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10340 | `		return PH7_OK;` |
|        - | 10341 | `	}` |
|        8 | 10342 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10343 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10344 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10345 | `		return PH7_OK;` |
|        - | 10346 | `	}` |
|        8 | 10347 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10348 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10349 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10350 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10351 | `		}` |
|      ! 0 | 10352 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10353 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10354 | `	}` |
|        8 | 10355 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10356 | `	return PH7_OK;` |
|        5 | 10357 |  |
|        - | 10358 | `/*` |
|        - | 10359 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10360 | ` */` |
|        6 | 10361 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10362 |  |
|        - | 10363 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10364 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10365 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10366 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10367 | `	return PH7_OK;` |
|        4 | 10368 |  |
|      ! 0 | 10369 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10370 |  |
|        - | 10371 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10372 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10373 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10374 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10375 | `	return PH7_OK;` |
|      ! 0 | 10376 |  |
|        6 | 10377 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10378 |  |
|        - | 10379 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10380 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10381 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10382 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10383 | `	return PH7_OK;` |
|        4 | 10384 |  |
|        6 | 10385 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10386 |  |
|        - | 10387 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10388 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10389 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10390 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10391 | `	return PH7_OK;` |
|        4 | 10392 |  |
|        - | 10393 | `/*` |
|        - | 10394 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10395 | ` */` |
|        4 | 10396 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10397 |  |
|        5 | 10398 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10399 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10400 | `	if( nArg < 1 ){` |
|      ! 0 | 10401 | `		return PH7_OK;` |
|        - | 10402 | `	}` |
|        5 | 10403 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10404 | `	if( pExecCtx ){` |
|        5 | 10405 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10406 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10407 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10408 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10409 | `			SyString sAttrName;` |
|        - | 10410 | `			ph7_value *pAttr;` |
|        5 | 10411 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10412 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10413 | `			if( pAttr ){` |
|        5 | 10414 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10415 | `			}` |
|        2 | 10416 | `		}` |
|        2 | 10417 | `	}` |
|        5 | 10418 | `	return PH7_OK;` |
|        3 | 10419 |  |
|        - | 10420 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10421 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10422 |  |
|        - | 10423 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10424 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10425 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10426 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10427 |  |
|      ! 0 | 10428 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10429 |  |
|        - | 10430 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10431 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10432 | `	ph7_exec_ctx *pCtx;` |
|        - | 10433 | `	ph7_vm_func *pFunc;` |
|        - | 10434 | `	ph7_value *pCallable;` |
|        - | 10435 | `	ph7_value *pCtxAttr;` |
|        - | 10436 | `	SyString sAttrName;` |
|        - | 10437 | `	/* Must not already be started */` |
|      ! 0 | 10438 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10439 | `	if( pCtx != 0 ){` |
|      ! 0 | 10440 | `		return SXERR_INVALID;` |
|        - | 10441 | `	}` |
|      ! 0 | 10442 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10443 | `		return SXERR_INVALID;` |
|        - | 10444 | `	}` |
|      ! 0 | 10445 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10446 | `	/* Get the callable */` |
|      ! 0 | 10447 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10448 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10449 | `	if( pCallable == 0 ){` |
|      ! 0 | 10450 | `		return SXERR_INVALID;` |
|        - | 10451 | `	}` |
|        - | 10452 | `	/* Resolve callable */` |
|      ! 0 | 10453 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10454 | `		SyString sName;` |
|        - | 10455 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10456 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10457 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10458 | `		if( pEntry == 0 ){` |
|      ! 0 | 10459 | `			return SXERR_NOTFOUND;` |
|        - | 10460 | `		}` |
|      ! 0 | 10461 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10462 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10463 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10464 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10465 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10466 | `		if( pMethod == 0 ){` |
|      ! 0 | 10467 | `			return SXERR_INVALID;` |
|        - | 10468 | `		}` |
|      ! 0 | 10469 | `		pClosureThis = pClosure;` |
|      ! 0 | 10470 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10471 | `	}else{` |
|      ! 0 | 10472 | `		return SXERR_INVALID;` |
|        - | 10473 | `	}` |
|        - | 10474 | `	/* Create context */` |
|      ! 0 | 10475 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10476 | `	if( pCtx == 0 ){` |
|      ! 0 | 10477 | `		return SXERR_MEM;` |
|        - | 10478 | `	}` |
|        - | 10479 | `	/* Store in __ctx */` |
|      ! 0 | 10480 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10481 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10482 | `	if( pCtxAttr ){` |
|      ! 0 | 10483 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10484 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10485 | `	}` |
|        - | 10486 | `	/* Set up frame with args */` |
|      ! 0 | 10487 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10488 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10489 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10490 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10491 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10492 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10493 |  |
|      ! 0 | 10494 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10495 |  |
|      ! 0 | 10496 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10497 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10498 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10499 |  |
|      ! 0 | 10500 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10501 |  |
|      ! 0 | 10502 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10503 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10504 |  |
|      ! 0 | 10505 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10506 |  |
|      ! 0 | 10507 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10508 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10509 |  |
|      ! 0 | 10510 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10511 |  |
|      ! 0 | 10512 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10513 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10514 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10515 |  |
|        - | 10516 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10517 | `/*` |
|        - | 10518 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10519 | ` */` |
|       22 | 10520 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10521 |  |
|        - | 10522 | `	ph7_generator *pGen;` |
|       24 | 10523 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10524 | `	if( pGen == 0 ){` |
|      ! 0 | 10525 | `		return 0;` |
|        - | 10526 | `	}` |
|       24 | 10527 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10528 | `	pGen->pCtx = pCtx;` |
|       24 | 10529 | `	pGen->iImplicitKey = 0;` |
|       24 | 10530 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10531 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10532 | `	/* Link the generator back to the exec context */` |
|       24 | 10533 | `	pCtx->pPrivate = pGen;` |
|       24 | 10534 | `	return pGen;` |
|       13 | 10535 |  |
|        - | 10536 | `/*` |
|        - | 10537 | ` * Release a generator and its execution context.` |
|        - | 10538 | ` */` |
|      ! 0 | 10539 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10540 |  |
|      ! 0 | 10541 | `	if( pGen == 0 ){` |
|      ! 0 | 10542 | `		return;` |
|        - | 10543 | `	}` |
|      ! 0 | 10544 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10545 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10546 | `	if( pGen->pCtx ){` |
|      ! 0 | 10547 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10548 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10549 | `		pGen->pCtx = 0;` |
|      ! 0 | 10550 | `	}` |
|      ! 0 | 10551 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10552 |  |
|        - | 10553 | `/*` |
|        - | 10554 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10555 | ` */` |
|      236 | 10556 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10557 |  |
|        - | 10558 | `	ph7_class_instance *pThis;` |
|        - | 10559 | `	SyString sAttr;` |
|        - | 10560 | `	ph7_value *pAttr;` |
|      238 | 10561 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10562 | `		return 0;` |
|        - | 10563 | `	}` |
|      238 | 10564 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10565 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10566 | `		return 0;` |
|        - | 10567 | `	}` |
|      238 | 10568 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10569 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10570 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10571 | `		return 0;` |
|        - | 10572 | `	}` |
|      238 | 10573 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10574 |  |
|        - | 10575 | `/*` |
|        - | 10576 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10577 | ` */` |
|       22 | 10578 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10579 |  |
|        - | 10580 | `	ph7_generator *pGen;` |
|        - | 10581 | `	sxi32 rc;` |
|       24 | 10582 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10583 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10584 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10585 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10586 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10587 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10588 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10589 | `	}` |
|       24 | 10590 | `	return PH7_OK;` |
|       13 | 10591 |  |
|        - | 10592 | `/*` |
|        - | 10593 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10594 | ` */` |
|       68 | 10595 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10596 |  |
|        - | 10597 | `	ph7_generator *pGen;` |
|       70 | 10598 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10599 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10600 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10601 | `	return PH7_OK;` |
|       36 | 10602 |  |
|        - | 10603 | `/*` |
|        - | 10604 | ` * Generator::current() — return the last yielded value.` |
|        - | 10605 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10606 | ` */` |
|       68 | 10607 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10608 |  |
|        - | 10609 | `	ph7_generator *pGen;` |
|        - | 10610 | `	sxi32 rc;` |
|       70 | 10611 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10612 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10613 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10614 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10615 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10616 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10617 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10618 | `	}` |
|       70 | 10619 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10620 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10621 | `	}else{` |
|      ! 0 | 10622 | `		ph7_result_null(pCtx);` |
|        - | 10623 | `	}` |
|       70 | 10624 | `	return PH7_OK;` |
|       36 | 10625 |  |
|        - | 10626 | `/*` |
|        - | 10627 | ` * Generator::key() — return the last yielded key.` |
|        - | 10628 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10629 | ` */` |
|       12 | 10630 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10631 |  |
|        - | 10632 | `	ph7_generator *pGen;` |
|        - | 10633 | `	sxi32 rc;` |
|       13 | 10634 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10635 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10636 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10637 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10638 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10639 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10640 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10641 | `	}` |
|       13 | 10642 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10643 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10644 | `	}else{` |
|      ! 0 | 10645 | `		ph7_result_null(pCtx);` |
|        - | 10646 | `	}` |
|       13 | 10647 | `	return PH7_OK;` |
|        7 | 10648 |  |
|        - | 10649 | `/*` |
|        - | 10650 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10651 | ` */` |
|       60 | 10652 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10653 |  |
|        - | 10654 | `	ph7_generator *pGen;` |
|        - | 10655 | `	sxi32 rc;` |
|       62 | 10656 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10657 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10658 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10659 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10660 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10661 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10662 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10663 | `	}else{` |
|      ! 0 | 10664 | `		return PH7_OK;` |
|        - | 10665 | `	}` |
|       62 | 10666 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10667 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10668 | `	return PH7_OK;` |
|       32 | 10669 |  |
|        - | 10670 | `/*` |
|        - | 10671 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10672 | ` */` |
|        4 | 10673 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10674 |  |
|        - | 10675 | `	ph7_generator *pGen;` |
|        - | 10676 | `	ph7_value *pSendVal;` |
|        - | 10677 | `	sxi32 rc;` |
|        5 | 10678 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10679 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10680 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10681 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10682 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10683 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10684 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10685 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10686 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10687 | `	}else{` |
|      ! 0 | 10688 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10689 | `		return PH7_OK;` |
|        - | 10690 | `	}` |
|        5 | 10691 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10692 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10693 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10694 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10695 | `	}else{` |
|        3 | 10696 | `		ph7_result_null(pCtx);` |
|        - | 10697 | `	}` |
|        5 | 10698 | `	return PH7_OK;` |
|        3 | 10699 |  |
|        - | 10700 | `/*` |
|        - | 10701 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10702 | ` *` |
|        - | 10703 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10704 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10705 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10706 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10707 | ` * the exception to the caller.` |
|        - | 10708 | ` */` |
|      ! 0 | 10709 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10710 |  |
|        - | 10711 | `	ph7_generator *pGen;` |
|        - | 10712 | `	const char *zMsg;` |
|        - | 10713 | `	int nLen;` |
|      ! 0 | 10714 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10715 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10716 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10717 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10718 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10719 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10720 | `			"Cannot throw into a closed generator");` |
|        - | 10721 | `	}` |
|        - | 10722 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10723 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10724 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10725 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10726 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10727 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10728 | `	nLen = 0;` |
|      ! 0 | 10729 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10730 | `		/* Try to get the exception's message */` |
|        - | 10731 | `		SyString sAttr;` |
|        - | 10732 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10733 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10734 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10735 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10736 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10737 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10738 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10739 | `		}` |
|      ! 0 | 10740 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10741 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10742 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10743 | `	}` |
|      ! 0 | 10744 | `	(void)nLen;` |
|      ! 0 | 10745 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10746 |  |
|        - | 10747 | `/*` |
|        - | 10748 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10749 | ` */` |
|        2 | 10750 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10751 |  |
|        - | 10752 | `	ph7_generator *pGen;` |
|        3 | 10753 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10754 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10755 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10756 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10757 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10758 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10759 | `	}` |
|        3 | 10760 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10761 | `	return PH7_OK;` |
|        2 | 10762 |  |
|        - | 10763 | `/*` |
|        - | 10764 | ` * Generator::__destruct() — clean up.` |
|        - | 10765 | ` */` |
|      ! 0 | 10766 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10767 |  |
|        - | 10768 | `	ph7_generator *pGen;` |
|      ! 0 | 10769 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10770 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10771 | `	if( pGen ){` |
|      ! 0 | 10772 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10773 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10774 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10775 | `			SyString sAttrName;` |
|        - | 10776 | `			ph7_value *pAttr;` |
|      ! 0 | 10777 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10778 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10779 | `			if( pAttr ){` |
|      ! 0 | 10780 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10781 | `			}` |
|      ! 0 | 10782 | `		}` |
|      ! 0 | 10783 | `	}` |
|      ! 0 | 10784 | `	return PH7_OK;` |
|      ! 0 | 10785 |  |
|        - | 10786 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10787 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10788 | `/*` |
|        - | 10789 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10790 | ` * the desired message.` |
|        - | 10791 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10792 | ` * in 'api.c' for additional information.` |
|        - | 10793 | ` */` |
|      370 | 10794 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10795 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10796 | `	SyString *pString /* Message to output */` |
|        - | 10797 | `	)` |
|        2 | 10798 |  |
|      372 | 10799 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10800 | `	sxi32 rc = SXRET_OK;` |
|        - | 10801 | `	/* Call the output consumer */` |
|      372 | 10802 | `	if( pString->nByte > 0 ){` |
|      372 | 10803 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10804 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10805 | `	}` |
|      372 | 10806 | `	return rc;` |
|        2 | 10807 |  |
|        - | 10808 | `/*` |
|        - | 10809 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10810 | ` * callback to consume the formatted message.` |
|        - | 10811 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10812 | ` * in 'api.c' for additional information.` |
|        - | 10813 | ` */` |
|        2 | 10814 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10815 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10816 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10817 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10818 | `	)` |
|        1 | 10819 |  |
|        3 | 10820 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10821 | `	sxi32 rc = SXRET_OK;` |
|        - | 10822 | `	SyBlob sWorker;` |
|        - | 10823 | `	/* Format the message and call the output consumer */` |
|        3 | 10824 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10825 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10826 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10827 | `		/* Consume the formatted message */` |
|        3 | 10828 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10829 | `	}` |
|        3 | 10830 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10831 | `	/* Release the working buffer */` |
|        3 | 10832 | `	SyBlobRelease(&sWorker);` |
|        3 | 10833 | `	return rc;` |
|        1 | 10834 |  |
|        - | 10835 | `/*` |
|        - | 10836 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10837 | ` * This function never fail and always return a pointer` |
|        - | 10838 | ` * to a null terminated string.` |
|        - | 10839 | ` */` |
|       12 | 10840 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10841 |  |
|       13 | 10842 | `	const char *zOp = "Unknown     ";` |
|       13 | 10843 | `	switch(nOp){` |
|        3 | 10844 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10845 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10846 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10847 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10848 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10849 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10850 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10851 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10852 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10853 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10854 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10855 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10856 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10857 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10858 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10859 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10860 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10861 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10862 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10863 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10864 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10865 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10866 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10867 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10868 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10869 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10870 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10871 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10872 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10873 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10874 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10875 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10876 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10877 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10878 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10879 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10880 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10881 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10882 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10883 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10884 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10885 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10886 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10887 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10888 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10889 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10890 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10891 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10892 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10893 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10894 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10895 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10896 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10897 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10898 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10899 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10900 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10901 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10902 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10903 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10904 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10905 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10906 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10907 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10908 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10909 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10910 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10911 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10912 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10913 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10914 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10915 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10916 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10917 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10918 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10919 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10920 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10921 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10922 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10923 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10924 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10925 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10926 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10927 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10928 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10929 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10930 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10931 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10932 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10933 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10934 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10935 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10936 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10937 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10938 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10939 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10940 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10941 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10942 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10943 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10944 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10945 | `	default:` |
|      ! 0 | 10946 | `		break;` |
|        - | 10947 | `	}` |
|       13 | 10948 | `	return zOp;` |
|        1 | 10949 |  |
|        - | 10950 | `/*` |
|        - | 10951 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10952 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10953 | ` * is responsible of consuming the generated dump.` |
|        - | 10954 | ` */` |
|        2 | 10955 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10956 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10957 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10958 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10959 | `	)` |
|        1 | 10960 |  |
|        - | 10961 | `	sxi32 rc;` |
|        3 | 10962 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10963 | `	return rc;` |
|        1 | 10964 |  |
|        - | 10965 | `/*` |
|        - | 10966 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10967 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10968 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10969 | ` * in 'compile.c' for additional information.` |
|        - | 10970 | ` */` |
|       14 | 10971 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10972 |  |
|       15 | 10973 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10974 | `	/* Evaluate and expand constant value */` |
|       15 | 10975 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10976 |  |
|        - | 10977 | `/*` |
|        - | 10978 | ` * Section:` |
|        - | 10979 | ` *  Function handling functions.` |
|        - | 10980 | ` * Status:` |
|        - | 10981 | ` *    Stable.` |
|        - | 10982 | ` */` |
|        - | 10983 | `/*` |
|        - | 10984 | ` * int func_num_args(void)` |
|        - | 10985 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10986 | ` * Parameters` |
|        - | 10987 | ` *   None.` |
|        - | 10988 | ` * Return` |
|        - | 10989 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10990 | ` *  or -1 if called from the globe scope.` |
|        - | 10991 | ` */` |
|      980 | 10992 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10993 |  |
|        - | 10994 | `	VmFrame *pFrame;` |
|        - | 10995 | `	ph7_vm *pVm;` |
|        - | 10996 | `	/* Point to the target VM */` |
|      982 | 10997 | `	pVm = pCtx->pVm;` |
|        - | 10998 | `	/* Current frame */` |
|      982 | 10999 | `	pFrame = pVm->pFrame;` |
|      982 | 11000 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 11001 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11002 | `		SXUNUSED(nArg);` |
|      ! 0 | 11003 | `		SXUNUSED(apArg);` |
|        - | 11004 | `		/* Global frame,return -1 */` |
|      ! 0 | 11005 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11006 | `		return SXRET_OK;` |
|        - | 11007 | `	}` |
|        - | 11008 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 11009 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 11010 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 11011 | `	return SXRET_OK;` |
|      492 | 11012 |  |
|        - | 11013 | `/*` |
|        - | 11014 | ` * value func_get_arg(int $arg_num)` |
|        - | 11015 | ` *   Return an item from the argument list.` |
|        - | 11016 | ` * Parameters` |
|        - | 11017 | ` *  Argument number(index start from zero).` |
|        - | 11018 | ` * Return` |
|        - | 11019 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11020 | ` */` |
|       22 | 11021 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11022 |  |
|       24 | 11023 | `	ph7_value *pObj = 0;` |
|       24 | 11024 | `	VmSlot *pSlot = 0;` |
|        - | 11025 | `	VmFrame *pFrame;` |
|        - | 11026 | `	ph7_vm *pVm;` |
|        - | 11027 | `	/* Point to the target VM */` |
|       24 | 11028 | `	pVm = pCtx->pVm;` |
|        - | 11029 | `	/* Current frame */` |
|       24 | 11030 | `	pFrame = pVm->pFrame;` |
|       24 | 11031 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11032 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11033 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11034 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11035 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11036 | `		return SXRET_OK;` |
|        - | 11037 | `	}` |
|        - | 11038 | `	/* Extract the desired index */` |
|       21 | 11039 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11040 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11041 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11042 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11043 | `		return SXRET_OK;` |
|        - | 11044 | `	}` |
|        - | 11045 | `	/* Extract the desired argument */` |
|       21 | 11046 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11047 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11048 | `			/* Return the desired argument */` |
|       21 | 11049 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11050 | `		}else{` |
|        - | 11051 | `			/* No such argument,return false */` |
|      ! 0 | 11052 | `			ph7_result_bool(pCtx,0);` |
|        - | 11053 | `		}` |
|       11 | 11054 | `	}else{` |
|        - | 11055 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11056 | `		ph7_result_bool(pCtx,0);` |
|        - | 11057 | `	}` |
|       21 | 11058 | `	return SXRET_OK;` |
|       13 | 11059 |  |
|        - | 11060 | `/*` |
|        - | 11061 | ` * array func_get_args_byref(void)` |
|        - | 11062 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11063 | ` * Parameters` |
|        - | 11064 | ` *  None.` |
|        - | 11065 | ` * Return` |
|        - | 11066 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11067 | ` *  member of the current user-defined function's argument list.` |
|        - | 11068 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11069 | ` * NOTE:` |
|        - | 11070 | ` *  Arguments are returned to the array by reference.` |
|        - | 11071 | ` */` |
|        2 | 11072 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11073 |  |
|        - | 11074 | `	ph7_value *pArray;` |
|        - | 11075 | `	VmFrame *pFrame;` |
|        - | 11076 | `	VmSlot *aSlot;` |
|        - | 11077 | `	sxu32 n;` |
|        - | 11078 | `	/* Point to the current frame */` |
|        3 | 11079 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11080 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11081 | `	if( pFrame->pParent == 0 ){` |
|        - | 11082 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11083 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11084 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11085 | `		return SXRET_OK;` |
|        - | 11086 | `	}` |
|        - | 11087 | `	/* Create a new array */` |
|        3 | 11088 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11089 | `	if( pArray == 0 ){` |
|      ! 0 | 11090 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11091 | `		SXUNUSED(apArg);` |
|      ! 0 | 11092 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11093 | `		return SXRET_OK;` |
|        - | 11094 | `	}` |
|        - | 11095 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11096 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11097 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11098 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11099 | `	}` |
|        - | 11100 | `	/* Return the freshly created array */` |
|        3 | 11101 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11102 | `	return SXRET_OK;` |
|        2 | 11103 |  |
|        - | 11104 | `/*` |
|        - | 11105 | ` * array func_get_args(void)` |
|        - | 11106 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11107 | ` * Parameters` |
|        - | 11108 | ` *  None.` |
|        - | 11109 | ` * Return` |
|        - | 11110 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11111 | ` *  member of the current user-defined function's argument list.` |
|        - | 11112 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11113 | ` */` |
|       88 | 11114 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11115 |  |
|       90 | 11116 | `	ph7_value *pObj = 0;` |
|        - | 11117 | `	ph7_value *pArray;` |
|        - | 11118 | `	VmFrame *pFrame;` |
|        - | 11119 | `	VmSlot *aSlot;` |
|        - | 11120 | `	sxu32 n;` |
|        - | 11121 | `	/* Point to the current frame */` |
|       90 | 11122 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11123 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11124 | `	if( pFrame->pParent == 0 ){` |
|        - | 11125 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11126 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11127 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11128 | `		return SXRET_OK;` |
|        - | 11129 | `	}` |
|        - | 11130 | `	/* Create a new array */` |
|       90 | 11131 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11132 | `	if( pArray == 0 ){` |
|      ! 0 | 11133 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11134 | `		SXUNUSED(apArg);` |
|      ! 0 | 11135 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11136 | `		return SXRET_OK;` |
|        - | 11137 | `	}` |
|        - | 11138 | `	/* Start filling the array with the given arguments */` |
|       90 | 11139 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11140 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11141 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11142 | `		if( pObj ){` |
|      134 | 11143 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11144 | `		}` |
|       68 | 11145 | `	}` |
|        - | 11146 | `	/* Return the freshly created array */` |
|       90 | 11147 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11148 | `	return SXRET_OK;` |
|       46 | 11149 |  |
|        - | 11150 | `/*` |
|        - | 11151 | ` * bool function_exists(string $name)` |
|        - | 11152 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11153 | ` * Parameters` |
|        - | 11154 | ` *  The name of the desired function.` |
|        - | 11155 | ` * Return` |
|        - | 11156 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11157 | ` */` |
|     1742 | 11158 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11159 |  |
|        - | 11160 | `	const char *zName;` |
|        - | 11161 | `	ph7_vm *pVm;` |
|        - | 11162 | `	int nLen;` |
|        - | 11163 | `	int res;` |
|     1744 | 11164 | `	if( nArg < 1 ){` |
|        - | 11165 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11166 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11167 | `		return SXRET_OK;` |
|        - | 11168 | `	}` |
|        - | 11169 | `	/* Point to the target VM */` |
|     1744 | 11170 | `	pVm = pCtx->pVm;` |
|        - | 11171 | `	/* Extract the function name */` |
|     1744 | 11172 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11173 | `	/* Assume the function is not defined */` |
|     1744 | 11174 | `	res = 0;` |
|        - | 11175 | `	/* Perform the lookup */` |
|     2613 | 11176 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1738 | 11177 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11178 | `			/* Function is defined */` |
|      266 | 11179 | `			res = 1;` |
|      132 | 11180 | `	}` |
|     1744 | 11181 | `	ph7_result_bool(pCtx,res);` |
|     1744 | 11182 | `	return SXRET_OK;` |
|      873 | 11183 |  |
|        - | 11184 | `/*` |
|        - | 11185 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11186 | ` * [i.e: Whether it is callable or not].` |
|        - | 11187 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11188 | ` */` |
|    23744 | 11189 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11190 |  |
|    23746 | 11191 | `	int res = 0;` |
|    23746 | 11192 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11193 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11194 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11195 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11196 | `		 * standard PHP behavior. */` |
|       20 | 11197 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11198 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11199 | `			res = 1;` |
|       10 | 11200 | `		}` |
|        9 | 11201 | `		(void)CallInvoke;` |
|    23737 | 11202 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11203 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11204 | `		if( pMap->nEntry == 2 ){` |
|        - | 11205 | `			ph7_class *pClass;` |
|        - | 11206 | `			ph7_value *pV;` |
|        - | 11207 | `			/* Extract the target class */` |
|       12 | 11208 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11209 | `			if( pV ){` |
|       12 | 11210 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11211 | `				if( pClass ){` |
|        - | 11212 | `					ph7_class_method *pMethod;` |
|        - | 11213 | `					/* Extract the target method */` |
|       10 | 11214 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11215 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11216 | `						/* Perform the lookup */` |
|       10 | 11217 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11218 | `						if( pMethod ){` |
|        - | 11219 | `							/* Method is callable */` |
|        5 | 11220 | `							res = 1;` |
|        2 | 11221 | `						}` |
|        4 | 11222 | `					}` |
|        4 | 11223 | `				}` |
|        5 | 11224 | `			}` |
|        7 | 11225 | `		}` |
|    23715 | 11226 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11227 | `		const char *zName;` |
|        - | 11228 | `		int nLen;` |
|        - | 11229 | `		/* Extract the name */` |
|     5870 | 11230 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11231 | `		/* Perform the lookup */` |
|     5885 | 11232 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11233 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11234 | `				/* Function is callable */` |
|     5852 | 11235 | `				res = 1;` |
|     2925 | 11236 | `		}` |
|     2934 | 11237 | `	}` |
|    23746 | 11238 | `	return res;` |
|        2 | 11239 |  |
|        - | 11240 | `/*` |
|        - | 11241 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11242 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11243 | ` * Parameters` |
|        - | 11244 | ` * $name` |
|        - | 11245 | ` *    The callback function to check` |
|        - | 11246 | ` * $syntax_only` |
|        - | 11247 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11248 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11249 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11250 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11251 | ` *    a string.` |
|        - | 11252 | ` * Return` |
|        - | 11253 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11254 | ` */` |
|       20 | 11255 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11256 |  |
|        - | 11257 | `	ph7_vm *pVm;` |
|        - | 11258 | `	int res;` |
|       21 | 11259 | `	if( nArg < 1 ){` |
|        - | 11260 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11261 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11262 | `		return SXRET_OK;` |
|        - | 11263 | `	}` |
|        - | 11264 | `	/* Point to the target VM */` |
|       21 | 11265 | `	pVm = pCtx->pVm;` |
|        - | 11266 | `	/* Perform the requested operation */` |
|       21 | 11267 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11268 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11269 | `	return SXRET_OK;` |
|       11 | 11270 |  |
|        - | 11271 | `/*` |
|        - | 11272 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11273 | ` * defined below.` |
|        - | 11274 | ` */` |
|     1306 | 11275 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11276 |  |
|     1307 | 11277 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11278 | `	ph7_value sName;` |
|        - | 11279 | `	sxi32 rc;` |
|        - | 11280 | `	/* Prepare the function name for insertion */` |
|     1307 | 11281 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11282 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11283 | `	/* Perform the insertion */` |
|     1307 | 11284 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11285 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11286 | `	return rc;` |
|        1 | 11287 |  |
|        - | 11288 | `/*` |
|        - | 11289 | ` * array get_defined_functions(void)` |
|        - | 11290 | ` *  Returns an array of all defined functions.` |
|        - | 11291 | ` * Parameter` |
|        - | 11292 | ` *  None.` |
|        - | 11293 | ` * Return` |
|        - | 11294 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11295 | ` *  both built-in (internal) and user-defined.` |
|        - | 11296 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11297 | ` *  defined ones using $arr["user"].` |
|        - | 11298 | ` * Note:` |
|        - | 11299 | ` *  NULL is returned on failure.` |
|        - | 11300 | ` */` |
|        2 | 11301 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11302 |  |
|        - | 11303 | `	ph7_value *pArray,*pEntry;` |
|        - | 11304 | `	/* NOTE:` |
|        - | 11305 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11306 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11307 | `	 */` |
|        3 | 11308 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11309 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11310 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11311 | `		SXUNUSED(apArg);` |
|        - | 11312 | `		/* Return NULL */` |
|      ! 0 | 11313 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11314 | `		return SXRET_OK;` |
|        - | 11315 | `	}` |
|        3 | 11316 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11317 | `	if( pEntry == 0 ){` |
|        - | 11318 | `		/* Return NULL */` |
|      ! 0 | 11319 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11320 | `		return SXRET_OK;` |
|        - | 11321 | `	}` |
|        - | 11322 | `	/* Fill with the appropriate information */` |
|        3 | 11323 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11324 | `	/* Create the 'internal' index */` |
|        3 | 11325 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11326 | `	/* Create the user-func array */` |
|        3 | 11327 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11328 | `	if( pEntry == 0 ){` |
|        - | 11329 | `		/* Return NULL */` |
|      ! 0 | 11330 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11331 | `		return SXRET_OK;` |
|        - | 11332 | `	}` |
|        - | 11333 | `	/* Fill with the appropriate information */` |
|        3 | 11334 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11335 | `	/* Create the 'user' index */` |
|        3 | 11336 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11337 | `	/* Return the multi-dimensional array */` |
|        3 | 11338 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11339 | `	return SXRET_OK;` |
|        2 | 11340 |  |
|        - | 11341 | `/*` |
|        - | 11342 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11343 | ` *  Register a function for execution on shutdown.` |
|        - | 11344 | ` * Note` |
|        - | 11345 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11346 | ` *  be called in the same order as they were registered.` |
|        - | 11347 | ` * Parameters` |
|        - | 11348 | ` *  $callback` |
|        - | 11349 | ` *   The shutdown callback to register.` |
|        - | 11350 | ` * $param` |
|        - | 11351 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11352 | ` * Return` |
|        - | 11353 | ` *  Nothing.` |
|        - | 11354 | ` */` |
|       10 | 11355 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11356 |  |
|        - | 11357 | `	VmShutdownCB sEntry;` |
|        - | 11358 | `	int i,j;` |
|       12 | 11359 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11360 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11361 | `		return PH7_OK;` |
|        - | 11362 | `	}` |
|        - | 11363 | `	/* Zero the Entry */` |
|       12 | 11364 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11365 | `	/* Initialize fields */` |
|       12 | 11366 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11367 | `	/* Save the callback name for later invocation name */` |
|       12 | 11368 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11369 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11370 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11371 | `	}` |
|        - | 11372 | `	/* Copy arguments */` |
|       12 | 11373 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11374 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11375 | `			/* Limit reached */` |
|      ! 0 | 11376 | `			break;` |
|        - | 11377 | `		}` |
|      ! 0 | 11378 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11379 | `	}` |
|       12 | 11380 | `	sEntry.nArg = j;` |
|        - | 11381 | `	/* Install the callback */` |
|       12 | 11382 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11383 | `	return PH7_OK;` |
|        7 | 11384 |  |
|        - | 11385 | `/*` |
|        - | 11386 | ` * Section:` |
|        - | 11387 | ` *  Class handling functions.` |
|        - | 11388 | ` * Status:` |
|        - | 11389 | ` *    Stable.` |
|        - | 11390 | ` */` |
|        - | 11391 | `/*` |
|        - | 11392 | ` * Extract the top active class. NULL is returned` |
|        - | 11393 | ` * if the class stack is empty.` |
|        - | 11394 | ` */` |
|      984 | 11395 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11396 |  |
|      986 | 11397 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11398 | `	ph7_class **apClass;` |
|      986 | 11399 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11400 | `		/* Empty stack,return NULL */` |
|       15 | 11401 | `		return 0;` |
|        - | 11402 | `	}` |
|        - | 11403 | `	/* Peek the last entry */` |
|      972 | 11404 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      972 | 11405 | `	return apClass[pSet->nUsed - 1];` |
|      494 | 11406 |  |
|        - | 11407 | `/*` |
|        - | 11408 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11409 | ` *   Get the class that declared the currently executing method.` |
|        - | 11410 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11411 | ` *` |
|        - | 11412 | ` * Parameters` |
|        - | 11413 | ` *   pVm: Target VM` |
|        - | 11414 | ` *` |
|        - | 11415 | ` * Return` |
|        - | 11416 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11417 | ` *   - Not executing within a class method` |
|        - | 11418 | ` *` |
|        - | 11419 | ` * Note` |
|        - | 11420 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11421 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11422 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11423 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11424 | ` *   declaring class.` |
|        - | 11425 | ` */` |
|       98 | 11426 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11427 |  |
|      100 | 11428 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11429 | `	ph7_vm_func *pVmFunc;` |
|        - | 11430 |  |
|        - | 11431 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11432 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11433 |  |
|        - | 11434 | `	/* Check if we're in a method context */` |
|      100 | 11435 | `	if( pFrame->pParent ){` |
|       96 | 11436 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11437 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11438 | `			/* Return the declaring class */` |
|       96 | 11439 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11440 | `		}` |
|      ! 0 | 11441 | `	}` |
|        - | 11442 |  |
|        5 | 11443 | `	return 0;` |
|       51 | 11444 |  |
|        - | 11445 |  |
|        - | 11446 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11447 | `/*` |
|        - | 11448 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11449 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11450 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11451 | ` * return value indicates failure.` |
|        - | 11452 | ` */` |
|        - | 11453 | `/*` |
|        - | 11454 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11455 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11456 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11457 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11458 | ` */` |
|     2480 | 11459 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11460 | `	ph7_vm *pVm,` |
|        - | 11461 | `	ph7_class_instance *pThis,` |
|        - | 11462 | `	ph7_class_method *pMethod,` |
|        - | 11463 | `	ph7_value *pResult,` |
|        - | 11464 | `	int nArg,` |
|        - | 11465 | `	ph7_value **apArg,` |
|        - | 11466 | `	VmCallArgMap *pMap` |
|        - | 11467 | `	)` |
|        2 | 11468 |  |
|        - | 11469 | `	ph7_value *aStack;` |
|        - | 11470 | `	VmInstr aInstr[2];` |
|        - | 11471 | `	int iCursor;` |
|        - | 11472 | `	int i;` |
|        - | 11473 | `	sxi32 rc;` |
|     2482 | 11474 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2482 | 11475 | `	if( aStack == 0 ){` |
|      ! 0 | 11476 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11477 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11478 | `		return SXERR_MEM;` |
|        - | 11479 | `	}` |
|     4024 | 11480 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1544 | 11481 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1544 | 11482 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      773 | 11483 | `	}` |
|     2482 | 11484 | `	iCursor = nArg + 1;` |
|     2482 | 11485 | `	if( pThis ){` |
|     2476 | 11486 | `		pThis->iRef++;` |
|     2476 | 11487 | `		aStack[i].x.pOther = pThis;` |
|     2476 | 11488 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1237 | 11489 | `	}` |
|     2482 | 11490 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2482 | 11491 | `	i++;` |
|     2482 | 11492 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2482 | 11493 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2482 | 11494 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2482 | 11495 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2482 | 11496 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2482 | 11497 | `	aInstr[0].iP1 = nArg;` |
|     2482 | 11498 | `	aInstr[0].iP2 = 0;` |
|     2482 | 11499 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2482 | 11500 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2482 | 11501 | `	aInstr[1].iP1 = 1;` |
|     2482 | 11502 | `	aInstr[1].iP2 = 0;` |
|     2482 | 11503 | `	aInstr[1].p3  = 0;` |
|     2482 | 11504 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2482 | 11505 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11506 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11507 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2482 | 11508 | `	return rc;` |
|     1242 | 11509 |  |
|     1922 | 11510 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11511 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11512 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11513 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11514 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11515 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11516 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11517 | `	)` |
|        2 | 11518 |  |
|     1924 | 11519 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11520 |  |
|        - | 11521 | `/*` |
|        - | 11522 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11523 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11524 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11525 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11526 | ` *` |
|        - | 11527 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11528 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11529 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11530 | ` *` |
|        - | 11531 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11532 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11533 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11534 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11535 | ` *` |
|        - | 11536 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11537 | ` */` |
|      174 | 11538 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11539 | `	ph7_vm *pVm,` |
|        - | 11540 | `	ph7_class_instance *pThis,` |
|        - | 11541 | `	int nArg,` |
|        - | 11542 | `	ph7_value **apArg,` |
|        - | 11543 | `	ph7_value *pResult,` |
|        - | 11544 | `	VmCallArgMap *pMap` |
|        - | 11545 | `	)` |
|        2 | 11546 |  |
|        - | 11547 | `	ph7_class_method *pMethod;` |
|      176 | 11548 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11549 | `	if( pMethod == 0 ){` |
|       13 | 11550 | `		if( pResult ){` |
|       13 | 11551 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11552 | `		}` |
|       13 | 11553 | `		return SXERR_INVALID;` |
|        - | 11554 | `	}` |
|      164 | 11555 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11556 |  |
|        - | 11557 | `/*` |
|        - | 11558 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11559 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11560 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11561 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11562 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11563 | ` * lookup or 'goto Exception').` |
|        - | 11564 | ` *` |
|        - | 11565 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11566 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11567 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11568 | ` * reported.` |
|        - | 11569 | ` */` |
|       12 | 11570 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11571 |  |
|        - | 11572 | `	ph7_class *pErrorClass;` |
|       13 | 11573 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11574 | `	ph7_class_method *pCons;` |
|        - | 11575 | `	VmFrame *pThrowFrame;` |
|        - | 11576 | `	char zMsg[256];` |
|        - | 11577 | `	int nMsg;` |
|        - | 11578 | `	sxi32 rc;` |
|       25 | 11579 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11580 | `		"Object of type %.*s is not callable",` |
|       12 | 11581 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11582 | `		pThis->pClass->sName.zString);` |
|       13 | 11583 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11584 | `	if( pErrorClass ){` |
|       13 | 11585 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11586 | `	}` |
|       13 | 11587 | `	if( pErrInst == 0 ){` |
|        - | 11588 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11589 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11590 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11591 | `		 * visible to the user. */` |
|      ! 0 | 11592 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11593 | `		return SXERR_ABORT;` |
|        - | 11594 | `	}` |
|       13 | 11595 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11596 | `	if( pCons ){` |
|        - | 11597 | `		ph7_value sArg;` |
|        - | 11598 | `		ph7_value *apMsg[1];` |
|        - | 11599 | `		SyString sMsgStr;` |
|       13 | 11600 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11601 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11602 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11603 | `		apMsg[0] = &sArg;` |
|       13 | 11604 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11605 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11606 | `	}` |
|        - | 11607 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11608 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11609 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11610 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11611 | `	if( pThrowFrame ){` |
|       13 | 11612 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11613 | `	}` |
|       13 | 11614 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11615 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11616 | `	return rc;` |
|        7 | 11617 |  |
|        - | 11618 | `/*` |
|        - | 11619 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11620 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11621 | ` * in the apArg[] array.` |
|        - | 11622 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11623 | ` * return value indicates failure.` |
|        - | 11624 | ` */` |
|     1212 | 11625 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11626 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11627 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11628 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11629 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11630 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11631 | `	)` |
|        2 | 11632 |  |
|        - | 11633 | `	ph7_value *aStack;` |
|        - | 11634 | `	VmInstr aInstr[2];` |
|        - | 11635 | `	int i;` |
|     1214 | 11636 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11637 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11638 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11639 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 11640 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 11641 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 11642 | `			nArg,apArg,pResult,0);` |
|        - | 11643 | `	}` |
|     1122 | 11644 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11645 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11646 | `		if( pResult ){` |
|        - | 11647 | `			/* Assume a null return value */` |
|      ! 0 | 11648 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11649 | `		}` |
|      511 | 11650 | `		return SXERR_INVALID;` |
|        - | 11651 | `	}` |
|      612 | 11652 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11653 | `		/* Class method */` |
|       15 | 11654 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 11655 | `		ph7_class_method *pMethod = 0;` |
|       15 | 11656 | `		ph7_class_instance *pThis = 0;` |
|       15 | 11657 | `		ph7_class *pClass = 0;` |
|        - | 11658 | `		ph7_value *pValue;` |
|        - | 11659 | `		sxi32 rc;` |
|       15 | 11660 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11661 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11662 | `			if( pResult ){` |
|        - | 11663 | `				/* Assume a null return value */` |
|      ! 0 | 11664 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11665 | `			}` |
|      ! 0 | 11666 | `			return SXRET_OK;` |
|        - | 11667 | `		}` |
|        - | 11668 | `		/* Extract the class name or an instance of it */` |
|       15 | 11669 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 11670 | `		if( pValue ){` |
|       15 | 11671 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 11672 | `		}` |
|       15 | 11673 | `		if( pClass == 0 ){` |
|        - | 11674 | `			/* No such class,return NULL */` |
|      ! 0 | 11675 | `			if( pResult ){` |
|      ! 0 | 11676 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11677 | `			}` |
|      ! 0 | 11678 | `			return SXRET_OK;` |
|        - | 11679 | `		}` |
|       15 | 11680 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11681 | `			/* Point to the class instance */` |
|        9 | 11682 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 11683 | `		}` |
|        - | 11684 | `		/* Try to extract the method */` |
|       15 | 11685 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 11686 | `		if( pValue ){` |
|       15 | 11687 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 11688 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 11689 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 11690 | `			}` |
|        7 | 11691 | `		}` |
|       15 | 11692 | `		if( pMethod == 0 ){` |
|        - | 11693 | `			/* No such method,return NULL */` |
|      ! 0 | 11694 | `			if( pResult ){` |
|      ! 0 | 11695 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11696 | `			}` |
|      ! 0 | 11697 | `			return SXRET_OK;` |
|        - | 11698 | `		}` |
|        - | 11699 | `		/* Call the class method */` |
|       15 | 11700 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 11701 | `		return rc;` |
|        - | 11702 | `	}` |
|        - | 11703 | `	/* Create a new operand stack */` |
|      598 | 11704 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 11705 | `	if( aStack == 0 ){` |
|      ! 0 | 11706 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11707 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11708 | `		if( pResult ){` |
|        - | 11709 | `			/* Assume a null return value */` |
|      ! 0 | 11710 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11711 | `		}` |
|      ! 0 | 11712 | `		return SXERR_MEM;` |
|        - | 11713 | `	}` |
|        - | 11714 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 11715 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 11716 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11717 | `		/*` |
|        - | 11718 | `		 * Symisc eXtension:` |
|        - | 11719 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11720 | `		 */` |
|     1304 | 11721 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 11722 | `	}` |
|        - | 11723 | `	/* Push the function name */` |
|      598 | 11724 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 11725 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11726 | `	/* Emit the CALL istruction */` |
|      598 | 11727 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 11728 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 11729 | `	aInstr[0].iP2 = 0;` |
|      598 | 11730 | `	aInstr[0].p3  = 0;` |
|        - | 11731 | `	/* Emit the DONE instruction */` |
|      598 | 11732 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 11733 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 11734 | `	aInstr[1].iP2 = 0;` |
|      598 | 11735 | `	aInstr[1].p3  = 0;` |
|        - | 11736 | `	/* Execute the function body (if available) */` |
|        - | 11737 | `	{` |
|        - | 11738 | `		sxi32 rcExec;` |
|      598 | 11739 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11740 | `		/* Clean up the mess left behind */` |
|      598 | 11741 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11742 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 11743 | `		return rcExec;` |
|        - | 11744 | `	}` |
|      608 | 11745 |  |
|        - | 11746 | `/*` |
|        - | 11747 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11748 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11749 | ` * parameter.` |
|        - | 11750 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11751 | ` * return value indicates failure.` |
|        - | 11752 | ` */` |
|      240 | 11753 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11754 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11755 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11756 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11757 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11758 | `	)` |
|        1 | 11759 |  |
|        - | 11760 | `	ph7_value *pArg;` |
|        - | 11761 | `	SySet aArg;` |
|        - | 11762 | `	va_list ap;` |
|        - | 11763 | `	sxi32 rc;` |
|      241 | 11764 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11765 | `	/* Copy arguments one after one */` |
|      241 | 11766 | `	va_start(ap,pResult);` |
|      399 | 11767 | `	for(;;){` |
|      799 | 11768 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 11769 | `		if( pArg == 0 ){` |
|      241 | 11770 | `			break;` |
|        - | 11771 | `		}` |
|      559 | 11772 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11773 | `	}` |
|        - | 11774 | `	/* Call the core routine */` |
|      241 | 11775 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11776 | `	/* Cleanup */` |
|      241 | 11777 | `	SySetRelease(&aArg);` |
|      241 | 11778 | `	return rc;` |
|        1 | 11779 |  |
|        - | 11780 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11781 | `/*` |
|        - | 11782 | ` * bool defined(string $name)` |
|        - | 11783 | ` *  Checks whether a given named constant exists.` |
|        - | 11784 | ` * Parameter:` |
|        - | 11785 | ` *  Name of the desired constant.` |
|        - | 11786 | ` * Return` |
|        - | 11787 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11788 | ` */` |
|       20 | 11789 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11790 |  |
|        - | 11791 | `	const char *zName;` |
|       22 | 11792 | `	int nLen = 0;` |
|       22 | 11793 | `	int res = 0;` |
|       22 | 11794 | `	if( nArg < 1 ){` |
|        - | 11795 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11796 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11797 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11798 | `		return SXRET_OK;` |
|        - | 11799 | `	}` |
|        - | 11800 | `	/* Extract constant name */` |
|       22 | 11801 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11802 | `	/* Perform the lookup */` |
|       22 | 11803 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11804 | `		/* Already defined */` |
|       20 | 11805 | `		res = 1;` |
|        9 | 11806 | `	}` |
|       22 | 11807 | `	ph7_result_bool(pCtx,res);` |
|       22 | 11808 | `	return SXRET_OK;` |
|       12 | 11809 |  |
|        - | 11810 | `/*` |
|        - | 11811 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11812 | ` * below.` |
|        - | 11813 | ` */` |
|       10 | 11814 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11815 |  |
|       12 | 11816 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11817 | `	/* Expand constant value */` |
|       12 | 11818 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11819 |  |
|        - | 11820 | `/*` |
|        - | 11821 | ` * bool define(string $constant_name,expression value)` |
|        - | 11822 | ` *  Defines a named constant at runtime.` |
|        - | 11823 | ` * Parameter:` |
|        - | 11824 | ` *  $constant_name` |
|        - | 11825 | ` *   The name of the constant` |
|        - | 11826 | ` *  $value` |
|        - | 11827 | ` *   Constant value` |
|        - | 11828 | ` * Return:` |
|        - | 11829 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11830 | ` */` |
|       12 | 11831 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11832 |  |
|        - | 11833 | `	const char *zName;  /* Constant name */` |
|        - | 11834 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11835 | `	int nLen = 0;       /* Name length */` |
|        - | 11836 | `	sxi32 rc;` |
|       14 | 11837 | `	if( nArg < 2 ){` |
|        - | 11838 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11839 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11840 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11841 | `		return SXRET_OK;` |
|        - | 11842 | `	}` |
|       14 | 11843 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11844 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11845 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11846 | `		return SXRET_OK;` |
|        - | 11847 | `	}` |
|        - | 11848 | `	/* Extract constant name */` |
|       14 | 11849 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11850 | `	if( nLen < 1 ){` |
|      ! 0 | 11851 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11852 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11853 | `		return SXRET_OK;` |
|        - | 11854 | `	}` |
|        - | 11855 | `	/* Duplicate constant value */` |
|       14 | 11856 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11857 | `	if( pValue == 0 ){` |
|      ! 0 | 11858 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11859 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11860 | `		return SXRET_OK;` |
|        - | 11861 | `	}` |
|        - | 11862 | `	/* Initialize the memory object */` |
|       14 | 11863 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11864 | `	/* Register the constant */` |
|       14 | 11865 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11866 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11867 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11868 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11869 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11870 | `		return SXRET_OK;` |
|        - | 11871 | `	}` |
|        - | 11872 | `	/* Duplicate constant value */` |
|       14 | 11873 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11874 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11875 | `		/* Lower case the constant name */` |
|      ! 0 | 11876 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11877 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11878 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11879 | `				/* UTF-8 stream */` |
|      ! 0 | 11880 | `				zCur++;` |
|      ! 0 | 11881 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11882 | `					zCur++;` |
|      ! 0 | 11883 | `				}` |
|      ! 0 | 11884 | `				continue;` |
|        - | 11885 | `			}` |
|      ! 0 | 11886 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11887 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11888 | `				zCur[0] = (char)c;` |
|      ! 0 | 11889 | `			}` |
|      ! 0 | 11890 | `			zCur++;` |
|      ! 0 | 11891 | `		}` |
|        - | 11892 | `		/* Finally,register the constant */` |
|      ! 0 | 11893 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11894 | `	}` |
|        - | 11895 | `	/* All done,return TRUE */` |
|       14 | 11896 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11897 | `	return SXRET_OK;` |
|        8 | 11898 |  |
|        - | 11899 | `/*` |
|        - | 11900 | ` * value constant(string $name)` |
|        - | 11901 | ` *  Returns the value of a constant` |
|        - | 11902 | ` * Parameter` |
|        - | 11903 | ` *  $name` |
|        - | 11904 | ` *    Name of the constant.` |
|        - | 11905 | ` * Return` |
|        - | 11906 | ` *  Constant value or NULL if not defined.` |
|        - | 11907 | ` */` |
|        8 | 11908 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11909 |  |
|        - | 11910 | `	SyHashEntry *pEntry;` |
|        - | 11911 | `	ph7_constant *pCons;` |
|        - | 11912 | `	const char *zName; /* Constant name */` |
|        - | 11913 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11914 | `	int nLen;` |
|       10 | 11915 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11916 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11917 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11918 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11919 | `		return SXRET_OK;` |
|        - | 11920 | `	}` |
|        - | 11921 | `	/* Extract the constant name */` |
|       10 | 11922 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11923 | `	/* Perform the query */` |
|       10 | 11924 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11925 | `	if( pEntry == 0 ){` |
|        3 | 11926 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11927 | `		ph7_result_null(pCtx);` |
|        3 | 11928 | `		return SXRET_OK;` |
|        - | 11929 | `	}` |
|        8 | 11930 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11931 | `	/* Point to the structure that describe the constant */` |
|        8 | 11932 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11933 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11934 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11935 | `	/* Return that value */` |
|        8 | 11936 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11937 | `	/* Cleanup */` |
|        8 | 11938 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11939 | `	return SXRET_OK;` |
|        6 | 11940 |  |
|        - | 11941 | `/*` |
|        - | 11942 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11943 | ` * defined below.` |
|        - | 11944 | ` */` |
|      466 | 11945 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11946 |  |
|      467 | 11947 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11948 | `	ph7_value sName;` |
|        - | 11949 | `	sxi32 rc;` |
|        - | 11950 | `	/* Prepare the constant name for insertion */` |
|      467 | 11951 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 11952 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11953 | `	/* Perform the insertion */` |
|      467 | 11954 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 11955 | `	PH7_MemObjRelease(&sName);` |
|      467 | 11956 | `	return rc;` |
|        1 | 11957 |  |
|        - | 11958 | `/*` |
|        - | 11959 | ` * array get_defined_constants(void)` |
|        - | 11960 | ` *  Returns an associative array with the names of all defined` |
|        - | 11961 | ` *  constants.` |
|        - | 11962 | ` * Parameters` |
|        - | 11963 | ` *  NONE.` |
|        - | 11964 | ` * Returns` |
|        - | 11965 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11966 | ` */` |
|        2 | 11967 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11968 |  |
|        - | 11969 | `	ph7_value *pArray;` |
|        - | 11970 | `	/* Create the array first*/` |
|        3 | 11971 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11972 | `	if( pArray == 0 ){` |
|      ! 0 | 11973 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11974 | `		SXUNUSED(apArg);` |
|        - | 11975 | `		/* Return NULL */` |
|      ! 0 | 11976 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11977 | `		return SXRET_OK;` |
|        - | 11978 | `	}` |
|        - | 11979 | `	/* Fill the array with the defined constants */` |
|        3 | 11980 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11981 | `	/* Return the created array */` |
|        3 | 11982 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11983 | `	return SXRET_OK;` |
|        2 | 11984 |  |
|        - | 11985 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11986 | `/*` |
|        - | 11987 | ` * Section:` |
|        - | 11988 | ` *  Random numbers/string generators.` |
|        - | 11989 | ` * Status:` |
|        - | 11990 | ` *    Stable.` |
|        - | 11991 | ` */` |
|        - | 11992 | `/*` |
|        - | 11993 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11994 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 11995 | ` * implemented in src/sx/sxrand.c).` |
|        - | 11996 | ` */` |
|     2894 | 11997 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11998 |  |
|        - | 11999 | `	sxu32 iNum;` |
|     2896 | 12000 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2896 | 12001 | `	return iNum;` |
|        2 | 12002 |  |
|        - | 12003 | `/*` |
|        - | 12004 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12005 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12006 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12007 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12008 | ` */` |
|   236034 | 12009 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12010 |  |
|        - | 12011 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12012 | `	int i;` |
|        - | 12013 | `	/* Generate a binary string first */` |
|   236036 | 12014 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12015 | `	/* Turn the binary string into english based alphabet */` |
|  2596544 | 12016 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2360510 | 12017 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1180256 | 12018 | `	 }` |
|   236036 | 12019 |  |
|        - | 12020 | `/*` |
|        - | 12021 | ` * int rand()` |
|        - | 12022 | ` * int mt_rand()` |
|        - | 12023 | ` * int rand(int $min,int $max)` |
|        - | 12024 | ` * int mt_rand(int $min,int $max)` |
|        - | 12025 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12026 | ` * Parameter` |
|        - | 12027 | ` *  $min` |
|        - | 12028 | ` *    The lowest value to return (default: 0)` |
|        - | 12029 | ` *  $max` |
|        - | 12030 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12031 | ` * Return` |
|        - | 12032 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12033 | ` * Note:` |
|        - | 12034 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12035 | ` *  by te SQLite3 library.` |
|        - | 12036 | ` */` |
|       20 | 12037 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12038 |  |
|        - | 12039 | `	sxu32 iNum;` |
|        - | 12040 | `	/* Generate the random number */` |
|       21 | 12041 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12042 | `	if( nArg > 1 ){` |
|        - | 12043 | `		sxu32 iMin,iMax;` |
|        3 | 12044 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12045 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12046 | `		if( iMin < iMax ){` |
|        3 | 12047 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12048 | `			if( iDiv > 0 ){` |
|        3 | 12049 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12050 | `			}` |
|        1 | 12051 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12052 | `			iNum %= iMax;` |
|      ! 0 | 12053 | `		}` |
|        1 | 12054 | `	}` |
|        - | 12055 | `	/* Return the number */` |
|       21 | 12056 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12057 | `	return SXRET_OK;` |
|        1 | 12058 |  |
|        - | 12059 | `/*` |
|        - | 12060 | ` * int getrandmax(void)` |
|        - | 12061 | ` * int mt_getrandmax(void)` |
|        - | 12062 | ` * int rc4_getrandmax(void)` |
|        - | 12063 | ` *   Show largest possible random value` |
|        - | 12064 | ` * Return` |
|        - | 12065 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12066 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12067 | ` * Note:` |
|        - | 12068 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12069 | ` *  by te SQLite3 library.` |
|        - | 12070 | ` */` |
|        4 | 12071 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12072 |  |
|        2 | 12073 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12074 | `	SXUNUSED(apArg);` |
|        5 | 12075 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12076 | `	return SXRET_OK;` |
|        1 | 12077 |  |
|        - | 12078 | `/*` |
|        - | 12079 | ` * string rand_str()` |
|        - | 12080 | ` * string rand_str(int $len)` |
|        - | 12081 | ` *  Generate a random string (English alphabet).` |
|        - | 12082 | ` * Parameter` |
|        - | 12083 | ` *  $len` |
|        - | 12084 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12085 | ` * Return` |
|        - | 12086 | ` *   A pseudo random string.` |
|        - | 12087 | ` * Note:` |
|        - | 12088 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12089 | ` *  by te SQLite3 library.` |
|        - | 12090 | ` *  This function is a symisc extension.` |
|        - | 12091 | ` */` |
|      120 | 12092 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12093 |  |
|        - | 12094 | `	char zString[1024];` |
|      122 | 12095 | `	int iLen = 0x10;` |
|      122 | 12096 | `	if( nArg > 0 ){` |
|        - | 12097 | `		/* Get the desired length */` |
|      122 | 12098 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12099 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12100 | `			/* Default length */` |
|        3 | 12101 | `			iLen = 0x10;` |
|        1 | 12102 | `		}` |
|       60 | 12103 | `	}` |
|        - | 12104 | `	/* Generate the random string */` |
|      122 | 12105 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12106 | `	/* Return the generated string */` |
|      122 | 12107 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12108 | `	return SXRET_OK;` |
|        2 | 12109 |  |
|        - | 12110 | `/*` |
|        - | 12111 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12112 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12113 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12114 | ` */` |
|      488 | 12115 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12116 |  |
|      488 | 12117 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12118 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12119 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12120 | `			"TypeError",` |
|        - | 12121 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12122 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12123 | `			ph7_type_name(pArg)` |
|        - | 12124 | `			);` |
|        - | 12125 | `	}` |
|      483 | 12126 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12127 | `		int len;` |
|        9 | 12128 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12129 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12130 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12131 | `				"TypeError",` |
|        - | 12132 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12133 | `				zFunc,iArgPos,zParamName` |
|        - | 12134 | `				);` |
|        - | 12135 | `		}` |
|        2 | 12136 | `	}` |
|      479 | 12137 | `	return SXRET_OK;` |
|      245 | 12138 |  |
|        - | 12139 | `/*` |
|        - | 12140 | ` * int random_int(int $min, int $max)` |
|        - | 12141 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12142 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12143 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12144 | ` *  power-of-two mask covering the range.` |
|        - | 12145 | ` */` |
|      242 | 12146 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12147 |  |
|        - | 12148 | `	sxi64 iMin,iMax;` |
|        - | 12149 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12150 | `	unsigned int nAttempt;` |
|        - | 12151 | `	int rc;` |
|      243 | 12152 | `	if( nArg != 2 ){` |
|       10 | 12153 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12154 | `			"ArgumentCountError",` |
|        - | 12155 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12156 | `			nArg` |
|        - | 12157 | `			);` |
|        - | 12158 | `	}` |
|      237 | 12159 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12160 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12161 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12162 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12163 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12164 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12165 | `	if( iMin > iMax ){` |
|        3 | 12166 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12167 | `			"ValueError",` |
|        - | 12168 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12169 | `			);` |
|        - | 12170 | `	}` |
|      229 | 12171 | `	if( iMin == iMax ){` |
|        5 | 12172 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12173 | `		return SXRET_OK;` |
|        - | 12174 | `	}` |
|      225 | 12175 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12176 | `	uMask = uRange;` |
|      225 | 12177 | `	uMask \|= uMask >> 1;` |
|      225 | 12178 | `	uMask \|= uMask >> 2;` |
|      225 | 12179 | `	uMask \|= uMask >> 4;` |
|      225 | 12180 | `	uMask \|= uMask >> 8;` |
|      225 | 12181 | `	uMask \|= uMask >> 16;` |
|      225 | 12182 | `	uMask \|= uMask >> 32;` |
|      225 | 12183 | `	uResult = 0;` |
|      354 | 12184 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12185 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12186 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12187 | `		 * and the low-half mask would always read 0). */` |
|        - | 12188 | `		sxu64 uDraw;` |
|      354 | 12189 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12190 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12191 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12192 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12193 | `				"Exception",` |
|        - | 12194 | `				"Cannot gather sufficient random data"` |
|        - | 12195 | `				);` |
|        - | 12196 | `		}` |
|      354 | 12197 | `		uDraw &= uMask;` |
|      354 | 12198 | `		if( uDraw <= uRange ){` |
|      225 | 12199 | `			uResult = uDraw;` |
|      225 | 12200 | `			break;` |
|        - | 12201 | `		}` |
|       60 | 12202 | `	}` |
|      225 | 12203 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12204 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12205 | `			"Exception",` |
|        - | 12206 | `			"Cannot gather sufficient random data"` |
|        - | 12207 | `			);` |
|        - | 12208 | `	}` |
|      225 | 12209 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12210 | `	return SXRET_OK;` |
|      122 | 12211 |  |
|        - | 12212 | `/*` |
|        - | 12213 | ` * string random_bytes(int $length)` |
|        - | 12214 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12215 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12216 | ` */` |
|       24 | 12217 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12218 |  |
|        - | 12219 | `	sxi64 iLen;` |
|        - | 12220 | `	unsigned char zStack[256];` |
|        - | 12221 | `	void *pBuf;` |
|        - | 12222 | `	int rc;` |
|       25 | 12223 | `	int bHeap = 0;` |
|       25 | 12224 | `	if( nArg != 1 ){` |
|        7 | 12225 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12226 | `			"ArgumentCountError",` |
|        - | 12227 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12228 | `			nArg` |
|        - | 12229 | `			);` |
|        - | 12230 | `	}` |
|       21 | 12231 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12232 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12233 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12234 | `	if( iLen < 1 ){` |
|        5 | 12235 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12236 | `			"ValueError",` |
|        - | 12237 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12238 | `			);` |
|        - | 12239 | `	}` |
|        - | 12240 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12241 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12242 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12243 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12244 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12245 | `			"ValueError",` |
|        - | 12246 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12247 | `			);` |
|        - | 12248 | `	}` |
|       13 | 12249 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12250 | `		pBuf = zStack;` |
|        7 | 12251 | `	}else{` |
|      ! 0 | 12252 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12253 | `		if( pBuf == 0 ){` |
|      ! 0 | 12254 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12255 | `				"Exception",` |
|        - | 12256 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12257 | `				iLen` |
|        - | 12258 | `				);` |
|        - | 12259 | `		}` |
|      ! 0 | 12260 | `		bHeap = 1;` |
|        - | 12261 | `	}` |
|       13 | 12262 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12263 | `		if( bHeap ){` |
|      ! 0 | 12264 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12265 | `		}` |
|      ! 0 | 12266 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12267 | `			"Exception",` |
|        - | 12268 | `			"Cannot gather sufficient random data"` |
|        - | 12269 | `			);` |
|        - | 12270 | `	}` |
|       13 | 12271 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12272 | `	if( bHeap ){` |
|      ! 0 | 12273 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12274 | `	}` |
|       13 | 12275 | `	return SXRET_OK;` |
|       13 | 12276 |  |
|        - | 12277 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12278 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12279 | `/* Unique ID private data */` |
|        - | 12280 | `struct unique_id_data` |
|        - | 12281 |  |
|        - | 12282 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12283 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12284 | `};` |
|        - | 12285 | `/*` |
|        - | 12286 | ` * Binary to hex consumer callback.` |
|        - | 12287 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12288 | ` * defined below.` |
|        - | 12289 | ` */` |
|      192 | 12290 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12291 |  |
|      193 | 12292 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12293 | `	sxu32 nBuflen;` |
|        - | 12294 | `	/* Extract result buffer length */` |
|      193 | 12295 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12296 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12297 | `			/*` |
|        - | 12298 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12299 | `			 * string will be 13 characters long` |
|        - | 12300 | `			 */` |
|       25 | 12301 | `		return SXERR_ABORT;` |
|        - | 12302 | `	}` |
|      169 | 12303 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12304 | `		return SXERR_ABORT;` |
|        - | 12305 | `	}` |
|        - | 12306 | `	/* Safely Consume the hex stream */` |
|      169 | 12307 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12308 | `	return SXRET_OK;` |
|       97 | 12309 |  |
|        - | 12310 | `/*` |
|        - | 12311 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12312 | ` *  Generate a unique ID` |
|        - | 12313 | ` * Parameter` |
|        - | 12314 | ` * $prefix` |
|        - | 12315 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12316 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12317 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12318 | ` * $more_entropy` |
|        - | 12319 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12320 | ` *  that the result will be unique.` |
|        - | 12321 | ` * Return` |
|        - | 12322 | ` *  Returns the unique identifier, as a string.` |
|        - | 12323 | ` */` |
|       24 | 12324 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12325 |  |
|        - | 12326 | `	struct unique_id_data sUniq;` |
|        - | 12327 | `	unsigned char zDigest[20];` |
|       25 | 12328 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12329 | `	const char *zPrefix;` |
|        - | 12330 | `	SHA1Context sCtx;` |
|        - | 12331 | `	char zRandom[7];` |
|        - | 12332 | `	int nPrefix;` |
|        - | 12333 | `	int entropy;` |
|        - | 12334 | `	/* Generate a random string first */` |
|       25 | 12335 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12336 | `	/* Initialize fields */` |
|       25 | 12337 | `	zPrefix = 0;` |
|       25 | 12338 | `	nPrefix = 0;` |
|       25 | 12339 | `	entropy = 0;` |
|       25 | 12340 | `	if( nArg > 0 ){` |
|        - | 12341 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12342 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12343 | `		if( nArg > 1 ){` |
|      ! 0 | 12344 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12345 | `		}` |
|      ! 0 | 12346 | `	}` |
|       25 | 12347 | `	SHA1Init(&sCtx);` |
|        - | 12348 | `	/* Generate the random ID */` |
|       25 | 12349 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12350 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12351 | `	}` |
|        - | 12352 | `	/* Append the random ID */` |
|       25 | 12353 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12354 | `	/* Append the random string */` |
|       25 | 12355 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12356 | `	/* Increment the number */` |
|       25 | 12357 | `	pVm->unique_id++;` |
|       25 | 12358 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12359 | `	/* Hexify the digest */` |
|       25 | 12360 | `	sUniq.pCtx = pCtx;` |
|       25 | 12361 | `	sUniq.entropy = entropy;` |
|       25 | 12362 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12363 | `	/* All done */` |
|       25 | 12364 | `	return PH7_OK;` |
|        1 | 12365 |  |
|        - | 12366 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12367 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12368 | `/*` |
|        - | 12369 | ` * Section:` |
|        - | 12370 | ` *  Language construct implementation as foreign functions.` |
|        - | 12371 | ` * Status:` |
|        - | 12372 | ` *    Stable.` |
|        - | 12373 | ` */` |
|        - | 12374 | `/*` |
|        - | 12375 | ` * void echo($string...)` |
|        - | 12376 | ` *  Output one or more messages.` |
|        - | 12377 | ` * Parameters` |
|        - | 12378 | ` *  $string` |
|        - | 12379 | ` *   Message to output.` |
|        - | 12380 | ` * Return` |
|        - | 12381 | ` *  NULL.` |
|        - | 12382 | ` */` |
|      ! 0 | 12383 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12384 |  |
|        - | 12385 | `	const char *zData;` |
|      ! 0 | 12386 | `	int nDataLen = 0;` |
|        - | 12387 | `	ph7_vm *pVm;` |
|        - | 12388 | `	int i,rc;` |
|        - | 12389 | `	/* Point to the target VM */` |
|      ! 0 | 12390 | `	pVm = pCtx->pVm;` |
|        - | 12391 | `	/* Output */` |
|      ! 0 | 12392 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12393 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12394 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12395 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12396 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12397 | `			if( rc == SXERR_ABORT ){` |
|        - | 12398 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12399 | `				return PH7_ABORT;` |
|        - | 12400 | `			}` |
|      ! 0 | 12401 | `		}` |
|      ! 0 | 12402 | `	}` |
|      ! 0 | 12403 | `	return SXRET_OK;` |
|      ! 0 | 12404 |  |
|        - | 12405 | `/*` |
|        - | 12406 | ` * int print($string...)` |
|        - | 12407 | ` *  Output one or more messages.` |
|        - | 12408 | ` * Parameters` |
|        - | 12409 | ` *  $string` |
|        - | 12410 | ` *   Message to output.` |
|        - | 12411 | ` * Return` |
|        - | 12412 | ` *  1 always.` |
|        - | 12413 | ` */` |
|        2 | 12414 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12415 |  |
|        - | 12416 | `	const char *zData;` |
|        3 | 12417 | `	int nDataLen = 0;` |
|        - | 12418 | `	ph7_vm *pVm;` |
|        - | 12419 | `	int i,rc;` |
|        - | 12420 | `	/* Point to the target VM */` |
|        3 | 12421 | `	pVm = pCtx->pVm;` |
|        - | 12422 | `	/* Output */` |
|        5 | 12423 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12424 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12425 | `		if( nDataLen > 0 ){` |
|        3 | 12426 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12427 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12428 | `			if( rc == SXERR_ABORT ){` |
|        - | 12429 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12430 | `				return PH7_ABORT;` |
|        - | 12431 | `			}` |
|        1 | 12432 | `		}` |
|        2 | 12433 | `	}` |
|        - | 12434 | `	/* Return 1 */` |
|        3 | 12435 | `	ph7_result_int(pCtx,1);` |
|        3 | 12436 | `	return SXRET_OK;` |
|        2 | 12437 |  |
|        - | 12438 | `/*` |
|        - | 12439 | ` * void exit(string $msg)` |
|        - | 12440 | ` * void exit(int $status)` |
|        - | 12441 | ` * void die(string $ms)` |
|        - | 12442 | ` * void die(int $status)` |
|        - | 12443 | ` *   Output a message and terminate program execution.` |
|        - | 12444 | ` * Parameter` |
|        - | 12445 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12446 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12447 | ` *  and not printed` |
|        - | 12448 | ` * Return` |
|        - | 12449 | ` *  NULL` |
|        - | 12450 | ` */` |
|      ! 0 | 12451 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12452 |  |
|      ! 0 | 12453 | `	if( nArg > 0 ){` |
|      ! 0 | 12454 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12455 | `			const char *zData;` |
|      ! 0 | 12456 | `			int iLen = 0;` |
|        - | 12457 | `			/* Print exit message */` |
|      ! 0 | 12458 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12459 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12460 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12461 | `			sxi32 iExitStatus;` |
|        - | 12462 | `			/* Record exit status code */` |
|      ! 0 | 12463 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12464 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12465 | `		}` |
|      ! 0 | 12466 | `	}` |
|        - | 12467 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12468 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12469 | `	 */` |
|      ! 0 | 12470 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12471 | `	return PH7_ABORT;` |
|      ! 0 | 12472 |  |
|        - | 12473 | `/*` |
|        - | 12474 | ` * bool isset($var,...)` |
|        - | 12475 | ` *  Finds out whether a variable is set.` |
|        - | 12476 | ` * Parameters` |
|        - | 12477 | ` *  One or more variable to check.` |
|        - | 12478 | ` * Return` |
|        - | 12479 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12480 | ` */` |
|    92676 | 12481 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12482 |  |
|        - | 12483 | `	ph7_value *pObj;` |
|    92678 | 12484 | `	int res = 0;` |
|        - | 12485 | `	int i;` |
|    92678 | 12486 | `	if( nArg < 1 ){` |
|        - | 12487 | `		/* Missing arguments,return false */` |
|      ! 0 | 12488 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12489 | `		return SXRET_OK;` |
|        - | 12490 | `	}` |
|        - | 12491 | `	/* Iterate over available arguments */` |
|   121136 | 12492 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92688 | 12493 | `		pObj = apArg[i];` |
|    92688 | 12494 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12495 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12496 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12497 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63256 | 12498 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12499 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12500 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12501 | `			}` |
|    31627 | 12502 | `		}` |
|    92688 | 12503 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92688 | 12504 | `		if( !res ){` |
|        - | 12505 | `			/* Variable not set,return FALSE */` |
|    64230 | 12506 | `			ph7_result_bool(pCtx,0);` |
|    64230 | 12507 | `			return SXRET_OK;` |
|        - | 12508 | `		}` |
|    14231 | 12509 | `	}` |
|        - | 12510 | `	/* All given variable are set,return TRUE */` |
|    28450 | 12511 | `	ph7_result_bool(pCtx,1);` |
|    28450 | 12512 | `	return SXRET_OK;` |
|    46340 | 12513 |  |
|        - | 12514 | `/*` |
|        - | 12515 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12516 | ` * frame,the reference table and discard it's contents.` |
|        - | 12517 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12518 | ` */` |
|  3160868 | 12519 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12520 |  |
|        - | 12521 | `	ph7_value *pObj;` |
|        - | 12522 | `	VmRefObj *pRef;` |
|  3160870 | 12523 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3160870 | 12524 | `	if( pObj ){` |
|        - | 12525 | `		/* Release the object */` |
|  3160870 | 12526 | `		PH7_MemObjRelease(pObj);` |
|  1580434 | 12527 | `	}` |
|        - | 12528 | `	/* Remove old reference links */` |
|  3160870 | 12529 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3160870 | 12530 | `	if( pRef ){` |
|  3160864 | 12531 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12532 | `		/* Unlink from the reference table */` |
|  3160864 | 12533 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3160864 | 12534 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12535 | `			VmSlot sFree;` |
|        - | 12536 | `			/* Restore to the free list */` |
|  3160856 | 12537 | `			sFree.nIdx = nObjIdx;` |
|  3160856 | 12538 | `			sFree.pUserData = 0;` |
|  3160856 | 12539 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1580427 | 12540 | `		}` |
|  1580431 | 12541 | `	}` |
|  3160870 | 12542 | `	return SXRET_OK;` |
|        2 | 12543 |  |
|        - | 12544 | `/*` |
|        - | 12545 | ` * void unset($var,...)` |
|        - | 12546 | ` *   Unset one or more given variable.` |
|        - | 12547 | ` * Parameters` |
|        - | 12548 | ` *  One or more variable to unset.` |
|        - | 12549 | ` * Return` |
|        - | 12550 | ` *  Nothing.` |
|        - | 12551 | ` */` |
|     7504 | 12552 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12553 |  |
|        - | 12554 | `	ph7_value *pObj;` |
|        - | 12555 | `	ph7_vm *pVm;` |
|        - | 12556 | `	int i;` |
|        - | 12557 | `	/* Point to the target VM */` |
|     7506 | 12558 | `	pVm = pCtx->pVm;` |
|        - | 12559 | `	/* Iterate and unset */` |
|    15010 | 12560 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7506 | 12561 | `		pObj = apArg[i];` |
|     7506 | 12562 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      818 | 12563 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12564 | `				/* Throw an error */` |
|      ! 0 | 12565 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12566 | `			}` |
|      410 | 12567 | `		}else{` |
|     6690 | 12568 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12569 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6690 | 12570 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6684 | 12571 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3341 | 12572 | `			}` |
|        - | 12573 | `		}` |
|     3754 | 12574 | `	}` |
|     7506 | 12575 | `	return SXRET_OK;` |
|        2 | 12576 |  |
|        - | 12577 | `/*` |
|        - | 12578 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12579 | ` */` |
|      116 | 12580 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12581 |  |
|      117 | 12582 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12583 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12584 | `	ph7_value *pObj;` |
|        - | 12585 | `	sxu32 nIdx;` |
|        - | 12586 | `	/* Extract the memory object */` |
|      117 | 12587 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12588 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12589 | `	if( pObj ){` |
|      117 | 12590 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12591 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12592 | `				SyString sName;` |
|        - | 12593 | `				ph7_value sKey;` |
|        - | 12594 | `				/* Perform the insertion */` |
|      115 | 12595 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12596 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12597 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12598 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12599 | `			}` |
|       57 | 12600 | `		}` |
|       58 | 12601 | `	}` |
|      117 | 12602 | `	return SXRET_OK;` |
|        1 | 12603 |  |
|        - | 12604 | `/*` |
|        - | 12605 | ` * array get_defined_vars(void)` |
|        - | 12606 | ` *  Returns an array of all defined variables.` |
|        - | 12607 | ` * Parameter` |
|        - | 12608 | ` *  None` |
|        - | 12609 | ` * Return` |
|        - | 12610 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12611 | ` */` |
|        2 | 12612 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12613 |  |
|        3 | 12614 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12615 | `	ph7_value *pArray;` |
|        - | 12616 | `	/* Create a new array */` |
|        3 | 12617 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12618 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12619 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12620 | `		SXUNUSED(apArg);` |
|        - | 12621 | `		/* Return NULL */` |
|      ! 0 | 12622 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12623 | `		return SXRET_OK;` |
|        - | 12624 | `	}` |
|        - | 12625 | `	/* Superglobals first */` |
|        3 | 12626 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12627 | `	/* Then variable defined in the current frame */` |
|        3 | 12628 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12629 | `	/* Finally,return the created array */` |
|        3 | 12630 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12631 | `	return SXRET_OK;` |
|        2 | 12632 |  |
|        - | 12633 | `/*` |
|        - | 12634 | ` * bool gettype($var)` |
|        - | 12635 | ` *  Get the type of a variable` |
|        - | 12636 | ` * Parameters` |
|        - | 12637 | ` *   $var` |
|        - | 12638 | ` *    The variable being type checked.` |
|        - | 12639 | ` * Return` |
|        - | 12640 | ` *   String representation of the given variable type.` |
|        - | 12641 | ` */` |
|       32 | 12642 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12643 |  |
|       34 | 12644 | `	const char *zType = "Empty";` |
|       34 | 12645 | `	if( nArg > 0 ){` |
|       34 | 12646 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12647 | `	}` |
|        - | 12648 | `	/* Return the variable type */` |
|       34 | 12649 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12650 | `	return SXRET_OK;` |
|        2 | 12651 |  |
|        - | 12652 | `/*` |
|        - | 12653 | ` * string get_resource_type(resource $handle)` |
|        - | 12654 | ` *  This function gets the type of the given resource.` |
|        - | 12655 | ` * Parameters` |
|        - | 12656 | ` *  $handle` |
|        - | 12657 | ` *  The evaluated resource handle.` |
|        - | 12658 | ` * Return` |
|        - | 12659 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12660 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12661 | ` *  the return value will be the string Unknown.` |
|        - | 12662 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12663 | ` *  is not a resource.` |
|        - | 12664 | ` */` |
|        2 | 12665 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12666 |  |
|        3 | 12667 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12668 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12669 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12670 | `		return PH7_OK;` |
|        - | 12671 | `	}` |
|        3 | 12672 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12673 | `	return SXRET_OK;` |
|        2 | 12674 |  |
|        - | 12675 | `/*` |
|        - | 12676 | ` * void var_dump(expression,....)` |
|        - | 12677 | ` *   var_dump � Dumps information about a variable` |
|        - | 12678 | ` * Parameters` |
|        - | 12679 | ` *   One or more expression to dump.` |
|        - | 12680 | ` * Returns` |
|        - | 12681 | ` *  Nothing.` |
|        - | 12682 | ` */` |
|      218 | 12683 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12684 |  |
|        - | 12685 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12686 | `	int i;` |
|      220 | 12687 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12688 | `	/* Dump one or more expressions */` |
|      444 | 12689 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12690 | `		ph7_value *pObj = apArg[i];` |
|        - | 12691 | `		/* Reset the working buffer */` |
|      226 | 12692 | `		SyBlobReset(&sDump);` |
|        - | 12693 | `		/* Dump the given expression */` |
|      226 | 12694 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12695 | `		/* Output */` |
|      226 | 12696 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12697 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12698 | `		}` |
|      114 | 12699 | `	}` |
|        - | 12700 | `	/* Release the working buffer */` |
|      220 | 12701 | `	SyBlobRelease(&sDump);` |
|      220 | 12702 | `	return SXRET_OK;` |
|        2 | 12703 |  |
|        - | 12704 | `/*` |
|        - | 12705 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12706 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12707 | ` * Parameters` |
|        - | 12708 | ` *   expression: Expression to dump` |
|        - | 12709 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12710 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12711 | ` *            print_r() will return the information rather than print it.` |
|        - | 12712 | ` * Return` |
|        - | 12713 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12714 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12715 | ` */` |
|       16 | 12716 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12717 |  |
|       17 | 12718 | `	int ret_string = 0;` |
|        - | 12719 | `	SyBlob sDump;` |
|       17 | 12720 | `	if( nArg < 1 ){` |
|        - | 12721 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12722 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12723 | `		return SXRET_OK;` |
|        - | 12724 | `	}` |
|       17 | 12725 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12726 | `	if ( nArg > 1 ){` |
|        - | 12727 | `		/* Where to redirect output */` |
|       11 | 12728 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12729 | `	}` |
|        - | 12730 | `	/* Generate dump */` |
|       17 | 12731 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12732 | `	if( !ret_string ){` |
|        - | 12733 | `		/* Output dump */` |
|        7 | 12734 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12735 | `		/* Return true */` |
|        7 | 12736 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12737 | `	}else{` |
|        - | 12738 | `		/* Generated dump as return value */` |
|       11 | 12739 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12740 | `	}` |
|        - | 12741 | `	/* Release the working buffer */` |
|       17 | 12742 | `	SyBlobRelease(&sDump);` |
|       17 | 12743 | `	return SXRET_OK;` |
|        9 | 12744 |  |
|        - | 12745 | `/*` |
|        - | 12746 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12747 | ` * Same job as print_r. (see coment above)` |
|        - | 12748 | ` */` |
|        2 | 12749 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12750 |  |
|        3 | 12751 | `	int ret_string = 0;` |
|        - | 12752 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12753 | `	if( nArg < 1 ){` |
|        - | 12754 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12755 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12756 | `		return SXRET_OK;` |
|        - | 12757 | `	}` |
|        3 | 12758 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12759 | `	if ( nArg > 1 ){` |
|        - | 12760 | `		/* Where to redirect output */` |
|        3 | 12761 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12762 | `	}` |
|        - | 12763 | `	/* Generate dump */` |
|        3 | 12764 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12765 | `	if( !ret_string ){` |
|        - | 12766 | `		/* Output dump */` |
|      ! 0 | 12767 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12768 | `		/* Return NULL */` |
|      ! 0 | 12769 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12770 | `	}else{` |
|        - | 12771 | `		/* Generated dump as return value */` |
|        3 | 12772 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12773 | `	}` |
|        - | 12774 | `	/* Release the working buffer */` |
|        3 | 12775 | `	SyBlobRelease(&sDump);` |
|        3 | 12776 | `	return SXRET_OK;` |
|        2 | 12777 |  |
|        - | 12778 | `/*` |
|        - | 12779 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12780 | ` *  Set/get the various assert flags.` |
|        - | 12781 | ` * Parameter` |
|        - | 12782 | ` * $what` |
|        - | 12783 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12784 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12785 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12786 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12787 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12788 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12789 | ` * $value` |
|        - | 12790 | ` *   An optional new value for the option.` |
|        - | 12791 | ` * Return` |
|        - | 12792 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12793 | ` */` |
|       28 | 12794 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12795 |  |
|       30 | 12796 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12797 | `	int iOption;` |
|        - | 12798 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12799 | `	if( nArg < 1 ){` |
|        3 | 12800 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12801 | `			"ArgumentCountError",` |
|        - | 12802 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12803 | `			);` |
|        - | 12804 | `	}` |
|        - | 12805 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12806 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12807 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12808 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12809 | `			"TypeError",` |
|        - | 12810 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12811 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12812 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12813 | `			);` |
|        - | 12814 | `	}` |
|       28 | 12815 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12816 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12817 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12818 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12819 | `	switch( iOption ){` |
|        5 | 12820 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12821 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12822 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12823 | `		if( nArg > 1 ){` |
|        5 | 12824 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12825 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12826 | `			}else{` |
|        3 | 12827 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12828 | `			}` |
|        2 | 12829 | `		}` |
|       12 | 12830 | `		break;` |
|        1 | 12831 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12832 | `		/* Return old callback or null */` |
|        3 | 12833 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12834 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12835 | `		}else{` |
|        3 | 12836 | `			ph7_result_null(pCtx);` |
|        - | 12837 | `		}` |
|        3 | 12838 | `		if( nArg > 1 ){` |
|      ! 0 | 12839 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12840 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12841 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12842 | `			}else{` |
|      ! 0 | 12843 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12844 | `			}` |
|      ! 0 | 12845 | `		}` |
|        3 | 12846 | `		break;` |
|        5 | 12847 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12848 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12849 | `		if( nArg > 1 ){` |
|        5 | 12850 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12851 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12852 | `			}else{` |
|        3 | 12853 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12854 | `			}` |
|        2 | 12855 | `		}` |
|       11 | 12856 | `		break;` |
|      ! 0 | 12857 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12858 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12859 | `		break;` |
|        1 | 12860 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12861 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12862 | `		break;` |
|      ! 0 | 12863 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12864 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12865 | `		break;` |
|        1 | 12866 | `	default:` |
|        - | 12867 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12868 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12869 | `			"ValueError",` |
|        - | 12870 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12871 | `			);` |
|        - | 12872 | `	}` |
|       26 | 12873 | `	return PH7_OK;` |
|       16 | 12874 |  |
|        - | 12875 | `/*` |
|        - | 12876 | ` * bool assert(mixed $assertion)` |
|        - | 12877 | ` *  Checks if assertion is FALSE.` |
|        - | 12878 | ` * Parameter` |
|        - | 12879 | ` *  $assertion` |
|        - | 12880 | ` *    The assertion to test.` |
|        - | 12881 | ` * Return` |
|        - | 12882 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12883 | ` */` |
|       24 | 12884 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12885 |  |
|       26 | 12886 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12887 | `	int iFlags,iResult;` |
|        - | 12888 | `	const char *zDesc;` |
|        - | 12889 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12890 | `	if( nArg < 1 ){` |
|        3 | 12891 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12892 | `			"ArgumentCountError",` |
|        - | 12893 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12894 | `			);` |
|        - | 12895 | `	}` |
|       24 | 12896 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12897 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12898 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12899 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12900 | `		return PH7_OK;` |
|        - | 12901 | `	}` |
|        - | 12902 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12903 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12904 | `	if( !iResult ){` |
|        - | 12905 | `		/* Assertion failed */` |
|        - | 12906 | `		/* Extract optional description */` |
|       13 | 12907 | `		zDesc = 0;` |
|       13 | 12908 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12909 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12910 | `		}` |
|       13 | 12911 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12912 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12913 | `			ph7_value sFile,sLine;` |
|        - | 12914 | `			ph7_value *apCbArg[3];` |
|        - | 12915 | `			SyString *pFile;` |
|        - | 12916 | `			/* Extract the processed script */` |
|      ! 0 | 12917 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12918 | `			if( pFile == 0 ){` |
|      ! 0 | 12919 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12920 | `			}` |
|        - | 12921 | `			/* Invoke the callback */` |
|      ! 0 | 12922 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12923 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12924 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12925 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12926 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12927 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12928 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12929 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12930 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12931 | `		}` |
|       13 | 12932 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12933 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12934 | `			return PH7_ABORT;` |
|        - | 12935 | `		}` |
|        - | 12936 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12937 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12938 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12939 | `				"AssertionError",` |
|        - | 12940 | `				"%s",` |
|        1 | 12941 | `				zDesc` |
|        - | 12942 | `				);` |
|      ! 0 | 12943 | `		}else{` |
|       11 | 12944 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12945 | `				"AssertionError",` |
|        - | 12946 | `				"assert(false)"` |
|        - | 12947 | `				);` |
|        - | 12948 | `		}` |
|        - | 12949 | `	}` |
|        - | 12950 | `	/* Assertion passed */` |
|       11 | 12951 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12952 | `	return PH7_OK;` |
|       14 | 12953 |  |
|        - | 12954 | `/*` |
|        - | 12955 | ` * Section:` |
|        - | 12956 | ` *  Error reporting functions.` |
|        - | 12957 | ` * Status:` |
|        - | 12958 | ` *    Stable.` |
|        - | 12959 | ` */` |
|        - | 12960 | `/*` |
|        - | 12961 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12962 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12963 | ` * Parameters` |
|        - | 12964 | ` *  $error_msg` |
|        - | 12965 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12966 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12967 | ` * $error_type` |
|        - | 12968 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12969 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12970 | ` * Return` |
|        - | 12971 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12972 | ` */` |
|       12 | 12973 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12974 |  |
|       14 | 12975 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12976 | `	int rc = PH7_OK;` |
|       14 | 12977 | `	if( nArg > 0 ){` |
|        - | 12978 | `		const char *zErr;` |
|        - | 12979 | `		int nLen;` |
|        - | 12980 | `		/* Extract the error message */` |
|       12 | 12981 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12982 | `		if( nArg > 1 ){` |
|        - | 12983 | `			/* Extract the error type */` |
|       12 | 12984 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12985 | `			switch( nErr ){` |
|        1 | 12986 | `			case 1:   /* E_ERROR */` |
|        - | 12987 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12988 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12989 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12990 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12991 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12992 | `				break;` |
|        1 | 12993 | `			case 2:   /* E_WARNING */` |
|        - | 12994 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12995 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12996 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12997 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12998 | `				break;` |
|        3 | 12999 | `			default:` |
|        8 | 13000 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13001 | `				break;` |
|        - | 13002 | `			}` |
|        5 | 13003 | `		}` |
|        - | 13004 | `		/* Report error */` |
|       12 | 13005 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13006 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13007 | `			return rc;` |
|        - | 13008 | `		}` |
|        - | 13009 | `		/* Return true */` |
|       12 | 13010 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13011 | `	}else{` |
|        - | 13012 | `		/* Missing arguments,return FALSE */` |
|        3 | 13013 | `		ph7_result_bool(pCtx,0);` |
|        - | 13014 | `	}` |
|       14 | 13015 | `	return rc;` |
|        8 | 13016 |  |
|        - | 13017 | `/*` |
|        - | 13018 | ` * int error_reporting([int $level])` |
|        - | 13019 | ` *  Sets which PHP errors are reported.` |
|        - | 13020 | ` * Parameters` |
|        - | 13021 | ` *  $level` |
|        - | 13022 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13023 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13024 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13025 | ` *   levels will not always behave as expected.` |
|        - | 13026 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13027 | ` *   in the predefined constants.` |
|        - | 13028 | ` * Return` |
|        - | 13029 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13030 | ` *   parameter is given.` |
|        - | 13031 | ` */` |
|       32 | 13032 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13033 |  |
|       34 | 13034 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13035 | `	int nOld;` |
|        - | 13036 | `	/* Extract the old reporting level */` |
|       34 | 13037 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13038 | `	if( nArg > 0 ){` |
|        - | 13039 | `		int nNew;` |
|        - | 13040 | `		/* Extract the desired error reporting level */` |
|       28 | 13041 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13042 | `		if( !nNew ){` |
|        - | 13043 | `			/* Do not report errors at all */` |
|        5 | 13044 | `			pVm->bErrReport = 0;` |
|        3 | 13045 | `		}else{` |
|        - | 13046 | `			/* Report all errors */` |
|       24 | 13047 | `			pVm->bErrReport = 1;` |
|        - | 13048 | `		}` |
|       13 | 13049 | `	}` |
|        - | 13050 | `	/* Return the old level */` |
|       34 | 13051 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13052 | `	return PH7_OK;` |
|        2 | 13053 |  |
|        - | 13054 | `/*` |
|        - | 13055 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13056 | ` *  Send an error message somewhere.` |
|        - | 13057 | ` * Parameter` |
|        - | 13058 | ` *  $message` |
|        - | 13059 | ` *   The error message that should be logged.` |
|        - | 13060 | ` *  $message_type` |
|        - | 13061 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13062 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13063 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13064 | ` *       This is the default option.` |
|        - | 13065 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13066 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13067 | ` *    2  No longer an option.` |
|        - | 13068 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13069 | ` *       to the end of the message string.` |
|        - | 13070 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13071 | ` *  $destination` |
|        - | 13072 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13073 | ` *  $extra_headers` |
|        - | 13074 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13075 | ` * Return` |
|        - | 13076 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13077 | ` * NOTE:` |
|        - | 13078 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13079 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13080 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13081 | ` *  Otherwise this function is no-op.` |
|        - | 13082 | ` */` |
|        4 | 13083 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13084 |  |
|        - | 13085 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13086 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13087 | `	int iType = 0;` |
|        5 | 13088 | `	if( nArg < 1 ){` |
|        - | 13089 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13090 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13091 | `		return PH7_OK;` |
|        - | 13092 | `	}` |
|        5 | 13093 | `	if( pVm->xErrLog  ){` |
|        - | 13094 | `		/* Invoke the user callback */` |
|      ! 0 | 13095 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13096 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13097 | `		if( nArg > 1 ){` |
|      ! 0 | 13098 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13099 | `			if( nArg > 2 ){` |
|      ! 0 | 13100 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13101 | `				if( nArg > 3 ){` |
|      ! 0 | 13102 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13103 | `				}` |
|      ! 0 | 13104 | `			}` |
|      ! 0 | 13105 | `		}` |
|      ! 0 | 13106 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13107 | `	}` |
|        - | 13108 | `	/* Retun TRUE */` |
|        5 | 13109 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13110 | `	return PH7_OK;` |
|        3 | 13111 |  |
|        - | 13112 | `/*` |
|        - | 13113 | ` * bool restore_exception_handler(void)` |
|        - | 13114 | ` *  Restores the previously defined exception handler function.` |
|        - | 13115 | ` * Parameter` |
|        - | 13116 | ` *  None` |
|        - | 13117 | ` * Return` |
|        - | 13118 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13119 | ` */` |
|        4 | 13120 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13121 |  |
|        5 | 13122 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13123 | `	ph7_value *pOld,*pNew;` |
|        - | 13124 | `	/* Point to the old and the new handler */` |
|        5 | 13125 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13126 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13127 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13128 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13129 | `		SXUNUSED(apArg);` |
|        - | 13130 | `		/* No installed handler,return FALSE */` |
|        5 | 13131 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13132 | `		return PH7_OK;` |
|        - | 13133 | `	}` |
|        - | 13134 | `	/* Copy the old handler */` |
|      ! 0 | 13135 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13136 | `	PH7_MemObjRelease(pOld);` |
|        - | 13137 | `	/* Return TRUE */` |
|      ! 0 | 13138 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13139 | `	return PH7_OK;` |
|        3 | 13140 |  |
|        - | 13141 | `/*` |
|        - | 13142 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13143 | ` *  Sets a user-defined exception handler function.` |
|        - | 13144 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13145 | ` * NOTE` |
|        - | 13146 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13147 | ` *  the satndard PHP engine.` |
|        - | 13148 | ` * Parameters` |
|        - | 13149 | ` *  $exception_handler` |
|        - | 13150 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13151 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13152 | ` *   that was thrown.` |
|        - | 13153 | ` *  Note:` |
|        - | 13154 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13155 | ` * Return` |
|        - | 13156 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13157 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13158 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13159 | ` */` |
|        4 | 13160 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13161 |  |
|        6 | 13162 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13163 | `	ph7_value *pOld,*pNew;` |
|        - | 13164 | `	/* Point to the old and the new handler */` |
|        6 | 13165 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13166 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13167 | `	/* Return the old handler */` |
|        6 | 13168 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13169 | `	if( nArg > 0 ){` |
|        6 | 13170 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13171 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13172 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13173 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13174 | `		}else{` |
|        6 | 13175 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13176 | `			/* Install the new handler */` |
|        6 | 13177 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13178 | `		}` |
|        2 | 13179 | `	}` |
|        6 | 13180 | `	return PH7_OK;` |
|        2 | 13181 |  |
|        - | 13182 | `/*` |
|        - | 13183 | ` * bool restore_error_handler(void)` |
|        - | 13184 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13185 | ` * Parameters:` |
|        - | 13186 | ` *  None.` |
|        - | 13187 | ` * Return` |
|        - | 13188 | ` *  Always TRUE.` |
|        - | 13189 | ` */` |
|        6 | 13190 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13191 |  |
|        7 | 13192 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13193 | `	ph7_value *pOld,*pNew;` |
|        - | 13194 | `	/* Point to the old and the new handler */` |
|        7 | 13195 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13196 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13197 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13198 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13199 | `		SXUNUSED(apArg);` |
|        - | 13200 | `		/* No installed callback,return FALSE */` |
|        7 | 13201 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13202 | `		return PH7_OK;` |
|        - | 13203 | `	}` |
|        - | 13204 | `	/* Copy the old callback */` |
|      ! 0 | 13205 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13206 | `	PH7_MemObjRelease(pOld);` |
|        - | 13207 | `	/* Return TRUE */` |
|      ! 0 | 13208 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13209 | `	return PH7_OK;` |
|        4 | 13210 |  |
|        - | 13211 | `/*` |
|        - | 13212 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13213 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13214 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13215 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13216 | ` *  Sets a user-defined error handler function.` |
|        - | 13217 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13218 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13219 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13220 | ` *  conditions (using trigger_error()).` |
|        - | 13221 | ` * Parameters` |
|        - | 13222 | ` *  $error_handler` |
|        - | 13223 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13224 | ` *   describing the error.` |
|        - | 13225 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13226 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13227 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13228 | ` *   The function can be shown as:` |
|        - | 13229 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13230 | ` *     errno` |
|        - | 13231 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13232 | ` *   errstr` |
|        - | 13233 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13234 | ` *   errfile` |
|        - | 13235 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13236 | ` *     was raised in, as a string.` |
|        - | 13237 | ` *  Note:` |
|        - | 13238 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13239 | ` * Return` |
|        - | 13240 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13241 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13242 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13243 | ` */` |
|    10856 | 13244 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13245 |  |
|    10858 | 13246 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13247 | `	ph7_value *pOld,*pNew;` |
|        - | 13248 | `	/* Point to the old and the new handler */` |
|    10858 | 13249 | `	pOld = &pVm->aErrCB[0];` |
|    10858 | 13250 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13251 | `	/* Return the old handler */` |
|    10858 | 13252 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10858 | 13253 | `	if( nArg > 0 ){` |
|    10858 | 13254 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13255 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5423 | 13256 | `			PH7_MemObjRelease(pNew);` |
|     5423 | 13257 | `			ph7_result_bool(pCtx,1);` |
|     2712 | 13258 | `		}else{` |
|     5436 | 13259 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13260 | `			/* Install the new handler */` |
|     5436 | 13261 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13262 | `		}` |
|     5428 | 13263 | `	}` |
|    10858 | 13264 | `	return PH7_OK;` |
|        2 | 13265 |  |
|        - | 13266 | `/*` |
|        - | 13267 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13268 | ` *  Generates a backtrace.` |
|        - | 13269 | ` * Paramaeter` |
|        - | 13270 | ` *  $options` |
|        - | 13271 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13272 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13273 | ` *   all the function/method arguments, to save memory.` |
|        - | 13274 | ` * $limit` |
|        - | 13275 | ` *   (Not Used)` |
|        - | 13276 | ` * Return` |
|        - | 13277 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13278 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13279 | ` *          Name        Type      Description` |
|        - | 13280 | ` *          ------      ------     -----------` |
|        - | 13281 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13282 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13283 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13284 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13285 | ` *          object      object    The current object.` |
|        - | 13286 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13287 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13288 | ` */` |
|      926 | 13289 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13290 |  |
|      928 | 13291 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13292 | `	ph7_value *pArray;` |
|        - | 13293 | `	ph7_class *pClass;` |
|        - | 13294 | `	ph7_value *pValue;` |
|        - | 13295 | `	SyString *pFile;` |
|        - | 13296 | `	/* Create a new array */` |
|      928 | 13297 | `	pArray = ph7_context_new_array(pCtx);` |
|      928 | 13298 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      928 | 13299 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13300 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13301 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13302 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13303 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13304 | `		SXUNUSED(apArg);` |
|      ! 0 | 13305 | `		return PH7_OK;` |
|        - | 13306 | `	}` |
|        - | 13307 | `	/* Dump running function name and it's arguments  */` |
|      928 | 13308 | `	if( pVm->pFrame->pParent ){` |
|      928 | 13309 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13310 | `		ph7_vm_func *pFunc;` |
|        - | 13311 | `		ph7_value *pArg;` |
|      928 | 13312 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      928 | 13313 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      928 | 13314 | `		if( pFrame->pParent && pFunc ){` |
|      928 | 13315 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      928 | 13316 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      928 | 13317 | `			ph7_value_reset_string_cursor(pValue);` |
|      463 | 13318 | `		}` |
|        - | 13319 | `		/* Function arguments */` |
|      928 | 13320 | `		pArg = ph7_context_new_array(pCtx);` |
|      928 | 13321 | `		if( pArg  ){` |
|        - | 13322 | `			ph7_value *pObj;` |
|        - | 13323 | `			VmSlot *aSlot;` |
|        - | 13324 | `			sxu32 n;` |
|        - | 13325 | `			/* Start filling the array with the given arguments */` |
|      928 | 13326 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3710 | 13327 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2784 | 13328 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2784 | 13329 | `				if( pObj ){` |
|     2784 | 13330 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1391 | 13331 | `				}` |
|     1393 | 13332 | `			}` |
|        - | 13333 | `			/* Save the array */` |
|      928 | 13334 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      463 | 13335 | `		}` |
|      463 | 13336 | `	}` |
|      928 | 13337 | `	ph7_value_int(pValue,1);` |
|        - | 13338 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13339 | `	 * line numbers at run-time. )` |
|        - | 13340 | `	 */` |
|      928 | 13341 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13342 | `	/* Current processed script */` |
|      928 | 13343 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      928 | 13344 | `	if( pFile ){` |
|      928 | 13345 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      928 | 13346 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      928 | 13347 | `		ph7_value_reset_string_cursor(pValue);` |
|      463 | 13348 | `	}` |
|        - | 13349 | `	/* Top class */` |
|      928 | 13350 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      928 | 13351 | `	if( pClass ){` |
|      924 | 13352 | `		ph7_value_reset_string_cursor(pValue);` |
|      924 | 13353 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      924 | 13354 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      461 | 13355 | `	}` |
|        - | 13356 | `	/* Return the freshly created array */` |
|      928 | 13357 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13358 | `	/*` |
|        - | 13359 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13360 | `	 * as soon we return from this function.` |
|        - | 13361 | `	 */` |
|      928 | 13362 | `	return PH7_OK;` |
|      465 | 13363 |  |
|        - | 13364 | `/*` |
|        - | 13365 | ` * Generate a small backtrace.` |
|        - | 13366 | ` * Store the generated dump in the given BLOB` |
|        - | 13367 | ` */` |
|        4 | 13368 | `static int VmMiniBacktrace(` |
|        - | 13369 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13370 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13371 | `	)` |
|        1 | 13372 |  |
|        5 | 13373 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13374 | `	ph7_vm_func *pFunc;` |
|        - | 13375 | `	ph7_class *pClass;` |
|        - | 13376 | `	SyString *pFile;` |
|        - | 13377 | `	/* Called function */` |
|        5 | 13378 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13379 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13380 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13381 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13382 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13383 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13384 | `	}else{` |
|      ! 0 | 13385 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13386 | `	}` |
|        5 | 13387 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13388 | `	/* Current processed script */` |
|        5 | 13389 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13390 | `	if( pFile ){` |
|        5 | 13391 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13392 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13393 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13394 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13395 | `	}` |
|        - | 13396 | `	/* Top class */` |
|        5 | 13397 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13398 | `	if( pClass ){` |
|      ! 0 | 13399 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13400 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13401 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13402 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13403 | `	}` |
|        5 | 13404 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13405 | `	/* All done */` |
|        5 | 13406 | `	return SXRET_OK;` |
|        1 | 13407 |  |
|        - | 13408 | `/*` |
|        - | 13409 | ` * void debug_print_backtrace()` |
|        - | 13410 | ` *  Prints a backtrace` |
|        - | 13411 | ` * Parameters` |
|        - | 13412 | ` * None` |
|        - | 13413 | ` * Return` |
|        - | 13414 | ` * NULL` |
|        - | 13415 | ` */` |
|        2 | 13416 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13417 |  |
|        3 | 13418 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13419 | `	SyBlob sDump;` |
|        3 | 13420 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13421 | `	/* Generate the backtrace */` |
|        3 | 13422 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13423 | `	/* Output backtrace */` |
|        3 | 13424 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13425 | `	/* All done,cleanup */` |
|        3 | 13426 | `	SyBlobRelease(&sDump);` |
|        1 | 13427 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13428 | `	SXUNUSED(apArg);` |
|        3 | 13429 | `	return PH7_OK;` |
|        1 | 13430 |  |
|        - | 13431 | `/*` |
|        - | 13432 | ` * string debug_string_backtrace()` |
|        - | 13433 | ` *  Generate a backtrace` |
|        - | 13434 | ` * Parameters` |
|        - | 13435 | ` * None` |
|        - | 13436 | ` * Return` |
|        - | 13437 | ` *  A mini backtrace().` |
|        - | 13438 | ` * Note that this is a symisc extension.` |
|        - | 13439 | ` */` |
|        2 | 13440 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13441 |  |
|        3 | 13442 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13443 | `	SyBlob sDump;` |
|        3 | 13444 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13445 | `	/* Generate the backtrace */` |
|        3 | 13446 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13447 | `	/* Return the backtrace */` |
|        3 | 13448 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13449 | `	/* All done,cleanup */` |
|        3 | 13450 | `	SyBlobRelease(&sDump);` |
|        1 | 13451 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13452 | `	SXUNUSED(apArg);` |
|        3 | 13453 | `	return PH7_OK;` |
|        1 | 13454 |  |
|        - | 13455 | `/*` |
|        - | 13456 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13457 | ` * exception is triggered.` |
|        - | 13458 | ` */` |
|      512 | 13459 | `static sxi32 VmUncaughtException(` |
|        - | 13460 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13461 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13462 | `	)` |
|        1 | 13463 |  |
|        - | 13464 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13465 | `	int nArg = 1;` |
|        - | 13466 | `	sxi32 rc;` |
|      513 | 13467 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13468 | `		/* Nesting limit reached */` |
|      ! 0 | 13469 | `		return SXRET_OK;` |
|        - | 13470 | `	}` |
|        - | 13471 | `	/* Call any exception handler if available */` |
|      513 | 13472 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13473 | `	if( pThis ){` |
|        - | 13474 | `		/* Load the exception instance */` |
|      513 | 13475 | `		sArg.x.pOther = pThis;` |
|      513 | 13476 | `		pThis->iRef++;` |
|      513 | 13477 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13478 | `	}else{` |
|      ! 0 | 13479 | `		nArg = 0;` |
|        - | 13480 | `	}` |
|      513 | 13481 | `	apArg[0] = &sArg;` |
|        - | 13482 | `	/* Call the exception handler if available */` |
|      513 | 13483 | `	pVm->nExceptDepth++;` |
|      513 | 13484 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13485 | `	pVm->nExceptDepth--;` |
|      513 | 13486 | `	if( rc != SXRET_OK ){` |
|        - | 13487 | `		SyBlob sMsgBuf;` |
|      511 | 13488 | `		const char *zClass = "Exception";` |
|      511 | 13489 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13490 | `		const char *zMsg;` |
|        - | 13491 | `		sxu32 nMsg;` |
|        - | 13492 | `		const char *zFuncName;` |
|        - | 13493 | `		int nFuncLen;` |
|      511 | 13494 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13495 | `		if( pThis ){` |
|        - | 13496 | `			ph7_class_method *pGetMessage;` |
|        - | 13497 | `			ph7_value sMsg;` |
|        - | 13498 | `			const char *zTmp;` |
|        - | 13499 | `			int nTmp;` |
|      511 | 13500 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13501 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13502 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13503 | `			if( pGetMessage ){` |
|      511 | 13504 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13505 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13506 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13507 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13508 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13509 | `					}` |
|      255 | 13510 | `				}` |
|      511 | 13511 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13512 | `			}` |
|      255 | 13513 | `		}` |
|      511 | 13514 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13515 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13516 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13517 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13518 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13519 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13520 | `		rc = SXERR_ABORT;` |
|      255 | 13521 | `	}` |
|      513 | 13522 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13523 | `	return rc;` |
|      257 | 13524 |  |
|        - | 13525 | `/*` |
|        - | 13526 | ` * Throw a user exception.` |
|        - | 13527 | ` *` |
|        - | 13528 | ` * Exception dispatch follows this sequence:` |
|        - | 13529 | ` *` |
|        - | 13530 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13531 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13532 | ` *` |
|        - | 13533 | ` * 2. If NO catch matches:` |
|        - | 13534 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13535 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13536 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13537 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13538 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13539 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13540 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13541 | ` *` |
|        - | 13542 | ` * 3. If a catch DOES match:` |
|        - | 13543 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13544 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13545 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13546 | ` *       finally block.` |
|        - | 13547 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13548 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13549 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13550 | ` *       in pPendingException (step 2c).` |
|        - | 13551 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13552 | ` *    d. Run finally (if present).` |
|        - | 13553 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13554 | ` *       that handlers are restored and finally has run.` |
|        - | 13555 | ` */` |
|      856 | 13556 | `static sxi32 VmThrowException(` |
|        - | 13557 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13558 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13559 | `	)` |
|        2 | 13560 |  |
|        - | 13561 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13562 | `	ph7_exception **apException;` |
|        - | 13563 | `	ph7_exception *pException;` |
|        - | 13564 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13565 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13566 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      858 | 13567 | `	VmCoalesceDisarm(pVm);` |
|        - | 13568 | `	/* Point to the stack of loaded exceptions */` |
|      858 | 13569 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      858 | 13570 | `	pException = 0;` |
|      858 | 13571 | `	pCatch = 0;` |
|      858 | 13572 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13573 | `		ph7_exception_block *aCatch;` |
|        - | 13574 | `		ph7_class *pClass;` |
|        - | 13575 | `		SyString *aNames;` |
|        - | 13576 | `		sxu32 nNames;` |
|        - | 13577 | `		int matched;` |
|        - | 13578 | `		sxu32 j,k;` |
|        - | 13579 | `		/* Locate the appropriate block to execute */` |
|      338 | 13580 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      338 | 13581 | `		(void)SySetPop(&pVm->aException);` |
|      338 | 13582 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      346 | 13583 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13584 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      344 | 13585 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      344 | 13586 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      344 | 13587 | `			matched = 0;` |
|      370 | 13588 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13589 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13590 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13591 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      362 | 13592 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      362 | 13593 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13594 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13595 | `					continue;` |
|        - | 13596 | `				}` |
|      362 | 13597 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      336 | 13598 | `					matched = 1;` |
|      336 | 13599 | `					break;` |
|        - | 13600 | `				}` |
|       14 | 13601 | `			}` |
|      344 | 13602 | `			if( matched ){` |
|        - | 13603 | `				/* Catch block found,break immediately */` |
|      336 | 13604 | `				pCatch = &aCatch[j];` |
|      336 | 13605 | `				break;` |
|        - | 13606 | `			}` |
|        5 | 13607 | `		}` |
|      168 | 13608 | `	}` |
|        - | 13609 | `	/* Execute the cached block if available */` |
|      858 | 13610 | `	if( pCatch == 0 ){` |
|        - | 13611 | `		sxi32 rc;` |
|        - | 13612 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13613 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13614 | `			pException->iFinallyDone = 1;` |
|        3 | 13615 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13616 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13617 | `				return SXERR_ABORT;` |
|        - | 13618 | `			}` |
|        1 | 13619 | `		}` |
|        - | 13620 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13621 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13622 | `			/* Re-throw to the outer handler */` |
|        3 | 13623 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13624 | `		}` |
|        - | 13625 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13626 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13627 | `		 * exception instead of reporting it uncaught.` |
|        - | 13628 | `		 */` |
|      522 | 13629 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13630 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13631 | `			 * by looking for a catch frame on the stack.` |
|        - | 13632 | `			 */` |
|      522 | 13633 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13634 | `			int inCatch = 0;` |
|     1050 | 13635 | `			while( pF ){` |
|      538 | 13636 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13637 | `					inCatch = 1;` |
|        9 | 13638 | `					break;` |
|        - | 13639 | `				}` |
|      529 | 13640 | `				pF = pF->pParent;` |
|        1 | 13641 | `			}` |
|      522 | 13642 | `			if( inCatch ){` |
|        - | 13643 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13644 | `				pThis->iRef++;` |
|        9 | 13645 | `				pVm->pPendingException = pThis;` |
|        9 | 13646 | `				return SXRET_OK;` |
|        - | 13647 | `			}` |
|      256 | 13648 | `		}` |
|        - | 13649 | `		/* Truly uncaught */` |
|      513 | 13650 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13651 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13652 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13653 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13654 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13655 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13656 | `			}` |
|      ! 0 | 13657 | `		}` |
|      513 | 13658 | `		return rc;` |
|      ! 0 | 13659 | `	}else{` |
|      336 | 13660 | `		VmFrame *pFrame = pVm->pFrame;` |
|      336 | 13661 | `		ph7_exception **apSaved = 0;` |
|        - | 13662 | `		sxu32 nSavedCount;` |
|        - | 13663 | `		sxi32 rc;` |
|      336 | 13664 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      336 | 13665 | `		if( pException->pFrame == pFrame ){` |
|      236 | 13666 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      117 | 13667 | `		}` |
|        - | 13668 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13669 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13670 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13671 | `		 */` |
|      336 | 13672 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      336 | 13673 | `		if( nSavedCount > 0 ){` |
|       16 | 13674 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13675 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13676 | `			if( apSaved ){` |
|       16 | 13677 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13678 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13679 | `				SySetReset(&pVm->aException);` |
|        5 | 13680 | `			}` |
|        5 | 13681 | `		}` |
|        - | 13682 | `		/* Create a private frame first */` |
|      336 | 13683 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      336 | 13684 | `		if( rc == SXRET_OK ){` |
|      336 | 13685 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      336 | 13686 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      336 | 13687 | `			if( pObj ){` |
|      336 | 13688 | `				pThis->iRef++;` |
|      336 | 13689 | `				pObj->x.pOther = pThis;` |
|      336 | 13690 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      167 | 13691 | `			}` |
|        - | 13692 | `			/* Execute the catch block */` |
|      336 | 13693 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13694 | `			/* Leave the frame */` |
|      336 | 13695 | `			VmLeaveFrame(&(*pVm));` |
|      167 | 13696 | `		}` |
|        - | 13697 | `		/* Restore the outer exception handlers */` |
|      336 | 13698 | `		if( apSaved ){` |
|        - | 13699 | `			sxu32 k;` |
|        - | 13700 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13701 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13702 | `			 * Restore the original outer entries.` |
|        - | 13703 | `			 */` |
|       11 | 13704 | `			SySetReset(&pVm->aException);` |
|       21 | 13705 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13706 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13707 | `			}` |
|       11 | 13708 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13709 | `		}` |
|        - | 13710 | `		/* Execute the finally block after catch */` |
|      336 | 13711 | `		if( pException->iHasFinally ){` |
|       16 | 13712 | `			pException->iFinallyDone = 1;` |
|        - | 13713 | `			{` |
|       16 | 13714 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13715 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13716 | `					return SXERR_ABORT;` |
|        - | 13717 | `				}` |
|        - | 13718 | `			}` |
|        7 | 13719 | `		}` |
|      336 | 13720 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13721 | `			return SXERR_ABORT;` |
|        - | 13722 | `		}` |
|        - | 13723 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13724 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13725 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13726 | `		 */` |
|      336 | 13727 | `		if( pVm->pPendingException ){` |
|        9 | 13728 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13729 | `			pVm->pPendingException = 0;` |
|        9 | 13730 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13731 | `		}` |
|        - | 13732 | `	}` |
|        - | 13733 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13734 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13735 | `	 */` |
|      328 | 13736 | `	return SXRET_OK;` |
|      430 | 13737 |  |
|        - | 13738 | `/*` |
|        - | 13739 | ` * Section:` |
|        - | 13740 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13741 | ` * Status:` |
|        - | 13742 | ` *    Stable.` |
|        - | 13743 | ` */` |
|        - | 13744 | `/*` |
|        - | 13745 | ` * string ph7version(void)` |
|        - | 13746 | ` *  Returns the running version of the PH7 version.` |
|        - | 13747 | ` * Parameters` |
|        - | 13748 | ` *  None` |
|        - | 13749 | ` * Return` |
|        - | 13750 | ` * Current PH7 version.` |
|        - | 13751 | ` */` |
|        2 | 13752 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13753 |  |
|        1 | 13754 | `	SXUNUSED(nArg);` |
|        1 | 13755 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13756 | `	/* Current engine version */` |
|        3 | 13757 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13758 | `	return PH7_OK;` |
|        1 | 13759 |  |
|        - | 13760 | `/*` |
|        - | 13761 | ` * string phpversion([ string $extension ])` |
|        - | 13762 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 13763 | ` * Parameters` |
|        - | 13764 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 13765 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 13766 | ` * Return` |
|        - | 13767 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 13768 | ` */` |
|        4 | 13769 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13770 |  |
|        2 | 13771 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 13772 | `	if( nArg > 0 ){` |
|      ! 0 | 13773 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13774 | `		return PH7_OK;` |
|        - | 13775 | `	}` |
|        5 | 13776 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 13777 | `	return PH7_OK;` |
|        3 | 13778 |  |
|        - | 13779 | `/*` |
|        - | 13780 | ` * string php_sapi_name(void)` |
|        - | 13781 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 13782 | ` * Parameters` |
|        - | 13783 | ` *  None` |
|        - | 13784 | ` * Return` |
|        - | 13785 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 13786 | ` */` |
|        2 | 13787 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13788 |  |
|        3 | 13789 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 13790 | `	SXUNUSED(nArg);` |
|        1 | 13791 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 13792 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 13793 | `	return PH7_OK;` |
|        1 | 13794 |  |
|        - | 13795 | `/*` |
|        - | 13796 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13797 | ` */` |
|        - | 13798 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13799 | ` "<html><head>"\` |
|        - | 13800 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13801 | ` "<style type=\"text/css\">"\` |
|        - | 13802 | ` "div {"\` |
|        - | 13803 | `     "border: 1px solid #cccccc;"\` |
|        - | 13804 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13805 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13806 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13807 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13808 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13809 | `     "-o-border-radius: 10px;"\` |
|        - | 13810 | `     "border-radius: 10px;"\` |
|        - | 13811 | `     "padding-left: 2em;"\` |
|        - | 13812 | `     "background-color: white;"\` |
|        - | 13813 | `     "margin-left: auto;"\` |
|        - | 13814 | `     "font-family: verdana;"\` |
|        - | 13815 | `     "padding-right: 2em;"\` |
|        - | 13816 | `     "margin-right: auto;"\` |
|        - | 13817 | `     "}"\` |
|        - | 13818 | `     "body {"\` |
|        - | 13819 | `     "padding: 0.2em;"\` |
|        - | 13820 | `     "font-style: normal;"\` |
|        - | 13821 | `     "font-size: medium;"\` |
|        - | 13822 | `     "background-color: #f2f2f2;"\` |
|        - | 13823 | `     "}"\` |
|        - | 13824 | `     "hr {"\` |
|        - | 13825 | `     "border-style: solid none none;"\` |
|        - | 13826 | `     "border-width: 1px medium medium;"\` |
|        - | 13827 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13828 | `     "height: 1px;"\` |
|        - | 13829 | `     "}"\` |
|        - | 13830 | `     "a {"\` |
|        - | 13831 | `     "color: #3366cc;"\` |
|        - | 13832 | `     "text-decoration: none;"\` |
|        - | 13833 | `     "}"\` |
|        - | 13834 | `     "a:hover {"\` |
|        - | 13835 | `     "color: #999999;"\` |
|        - | 13836 | `     "}"\` |
|        - | 13837 | `     "a:active {"\` |
|        - | 13838 | `     "color: #663399;"\` |
|        - | 13839 | `     "}"\` |
|        - | 13840 | `     "h1 {"\` |
|        - | 13841 | `     "margin: 0;"\` |
|        - | 13842 | `     "padding: 0;"\` |
|        - | 13843 | `     "font-family: Verdana;"\` |
|        - | 13844 | `     "font-weight: bold;"\` |
|        - | 13845 | `     "font-style: normal;"\` |
|        - | 13846 | `     "font-size: medium;"\` |
|        - | 13847 | `     "text-transform: capitalize;"\` |
|        - | 13848 | `     "color: #0a328c;"\` |
|        - | 13849 | `     "}"\` |
|        - | 13850 | `     "p {"\` |
|        - | 13851 | `     "margin: 0 auto;"\` |
|        - | 13852 | `     "font-size: medium;"\` |
|        - | 13853 | `     "font-style: normal;"\` |
|        - | 13854 | `     "font-family: verdana;"\` |
|        - | 13855 | `     "}"\` |
|        - | 13856 | `"</style></head><body>"\` |
|        - | 13857 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13858 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13859 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13860 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13861 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13862 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13863 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13864 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13865 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13866 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13867 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13868 |  |
|        - | 13869 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13870 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13871 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13872 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13873 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13874 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13875 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13876 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13877 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13878 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13879 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13880 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13881 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13882 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13883 |  |
|        - | 13884 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13885 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13886 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13887 | `"&nbsp;*<br>"\` |
|        - | 13888 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13889 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13890 | `"&nbsp;* are met:<br>"\` |
|        - | 13891 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13892 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13893 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13894 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13895 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13896 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13897 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13898 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13899 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13900 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13901 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13902 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13903 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13904 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13905 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13906 | `"&nbsp;*<br>"\` |
|        - | 13907 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13908 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13909 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13910 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13911 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13912 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13913 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13914 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13915 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13916 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13917 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13918 | `"&nbsp;*/<br>"\` |
|        - | 13919 | `"</span></small></small></p>"\` |
|        - | 13920 | `"</div></body></html>"` |
|        - | 13921 | `/*` |
|        - | 13922 | ` * bool ph7credits(void)` |
|        - | 13923 | ` * bool ph7info(void)` |
|        - | 13924 | ` * bool ph7copyright(void)` |
|        - | 13925 | ` *  Prints out the credits for PH7 engine` |
|        - | 13926 | ` * Parameters` |
|        - | 13927 | ` *  None` |
|        - | 13928 | ` * Return` |
|        - | 13929 | ` *  Always TRUE` |
|        - | 13930 | ` */` |
|        2 | 13931 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13932 |  |
|        3 | 13933 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13934 | `	/* Expand the HTML page above*/` |
|        3 | 13935 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13936 | `	ph7_context_output_format(` |
|        1 | 13937 | `		pCtx,` |
|        - | 13938 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13939 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13940 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13941 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13942 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13943 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13944 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13945 | `#ifdef __WINNT__` |
|        - | 13946 | `		"Windows NT"` |
|        - | 13947 | `#elif defined(__UNIXES__)` |
|        - | 13948 | `		"UNIX-Like"` |
|        - | 13949 | `#else` |
|        - | 13950 | `		"Other OS"` |
|        - | 13951 | `#endif` |
|        - | 13952 | `		);` |
|        3 | 13953 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13954 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13955 | `	SXUNUSED(apArg);` |
|        - | 13956 | `	/* Return TRUE */` |
|        - | 13957 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13958 | `	return PH7_OK;` |
|        1 | 13959 |  |
|        - | 13960 | `/*` |
|        - | 13961 | ` * Section:` |
|        - | 13962 | ` *    URL related routines.` |
|        - | 13963 | ` * Status:` |
|        - | 13964 | ` *    Stable.` |
|        - | 13965 | ` */` |
|        - | 13966 | `/*` |
|        - | 13967 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13968 | ` *  Parse a URL and return its fields.` |
|        - | 13969 | ` * Parameters` |
|        - | 13970 | ` *  $url` |
|        - | 13971 | ` *   The URL to parse.` |
|        - | 13972 | ` * $component` |
|        - | 13973 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13974 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13975 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13976 | ` *  in which case the return value will be an integer).` |
|        - | 13977 | ` * Return` |
|        - | 13978 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13979 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13980 | ` *  this array are:` |
|        - | 13981 | ` *   scheme - e.g. http` |
|        - | 13982 | ` *   host` |
|        - | 13983 | ` *   port` |
|        - | 13984 | ` *   user` |
|        - | 13985 | ` *   pass` |
|        - | 13986 | ` *   path` |
|        - | 13987 | ` *   query - after the question mark ?` |
|        - | 13988 | ` *   fragment - after the hashmark #` |
|        - | 13989 | ` * Note:` |
|        - | 13990 | ` *  FALSE is returned on failure.` |
|        - | 13991 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13992 | ` *  with the standard PHP engine.` |
|        - | 13993 | ` */` |
|       28 | 13994 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13995 |  |
|        - | 13996 | `	const char *zStr; /* Input string */` |
|        - | 13997 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13998 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13999 | `	int nLen;` |
|        - | 14000 | `	sxi32 rc;` |
|       29 | 14001 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14002 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14003 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14004 | `		return PH7_OK;` |
|        - | 14005 | `	}` |
|        - | 14006 | `	/* Extract the given URI */` |
|       29 | 14007 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14008 | `	if( nLen < 1 ){` |
|        - | 14009 | `		/* Nothing to process,return FALSE */` |
|        3 | 14010 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14011 | `		return PH7_OK;` |
|        - | 14012 | `	}` |
|        - | 14013 | `	/* Get a parse */` |
|       27 | 14014 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14015 | `	if( rc != SXRET_OK ){` |
|        - | 14016 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14017 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14018 | `		return PH7_OK;` |
|        - | 14019 | `	}` |
|       27 | 14020 | `	if( nArg > 1 ){` |
|      ! 0 | 14021 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14022 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14023 | `		switch(nComponent){` |
|      ! 0 | 14024 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14025 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14026 | `			if( pComp->nByte < 1 ){` |
|        - | 14027 | `				/* No available value,return NULL */` |
|      ! 0 | 14028 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14029 | `			}else{` |
|      ! 0 | 14030 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14031 | `			}` |
|      ! 0 | 14032 | `			break;` |
|      ! 0 | 14033 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14034 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14035 | `			if( pComp->nByte < 1 ){` |
|        - | 14036 | `				/* No available value,return NULL */` |
|      ! 0 | 14037 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14038 | `			}else{` |
|      ! 0 | 14039 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14040 | `			}` |
|      ! 0 | 14041 | `			break;` |
|      ! 0 | 14042 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14043 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14044 | `			if( pComp->nByte < 1 ){` |
|        - | 14045 | `				/* No available value,return NULL */` |
|      ! 0 | 14046 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14047 | `			}else{` |
|      ! 0 | 14048 | `				int iPort = 0;` |
|        - | 14049 | `				/* Cast the value to integer */` |
|      ! 0 | 14050 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14051 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14052 | `			}` |
|      ! 0 | 14053 | `			break;` |
|      ! 0 | 14054 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14055 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14056 | `			if( pComp->nByte < 1 ){` |
|        - | 14057 | `				/* No available value,return NULL */` |
|      ! 0 | 14058 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14059 | `			}else{` |
|      ! 0 | 14060 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14061 | `			}` |
|      ! 0 | 14062 | `			break;` |
|      ! 0 | 14063 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14064 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14065 | `			if( pComp->nByte < 1 ){` |
|        - | 14066 | `				/* No available value,return NULL */` |
|      ! 0 | 14067 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14068 | `			}else{` |
|      ! 0 | 14069 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14070 | `			}` |
|      ! 0 | 14071 | `			break;` |
|      ! 0 | 14072 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14073 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14074 | `			if( pComp->nByte < 1 ){` |
|        - | 14075 | `				/* No available value,return NULL */` |
|      ! 0 | 14076 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14077 | `			}else{` |
|      ! 0 | 14078 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14079 | `			}` |
|      ! 0 | 14080 | `			break;` |
|      ! 0 | 14081 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14082 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14083 | `			if( pComp->nByte < 1 ){` |
|        - | 14084 | `				/* No available value,return NULL */` |
|      ! 0 | 14085 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14086 | `			}else{` |
|      ! 0 | 14087 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14088 | `			}` |
|      ! 0 | 14089 | `			break;` |
|      ! 0 | 14090 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14091 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14092 | `			if( pComp->nByte < 1 ){` |
|        - | 14093 | `				/* No available value,return NULL */` |
|      ! 0 | 14094 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14095 | `			}else{` |
|      ! 0 | 14096 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14097 | `			}` |
|      ! 0 | 14098 | `			break;` |
|      ! 0 | 14099 | `		default:` |
|        - | 14100 | `			/* No such entry,return NULL */` |
|      ! 0 | 14101 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14102 | `			break;` |
|        - | 14103 | `		}` |
|      ! 0 | 14104 | `	}else{` |
|        - | 14105 | `		ph7_value *pArray,*pValue;` |
|        - | 14106 | `		/* Return an associative array */` |
|       27 | 14107 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14108 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14109 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14110 | `			/* Out of memory */` |
|      ! 0 | 14111 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14112 | `			/* Return false */` |
|      ! 0 | 14113 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14114 | `			return PH7_OK;` |
|        - | 14115 | `		}` |
|        - | 14116 | `		/* Fill the array */` |
|       27 | 14117 | `		pComp = &sURI.sScheme;` |
|       27 | 14118 | `		if( pComp->nByte > 0 ){` |
|       19 | 14119 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14120 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14121 | `		}` |
|        - | 14122 | `		/* Reset the string cursor */` |
|       27 | 14123 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14124 | `		pComp = &sURI.sHost;` |
|       27 | 14125 | `		if( pComp->nByte > 0 ){` |
|       25 | 14126 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14127 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14128 | `		}` |
|        - | 14129 | `		/* Reset the string cursor */` |
|       27 | 14130 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14131 | `		pComp = &sURI.sPort;` |
|       27 | 14132 | `		if( pComp->nByte > 0 ){` |
|       11 | 14133 | `			int iPort = 0;/* cc warning */` |
|        - | 14134 | `			/* Convert to integer */` |
|       11 | 14135 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14136 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14137 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14138 | `		}` |
|        - | 14139 | `		/* Reset the string cursor */` |
|       27 | 14140 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14141 | `		pComp = &sURI.sUser;` |
|       27 | 14142 | `		if( pComp->nByte > 0 ){` |
|        7 | 14143 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14144 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14145 | `		}` |
|        - | 14146 | `		/* Reset the string cursor */` |
|       27 | 14147 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14148 | `		pComp = &sURI.sPass;` |
|       27 | 14149 | `		if( pComp->nByte > 0 ){` |
|        7 | 14150 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14151 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14152 | `		}` |
|        - | 14153 | `		/* Reset the string cursor */` |
|       27 | 14154 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14155 | `		pComp = &sURI.sPath;` |
|       27 | 14156 | `		if( pComp->nByte > 0 ){` |
|       17 | 14157 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14158 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14159 | `		}` |
|        - | 14160 | `		/* Reset the string cursor */` |
|       27 | 14161 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14162 | `		pComp = &sURI.sQuery;` |
|       27 | 14163 | `		if( pComp->nByte > 0 ){` |
|        5 | 14164 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14165 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14166 | `		}` |
|        - | 14167 | `		/* Reset the string cursor */` |
|       27 | 14168 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14169 | `		pComp = &sURI.sFragment;` |
|       27 | 14170 | `		if( pComp->nByte > 0 ){` |
|        5 | 14171 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14172 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14173 | `		}` |
|        - | 14174 | `		/* Return the created array */` |
|       27 | 14175 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14176 | `		/* NOTE:` |
|        - | 14177 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14178 | `		 * automatically as soon we return from this function.` |
|        - | 14179 | `		 */` |
|        - | 14180 | `	}` |
|        - | 14181 | `	/* All done */` |
|       27 | 14182 | `	return PH7_OK;` |
|       15 | 14183 |  |
|        - | 14184 | `/*` |
|        - | 14185 | ` * Section:` |
|        - | 14186 | ` *   Array related routines.` |
|        - | 14187 | ` * Status:` |
|        - | 14188 | ` *    Stable.` |
|        - | 14189 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14190 | ` *  Array related functions that need access to the underlying` |
|        - | 14191 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14192 | ` */` |
|        - | 14193 | `/*` |
|        - | 14194 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14195 | ` * of the following structure.` |
|        - | 14196 | ` */` |
|        - | 14197 | `struct compact_data` |
|        - | 14198 |  |
|        - | 14199 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14200 | `	int nRecCount;      /* Recursion count */` |
|        - | 14201 | `};` |
|        - | 14202 | `/*` |
|        - | 14203 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14204 | ` */` |
|      ! 0 | 14205 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14206 |  |
|      ! 0 | 14207 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14208 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14209 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14210 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14211 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14212 | `		SyString sVar;` |
|      ! 0 | 14213 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14214 | `		if( sVar.nByte > 0 ){` |
|        - | 14215 | `			/* Query the current frame */` |
|      ! 0 | 14216 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14217 | `			/* ^` |
|        - | 14218 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14219 | `			 */` |
|      ! 0 | 14220 | `			if( pKey ){` |
|        - | 14221 | `				/* Perform the insertion */` |
|      ! 0 | 14222 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14223 | `			}` |
|      ! 0 | 14224 | `		}` |
|      ! 0 | 14225 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14226 | `		int rc;` |
|        - | 14227 | `		/* Recursively traverse this array */` |
|      ! 0 | 14228 | `		pData->nRecCount++;` |
|      ! 0 | 14229 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14230 | `		pData->nRecCount--;` |
|      ! 0 | 14231 | `		return rc;` |
|        - | 14232 | `	}` |
|      ! 0 | 14233 | `	return SXRET_OK;` |
|      ! 0 | 14234 |  |
|        - | 14235 | `/*` |
|        - | 14236 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14237 | ` *  Create array containing variables and their values.` |
|        - | 14238 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14239 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14240 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14241 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14242 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14243 | ` * Parameters` |
|        - | 14244 | ` *  $varname` |
|        - | 14245 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14246 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14247 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14248 | ` *   it recursively.` |
|        - | 14249 | ` * Return` |
|        - | 14250 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14251 | ` */` |
|        2 | 14252 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14253 |  |
|        - | 14254 | `	ph7_value *pArray,*pObj;` |
|        3 | 14255 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14256 | `	const char *zName;` |
|        - | 14257 | `	SyString sVar;` |
|        - | 14258 | `	int i,nLen;` |
|        3 | 14259 | `	if( nArg < 1 ){` |
|        - | 14260 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14261 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14262 | `		return PH7_OK;` |
|        - | 14263 | `	}` |
|        - | 14264 | `	/* Create the array */` |
|        3 | 14265 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14266 | `	if( pArray == 0 ){` |
|        - | 14267 | `		/* Out of memory */` |
|      ! 0 | 14268 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14269 | `		/* Return NULL */` |
|      ! 0 | 14270 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14271 | `		return PH7_OK;` |
|        - | 14272 | `	}` |
|        - | 14273 | `	/* Perform the requested operation */` |
|        7 | 14274 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14275 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14276 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14277 | `				struct compact_data sData;` |
|      ! 0 | 14278 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14279 | `				/* Recursively walk the array */` |
|      ! 0 | 14280 | `				sData.nRecCount = 0;` |
|      ! 0 | 14281 | `				sData.pArray = pArray;` |
|      ! 0 | 14282 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14283 | `			}` |
|      ! 0 | 14284 | `		}else{` |
|        - | 14285 | `			/* Extract variable name */` |
|        5 | 14286 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14287 | `			if( nLen > 0 ){` |
|        5 | 14288 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14289 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14290 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14291 | `				if( pObj ){` |
|        5 | 14292 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14293 | `				}` |
|        2 | 14294 | `			}` |
|        - | 14295 | `		}` |
|        3 | 14296 | `	}` |
|        - | 14297 | `	/* Return the array */` |
|        3 | 14298 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14299 | `	return PH7_OK;` |
|        2 | 14300 |  |
|        - | 14301 | `/*` |
|        - | 14302 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14303 | ` * of the following structure.` |
|        - | 14304 | ` */` |
|        - | 14305 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14306 | `struct extract_aux_data` |
|        - | 14307 |  |
|        - | 14308 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14309 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14310 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14311 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14312 | `	int iFlags;           /* Control flags */` |
|        - | 14313 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14314 | `};` |
|        - | 14315 | `/* Forward declaration */` |
|        - | 14316 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14317 | `/*` |
|        - | 14318 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14319 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14320 | ` * Parameters` |
|        - | 14321 | ` * $var_array` |
|        - | 14322 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14323 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14324 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14325 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14326 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14327 | ` * $extract_type` |
|        - | 14328 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14329 | ` *  It can be one of the following values:` |
|        - | 14330 | ` *   EXTR_OVERWRITE` |
|        - | 14331 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14332 | ` *   EXTR_SKIP` |
|        - | 14333 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14334 | ` *   EXTR_PREFIX_SAME` |
|        - | 14335 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14336 | ` *   EXTR_PREFIX_ALL` |
|        - | 14337 | ` *       Prefix all variable names with prefix.` |
|        - | 14338 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14339 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14340 | ` *   EXTR_IF_EXISTS` |
|        - | 14341 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14342 | ` *       otherwise do nothing.` |
|        - | 14343 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14344 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14345 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14346 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14347 | ` *      the current symbol table.` |
|        - | 14348 | ` * $prefix` |
|        - | 14349 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14350 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14351 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14352 | ` *  underscore character.` |
|        - | 14353 | ` * Return` |
|        - | 14354 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14355 | ` */` |
|        4 | 14356 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14357 |  |
|        - | 14358 | `	extract_aux_data sAux;` |
|        - | 14359 | `	ph7_hashmap *pMap;` |
|        5 | 14360 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14361 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14362 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14363 | `		return PH7_OK;` |
|        - | 14364 | `	}` |
|        - | 14365 | `	/* Point to the target hashmap */` |
|        5 | 14366 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14367 | `	if( pMap->nEntry < 1 ){` |
|        - | 14368 | `		/* Empty map,return  0 */` |
|      ! 0 | 14369 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14370 | `		return PH7_OK;` |
|        - | 14371 | `	}` |
|        - | 14372 | `	/* Prepare the aux data */` |
|        5 | 14373 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14374 | `	if( nArg > 1 ){` |
|        3 | 14375 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14376 | `		if( nArg > 2 ){` |
|      ! 0 | 14377 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14378 | `		}` |
|        1 | 14379 | `	}` |
|        5 | 14380 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14381 | `	/* Invoke the worker callback */` |
|        5 | 14382 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14383 | `	/* Number of variables successfully imported */` |
|        5 | 14384 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14385 | `	return PH7_OK;` |
|        3 | 14386 |  |
|        - | 14387 | `/*` |
|        - | 14388 | ` * Worker callback for the [extract()] function defined` |
|        - | 14389 | ` * below.` |
|        - | 14390 | ` */` |
|        8 | 14391 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14392 |  |
|        9 | 14393 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14394 | `	int iFlags = pAux->iFlags;` |
|        9 | 14395 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14396 | `	ph7_value *pObj;` |
|        - | 14397 | `	SyString sVar;` |
|        9 | 14398 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14399 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14400 | `	}` |
|        - | 14401 | `	/* Perform a string cast */` |
|        9 | 14402 | `	PH7_MemObjToString(pKey);` |
|        9 | 14403 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14404 | `		/* Unavailable variable name */` |
|      ! 0 | 14405 | `		return SXRET_OK;` |
|        - | 14406 | `	}` |
|        9 | 14407 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14408 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14409 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14410 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14411 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14412 | `			);` |
|      ! 0 | 14413 | `	}else{` |
|       13 | 14414 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14415 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14416 | `	}` |
|        9 | 14417 | `	sVar.zString = pAux->zWorker;` |
|        - | 14418 | `	/* Try to extract the variable */` |
|        9 | 14419 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14420 | `	if( pObj ){` |
|        - | 14421 | `		/* Collision */` |
|        5 | 14422 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14423 | `			return SXRET_OK;` |
|        - | 14424 | `		}` |
|        5 | 14425 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14426 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14427 | `				/* Already prefixed */` |
|      ! 0 | 14428 | `				return SXRET_OK;` |
|        - | 14429 | `			}` |
|      ! 0 | 14430 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14431 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14432 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14433 | `				);` |
|      ! 0 | 14434 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14435 | `		}` |
|        3 | 14436 | `	}else{` |
|        - | 14437 | `		/* Create the variable */` |
|        5 | 14438 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14439 | `	}` |
|        9 | 14440 | `	if( pObj ){` |
|        - | 14441 | `		/* Overwrite the old value */` |
|        9 | 14442 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14443 | `		/* Increment counter */` |
|        9 | 14444 | `		pAux->iCount++;` |
|        4 | 14445 | `	}` |
|        9 | 14446 | `	return SXRET_OK;` |
|        5 | 14447 |  |
|        - | 14448 | `/*` |
|        - | 14449 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14450 | ` * defined below.` |
|        - | 14451 | ` */` |
|        2 | 14452 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14453 |  |
|        3 | 14454 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14455 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14456 | `	ph7_value *pObj;` |
|        - | 14457 | `	SyString sVar;` |
|        - | 14458 | `	/* Perform a string cast */` |
|        3 | 14459 | `	PH7_MemObjToString(pKey);` |
|        3 | 14460 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14461 | `		/* Unavailable variable name */` |
|      ! 0 | 14462 | `		return SXRET_OK;` |
|        - | 14463 | `	}` |
|        3 | 14464 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14465 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14466 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14467 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14468 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14469 | `			);` |
|        2 | 14470 | `	}else{` |
|      ! 0 | 14471 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14472 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14473 | `	}` |
|        3 | 14474 | `	sVar.zString = pAux->zWorker;` |
|        - | 14475 | `	/* Extract the variable */` |
|        3 | 14476 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14477 | `	if( pObj ){` |
|        3 | 14478 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14479 | `	}` |
|        3 | 14480 | `	return SXRET_OK;` |
|        2 | 14481 |  |
|        - | 14482 | `/*` |
|        - | 14483 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14484 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14485 | ` * Parameters` |
|        - | 14486 | ` * $types` |
|        - | 14487 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14488 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14489 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14490 | ` *  POST includes the POST uploaded file information.` |
|        - | 14491 | ` *  Note:` |
|        - | 14492 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14493 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14494 | ` * $prefix` |
|        - | 14495 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14496 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14497 | ` *  variable named $pref_userid.` |
|        - | 14498 | ` * Return` |
|        - | 14499 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14500 | ` */` |
|        2 | 14501 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14502 |  |
|        - | 14503 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14504 | `	extract_aux_data sAux;` |
|        - | 14505 | `	int nLen,nPrefixLen;` |
|        - | 14506 | `	ph7_value *pSuper;` |
|        - | 14507 | `	ph7_vm *pVm;` |
|        - | 14508 | `	/* By default import only $_GET variables  */` |
|        3 | 14509 | `	zImport = "G";` |
|        3 | 14510 | `	nLen = (int)sizeof(char);` |
|        3 | 14511 | `	zPrefix = 0;` |
|        3 | 14512 | `	nPrefixLen = 0;` |
|        3 | 14513 | `	if( nArg > 0 ){` |
|        3 | 14514 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14515 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14516 | `		}` |
|        3 | 14517 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14518 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14519 | `		}` |
|        1 | 14520 | `	}` |
|        - | 14521 | `	/* Point to the underlying VM */` |
|        3 | 14522 | `	pVm = pCtx->pVm;` |
|        - | 14523 | `	/* Initialize the aux data */` |
|        3 | 14524 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14525 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14526 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14527 | `	sAux.pVm = pVm;` |
|        - | 14528 | `	/* Extract */` |
|        3 | 14529 | `	zEnd = &zImport[nLen];` |
|        5 | 14530 | `	while( zImport < zEnd ){` |
|        3 | 14531 | `		int c = zImport[0];` |
|        3 | 14532 | `		pSuper = 0;` |
|        3 | 14533 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14534 | `			/* Import $_GET variables */` |
|        3 | 14535 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14536 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14537 | `			/* Import $_POST variables */` |
|      ! 0 | 14538 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14539 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14540 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14541 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14542 | `		}` |
|        3 | 14543 | `		if( pSuper ){` |
|        - | 14544 | `			/* Iterate throw array entries */` |
|        3 | 14545 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14546 | `		}` |
|        - | 14547 | `		/* Advance the cursor */` |
|        3 | 14548 | `		zImport++;` |
|        1 | 14549 | `	}` |
|        - | 14550 | `	/* All done,return TRUE*/` |
|        3 | 14551 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14552 | `	return PH7_OK;` |
|        1 | 14553 |  |
|        - | 14554 | `/*` |
|        - | 14555 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14556 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14557 | ` * information.` |
|        - | 14558 | ` */` |
|    12718 | 14559 | `static sxi32 VmEvalChunk(` |
|        - | 14560 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14561 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14562 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14563 | `	int iFlags,         /* Compile flag */` |
|        - | 14564 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14565 | `	)` |
|        2 | 14566 |  |
|        - | 14567 | `	SySet *pByteCode,aByteCode;` |
|        - | 14568 | `	SyBlob sSavedNs;` |
|    12720 | 14569 | `	ProcConsumer xErr = 0;` |
|    12720 | 14570 | `	void *pErrData = 0;` |
|        - | 14571 | `	/* Initialize bytecode container */` |
|    12720 | 14572 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12720 | 14573 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14574 | `	/* Reset the code generator */` |
|    12720 | 14575 | `	if( bTrueReturn ){` |
|        - | 14576 | `		/* Included file,log compile-time errors */` |
|     9564 | 14577 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9564 | 14578 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4781 | 14579 | `	}` |
|    12720 | 14580 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14581 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14582 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14583 | `	 * the caller's namespace is restored. */` |
|    12720 | 14584 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12720 | 14585 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12720 | 14586 | `	if( bTrueReturn ){` |
|        - | 14587 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9564 | 14588 | `		SyBlobReset(&pVm->sNamespace);` |
|     4781 | 14589 | `	}` |
|        - | 14590 | `	/* Swap bytecode container */` |
|    12720 | 14591 | `	pByteCode = pVm->pByteContainer;` |
|    12720 | 14592 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14593 | `	/* Compile the chunk */` |
|    12720 | 14594 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19079 | 14595 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14596 | `		/* Compilation error,return false */` |
|        3 | 14597 | `		if( pCtx ){` |
|        3 | 14598 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14599 | `		}` |
|        2 | 14600 | `	}else{` |
|        - | 14601 | `		/* Mount any newly defined classes */` |
|        - | 14602 | `		SyHashEntry *pEntry;` |
|        - | 14603 | `		ph7_class *pClass;` |
|        - | 14604 | `		ph7_value sResult; /* Return value */` |
|        - | 14605 | `		sxi32 rc;` |
|    12718 | 14606 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   962016 | 14607 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   942942 | 14608 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14609 | `			/* Only mount classes that haven't been mounted yet */` |
|   942942 | 14610 | `			if( !pClass->bMounted ){` |
|   245668 | 14611 | `				rc = VmMountUserClass(pVm,pClass);` |
|   245668 | 14612 | `				if( rc != SXRET_OK ){` |
|        - | 14613 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14614 | `					if( pCtx ){` |
|      ! 0 | 14615 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14616 | `					}` |
|      ! 0 | 14617 | `					goto Cleanup;` |
|        - | 14618 | `				}` |
|   122833 | 14619 | `			}` |
|        2 | 14620 | `		}` |
|    12718 | 14621 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14622 | `			/* Out of memory */` |
|      ! 0 | 14623 | `			if( pCtx ){` |
|      ! 0 | 14624 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14625 | `			}` |
|      ! 0 | 14626 | `			goto Cleanup;` |
|        - | 14627 | `		}` |
|    12718 | 14628 | `		if( bTrueReturn ){` |
|        - | 14629 | `			/* Assume a boolean true return value */` |
|     9564 | 14630 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4783 | 14631 | `		}else{` |
|        - | 14632 | `			/* Assume a null return value */` |
|     3156 | 14633 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14634 | `		}` |
|        - | 14635 | `		/* Execute the compiled chunk */` |
|    12718 | 14636 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12718 | 14637 | `		if( pCtx ){` |
|        - | 14638 | `			/* Set the execution result */` |
|     9584 | 14639 | `			ph7_result_value(pCtx,&sResult);` |
|     4791 | 14640 | `		}` |
|    12718 | 14641 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14642 | `	}` |
|     6359 | 14643 | `Cleanup:` |
|        - | 14644 | `	/* Cleanup the mess left behind */` |
|    12720 | 14645 | `	pVm->pByteContainer = pByteCode;` |
|    12720 | 14646 | `	SySetRelease(&aByteCode);` |
|        - | 14647 | `	/* Restore caller's namespace state */` |
|    12720 | 14648 | `	SyBlobReset(&pVm->sNamespace);` |
|    12720 | 14649 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12720 | 14650 | `	SyBlobRelease(&sSavedNs);` |
|    12720 | 14651 | `	return SXRET_OK;` |
|        2 | 14652 |  |
|        - | 14653 | `/*` |
|        - | 14654 | ` * value eval(string $code)` |
|        - | 14655 | ` *   Evaluate a string as PHP code.` |
|        - | 14656 | ` * Parameter` |
|        - | 14657 | ` *  code: PHP code to evaluate.` |
|        - | 14658 | ` * Return` |
|        - | 14659 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14660 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14661 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14662 | ` */` |
|       24 | 14663 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14664 |  |
|        - | 14665 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 14666 | `	if( nArg < 1 ){` |
|        - | 14667 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14668 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14669 | `		return SXRET_OK;` |
|        - | 14670 | `	}` |
|        - | 14671 | `	/* Chunk to evaluate */` |
|       26 | 14672 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 14673 | `	if( sChunk.nByte < 1 ){` |
|        - | 14674 | `		/* Empty string,return NULL */` |
|        3 | 14675 | `		ph7_result_null(pCtx);` |
|        3 | 14676 | `		return SXRET_OK;` |
|        - | 14677 | `	}` |
|        - | 14678 | `	/* Eval the chunk */` |
|       24 | 14679 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 14680 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14681 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 14682 | `		return PH7_ABORT;` |
|        - | 14683 | `	}` |
|       22 | 14684 | `	return SXRET_OK;` |
|       14 | 14685 |  |
|        - | 14686 | `/*` |
|        - | 14687 | ` * Check if a file path is already included.` |
|        - | 14688 | ` */` |
|    19120 | 14689 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14690 |  |
|        - | 14691 | `	SyString *aEntries;` |
|        - | 14692 | `	sxu32 n;` |
|    19122 | 14693 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14694 | `	/* Perform a linear search */` |
| 91260720 | 14695 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 91241606 | 14696 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14697 | `			/* Already included */` |
|        7 | 14698 | `			return TRUE;` |
|        - | 14699 | `		}` |
| 45620801 | 14700 | `	}` |
|    19116 | 14701 | `	return FALSE;` |
|     9562 | 14702 |  |
|        - | 14703 | `/*` |
|        - | 14704 | ` * Push a file path in the appropriate VM container.` |
|        - | 14705 | ` */` |
|    22246 | 14706 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14707 |  |
|        - | 14708 | `	SyString sPath;` |
|        - | 14709 | `	char *zDup;` |
|        - | 14710 | `#ifdef __WINNT__` |
|        - | 14711 | `	char *zCur;` |
|        - | 14712 | `#endif` |
|        - | 14713 | `	sxi32 rc;` |
|    22248 | 14714 | `	if( nLen < 0 ){` |
|     3128 | 14715 | `		nLen = SyStrlen(zPath);` |
|     1563 | 14716 | `	}` |
|        - | 14717 | `	/* Duplicate the file path first */` |
|    22248 | 14718 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22248 | 14719 | `	if( zDup == 0 ){` |
|      ! 0 | 14720 | `		return SXERR_MEM;` |
|        - | 14721 | `	}` |
|        - | 14722 | `#ifdef __WINNT__` |
|        - | 14723 | `	/* Normalize path on windows` |
|        - | 14724 | `	 * Example:` |
|        - | 14725 | `	 *    Path/To/File.php` |
|        - | 14726 | `	 * becomes` |
|        - | 14727 | `	 *   path\to\file.php` |
|        - | 14728 | `	 */` |
|        2 | 14729 | `	zCur = zDup;` |
|        2 | 14730 | `	while( zCur[0] != 0 ){` |
|        2 | 14731 | `		if( zCur[0] == '/' ){` |
|        2 | 14732 | `			zCur[0] = '\\';` |
|        2 | 14733 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14734 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14735 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14736 | `		}` |
|        2 | 14737 | `		zCur++;` |
|        2 | 14738 | `	}` |
|        - | 14739 | `#endif` |
|        - | 14740 | `	/* Install the file path */` |
|    22248 | 14741 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22248 | 14742 | `	if( !bMain ){` |
|    19122 | 14743 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14744 | `			/* Already included */` |
|        7 | 14745 | `			*pNew = 0;` |
|        4 | 14746 | `		}else{` |
|        - | 14747 | `			/* Insert in the corresponding container */` |
|    19116 | 14748 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19116 | 14749 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14750 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14751 | `				return rc;` |
|        - | 14752 | `			}` |
|    19116 | 14753 | `			*pNew = 1;` |
|        - | 14754 | `		}` |
|     9560 | 14755 | `	}` |
|    22248 | 14756 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22248 | 14757 | `	return SXRET_OK;` |
|    11125 | 14758 |  |
|        - | 14759 | `/*` |
|        - | 14760 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14761 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14762 | ` * indicates failure.` |
|        - | 14763 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14764 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14765 | ` * operations.` |
|        - | 14766 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14767 | ` * this function is a no-op.` |
|        - | 14768 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14769 | ` * constructs for more information.` |
|        - | 14770 | ` */` |
|     9572 | 14771 | `static sxi32 VmExecIncludedFile(` |
|        - | 14772 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14773 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14774 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14775 | `	 )` |
|        2 | 14776 |  |
|        - | 14777 | `	sxi32 rc;` |
|        - | 14778 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14779 | `	const ph7_io_stream *pStream;` |
|        - | 14780 | `	SyBlob sContents;` |
|        - | 14781 | `	void *pHandle;` |
|        - | 14782 | `	ph7_vm *pVm;` |
|        - | 14783 | `	int isNew;` |
|        - | 14784 | `	/* Initialize fields */` |
|     9574 | 14785 | `	pVm = pCtx->pVm;` |
|     9574 | 14786 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9574 | 14787 | `	isNew = 0;` |
|        - | 14788 | `	/* Extract the associated stream */` |
|     9574 | 14789 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14790 | `	/*` |
|        - | 14791 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14792 | `	 * in a read-only mode.` |
|        - | 14793 | `	 */` |
|     9574 | 14794 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9574 | 14795 | `	if( pHandle == 0 ){` |
|        8 | 14796 | `		return SXERR_IO;` |
|        - | 14797 | `	}` |
|     9568 | 14798 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9568 | 14799 | `	if( IncludeOnce && !isNew ){` |
|        - | 14800 | `		/* Already included */` |
|        5 | 14801 | `		rc = SXERR_EXISTS;` |
|        3 | 14802 | `	}else{` |
|        - | 14803 | `		/* Read the whole file contents */` |
|     9564 | 14804 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9564 | 14805 | `		if( rc == SXRET_OK ){` |
|        - | 14806 | `			SyString sScript;` |
|        - | 14807 | `			/* Compile and execute the script */` |
|     9564 | 14808 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9564 | 14809 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4781 | 14810 | `		}` |
|        - | 14811 | `	}` |
|        - | 14812 | `	/* Pop from the set of included file */` |
|     9568 | 14813 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14814 | `	/* Close the handle */` |
|     9568 | 14815 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14816 | `	/* Release the working buffer */` |
|     9568 | 14817 | `	SyBlobRelease(&sContents);` |
|        - | 14818 | `#else` |
|        - | 14819 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14820 | `	SXUNUSED(pPath);` |
|        - | 14821 | `	SXUNUSED(IncludeOnce);` |
|        - | 14822 | `	rc = SXERR_IO;` |
|        - | 14823 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9568 | 14824 | `	return rc;` |
|     4788 | 14825 |  |
|        - | 14826 | `/*` |
|        - | 14827 | ` * string get_include_path(void)` |
|        - | 14828 | ` *  Gets the current include_path configuration option.` |
|        - | 14829 | ` * Parameter` |
|        - | 14830 | ` *  None` |
|        - | 14831 | ` * Return` |
|        - | 14832 | ` *  Included paths as a string` |
|        - | 14833 | ` */` |
|        2 | 14834 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14835 |  |
|        3 | 14836 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14837 | `	SyString *aEntry;` |
|        - | 14838 | `	int dir_sep;` |
|        - | 14839 | `	sxu32 n;` |
|        - | 14840 | `#ifdef __WINNT__` |
|        1 | 14841 | `	dir_sep = ';';` |
|        - | 14842 | `#else` |
|        - | 14843 | `	/* Assume UNIX path separator */` |
|        2 | 14844 | `	dir_sep = ':';` |
|        - | 14845 | `#endif` |
|        1 | 14846 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14847 | `	SXUNUSED(apArg);` |
|        - | 14848 | `	/* Point to the list of import paths */` |
|        3 | 14849 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14850 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14851 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14852 | `		if( n > 0 ){` |
|        - | 14853 | `			/* Append dir seprator */` |
|      ! 0 | 14854 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14855 | `		}` |
|        - | 14856 | `		/* Append path */` |
|        3 | 14857 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14858 | `	}` |
|        3 | 14859 | `	return PH7_OK;` |
|        1 | 14860 |  |
|        - | 14861 | `/*` |
|        - | 14862 | ` * string get_get_included_files(void)` |
|        - | 14863 | ` *  Gets the current include_path configuration option.` |
|        - | 14864 | ` * Parameter` |
|        - | 14865 | ` *  None` |
|        - | 14866 | ` * Return` |
|        - | 14867 | ` *  Included paths as a string` |
|        - | 14868 | ` */` |
|        2 | 14869 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14870 |  |
|        3 | 14871 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14872 | `	ph7_value *pArray,*pWorker;` |
|        - | 14873 | `	SyString *pEntry;` |
|        - | 14874 | `	int c,d;` |
|        - | 14875 | `	/* Create an array and a working value */` |
|        3 | 14876 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14877 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14878 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14879 | `		/* Out of memory,return null */` |
|      ! 0 | 14880 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14881 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14882 | `		SXUNUSED(apArg);` |
|      ! 0 | 14883 | `		return PH7_OK;` |
|        - | 14884 | `	}` |
|        3 | 14885 | `	c = d = '/';` |
|        - | 14886 | `#ifdef __WINNT__` |
|        1 | 14887 | `	d = '\\';` |
|        - | 14888 | `#endif` |
|        - | 14889 | `	/* Iterate throw entries */` |
|        3 | 14890 | `	SySetResetCursor(pFiles);` |
|     3917 | 14891 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14892 | `		const char *zBase,*zEnd;` |
|        - | 14893 | `		int iLen;` |
|        - | 14894 | `		/* reset the string cursor */` |
|     3915 | 14895 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14896 | `		/* Extract base name */` |
|     3915 | 14897 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14898 | `		/* Ignore trailing '/' */` |
|     5872 | 14899 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14900 | `			zEnd--;` |
|      ! 0 | 14901 | `		}` |
|     3915 | 14902 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 14903 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 14904 | `			zEnd--;` |
|        1 | 14905 | `		}` |
|     3915 | 14906 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 14907 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14908 | `		/* Copy entry name */` |
|     3915 | 14909 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14910 | `		/* Perform the insertion */` |
|     3915 | 14911 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14912 | `	}` |
|        - | 14913 | `	/* All done,return the created array */` |
|        3 | 14914 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14915 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14916 | `	 * by the engine as soon we return from this foreign` |
|        - | 14917 | `	 * function.` |
|        - | 14918 | `	 */` |
|        3 | 14919 | `	return PH7_OK;` |
|        2 | 14920 |  |
|        - | 14921 | `/*` |
|        - | 14922 | ` * include:` |
|        - | 14923 | ` * According to the PHP reference manual.` |
|        - | 14924 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14925 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14926 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14927 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14928 | ` *  and the current working directory before failing. The include()` |
|        - | 14929 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14930 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14931 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14932 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14933 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14934 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14935 | ` *  directory to find the requested file.` |
|        - | 14936 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14937 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14938 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14939 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14940 | ` */` |
|     9554 | 14941 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14942 |  |
|        - | 14943 | `	SyString sFile;` |
|        - | 14944 | `	sxi32 rc;` |
|     9556 | 14945 | `	if( nArg < 1 ){` |
|        - | 14946 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14947 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14948 | `		return SXRET_OK;` |
|        - | 14949 | `	}` |
|        - | 14950 | `	/* File to include */` |
|     9556 | 14951 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9556 | 14952 | `	if( sFile.nByte < 1 ){` |
|        - | 14953 | `		/* Empty string,return NULL */` |
|      ! 0 | 14954 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14955 | `		return SXRET_OK;` |
|        - | 14956 | `	}` |
|        - | 14957 | `	/* Open,compile and execute the desired script */` |
|     9556 | 14958 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9556 | 14959 | `	if( rc != SXRET_OK ){` |
|        - | 14960 | `		/* Emit a warning and return false */` |
|        3 | 14961 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14962 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14963 | `	}` |
|     9556 | 14964 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14965 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 14966 | `		return PH7_ABORT;` |
|        - | 14967 | `	}` |
|     9552 | 14968 | `	return SXRET_OK;` |
|     4779 | 14969 |  |
|        - | 14970 | `/*` |
|        - | 14971 | ` * include_once:` |
|        - | 14972 | ` *  According to the PHP reference manual.` |
|        - | 14973 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14974 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14975 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14976 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14977 | ` *   just once.` |
|        - | 14978 | ` */` |
|        4 | 14979 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14980 |  |
|        - | 14981 | `	SyString sFile;` |
|        - | 14982 | `	sxi32 rc;` |
|        5 | 14983 | `	if( nArg < 1 ){` |
|        - | 14984 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14985 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14986 | `		return SXRET_OK;` |
|        - | 14987 | `	}` |
|        - | 14988 | `	/* File to include */` |
|        5 | 14989 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14990 | `	if( sFile.nByte < 1 ){` |
|        - | 14991 | `		/* Empty string,return NULL */` |
|      ! 0 | 14992 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14993 | `		return SXRET_OK;` |
|        - | 14994 | `	}` |
|        - | 14995 | `	/* Open,compile and execute the desired script */` |
|        5 | 14996 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14997 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14998 | `		/* File already included,return TRUE */` |
|        3 | 14999 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15000 | `		return SXRET_OK;` |
|        - | 15001 | `	}` |
|        3 | 15002 | `	if( rc != SXRET_OK ){` |
|        - | 15003 | `		/* Emit a warning and return false */` |
|      ! 0 | 15004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15005 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15006 | ` 	}` |
|        3 | 15007 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15008 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15009 | `		return PH7_ABORT;` |
|        - | 15010 | `	}` |
|        3 | 15011 | `	return SXRET_OK;` |
|        3 | 15012 |  |
|        - | 15013 | `/*` |
|        - | 15014 | ` * require.` |
|        - | 15015 | ` *  According to the PHP reference manual.` |
|        - | 15016 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15017 | ` *   also produce a fatal level error.` |
|        - | 15018 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15019 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15020 | ` */` |
|        6 | 15021 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15022 |  |
|        - | 15023 | `	SyString sFile;` |
|        - | 15024 | `	sxi32 rc;` |
|        8 | 15025 | `	if( nArg < 1 ){` |
|        - | 15026 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15027 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15028 | `		return SXRET_OK;` |
|        - | 15029 | `	}` |
|        - | 15030 | `	/* File to include */` |
|        8 | 15031 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15032 | `	if( sFile.nByte < 1 ){` |
|        - | 15033 | `		/* Empty string,return NULL */` |
|      ! 0 | 15034 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15035 | `		return SXRET_OK;` |
|        - | 15036 | `	}` |
|        - | 15037 | `	/* Open,compile and execute the desired script */` |
|        8 | 15038 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15039 | `	if( rc != SXRET_OK ){` |
|        - | 15040 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15041 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15042 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15043 | `		return PH7_ABORT;` |
|        - | 15044 | `	}` |
|        8 | 15045 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15046 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15047 | `		return PH7_ABORT;` |
|        - | 15048 | `	}` |
|        8 | 15049 | `	return SXRET_OK;` |
|        5 | 15050 |  |
|        - | 15051 | `/*` |
|        - | 15052 | ` * require_once:` |
|        - | 15053 | ` *  According to the PHP reference manual.` |
|        - | 15054 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15055 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15056 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15057 | ` *   and how it differs from its non _once siblings.` |
|        - | 15058 | ` */` |
|        4 | 15059 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15060 |  |
|        - | 15061 | `	SyString sFile;` |
|        - | 15062 | `	sxi32 rc;` |
|        5 | 15063 | `	if( nArg < 1 ){` |
|        - | 15064 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15065 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15066 | `		return SXRET_OK;` |
|        - | 15067 | `	}` |
|        - | 15068 | `	/* File to include */` |
|        5 | 15069 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15070 | `	if( sFile.nByte < 1 ){` |
|        - | 15071 | `		/* Empty string,return NULL */` |
|      ! 0 | 15072 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15073 | `		return SXRET_OK;` |
|        - | 15074 | `	}` |
|        - | 15075 | `	/* Open,compile and execute the desired script */` |
|        5 | 15076 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15077 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15078 | `		/* File already included,return TRUE */` |
|        3 | 15079 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15080 | `		return SXRET_OK;` |
|        - | 15081 | `	}` |
|        3 | 15082 | `	if( rc != SXRET_OK ){` |
|        - | 15083 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15084 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15085 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15086 | `		return PH7_ABORT;` |
|        - | 15087 | `	}` |
|        3 | 15088 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15089 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15090 | `		return PH7_ABORT;` |
|        - | 15091 | `	}` |
|        3 | 15092 | `	return SXRET_OK;` |
|        3 | 15093 |  |
|        - | 15094 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15095 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15096 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15097 | `/*` |
|        - | 15098 | ` * Section:` |
|        - | 15099 | ` *  SPL Autoloading functions.` |
|        - | 15100 | ` * Status:` |
|        - | 15101 | ` *  Stable.` |
|        - | 15102 | ` */` |
|        - | 15103 | `/*` |
|        - | 15104 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15105 | ` *  Register given function as __autoload() implementation.` |
|        - | 15106 | ` * Parameters` |
|        - | 15107 | ` *  callback` |
|        - | 15108 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15109 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15110 | ` *  throw` |
|        - | 15111 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15112 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15113 | ` *  prepend` |
|        - | 15114 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15115 | ` *   autoload stack instead of appending it.` |
|        - | 15116 | ` * Return` |
|        - | 15117 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15118 | ` */` |
|       34 | 15119 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15120 |  |
|        - | 15121 | `	VmAutoloadCB sEntry;` |
|       36 | 15122 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15123 | `	int iPrepend = 0;` |
|        - | 15124 | `	sxu32 n;` |
|       36 | 15125 | `	if( nArg < 1 ){` |
|        - | 15126 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15127 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15128 | `		/* Check for duplicates first */` |
|        9 | 15129 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15130 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15131 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15132 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15133 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15134 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15135 | `				return SXRET_OK;` |
|        - | 15136 | `			}` |
|      ! 0 | 15137 | `		}` |
|        5 | 15138 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15139 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15140 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15141 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15142 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15143 | `		return SXRET_OK;` |
|        - | 15144 | `	}` |
|        - | 15145 | `	/* Validate that the callback is callable */` |
|       28 | 15146 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15147 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15148 | `		if( nArg >= 2 ){` |
|      ! 0 | 15149 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15150 | `		}` |
|      ! 0 | 15151 | `		if( iThrow ){` |
|      ! 0 | 15152 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15153 | `				"Argument is not callable");` |
|      ! 0 | 15154 | `		}` |
|      ! 0 | 15155 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15156 | `		return SXRET_OK;` |
|        - | 15157 | `	}` |
|        - | 15158 | `	/* Check for duplicates */` |
|       46 | 15159 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15160 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15161 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15162 | `			/* Already registered */` |
|      ! 0 | 15163 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15164 | `			return SXRET_OK;` |
|        - | 15165 | `		}` |
|       11 | 15166 | `	}` |
|        - | 15167 | `	/* Check prepend flag */` |
|       28 | 15168 | `	if( nArg >= 3 ){` |
|        3 | 15169 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15170 | `	}` |
|        - | 15171 | `	/* Store the callback */` |
|       28 | 15172 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15173 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15174 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15175 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15176 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15177 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15178 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15179 | `		VmAutoloadCB *aBase;` |
|        3 | 15180 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15181 | `		/* Rotate: move last entry to front */` |
|        3 | 15182 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15183 | `		if( aBase ){` |
|        - | 15184 | `			VmAutoloadCB sTemp;` |
|        - | 15185 | `			sxu32 i;` |
|        3 | 15186 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15187 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15188 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15189 | `			}` |
|        3 | 15190 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15191 | `		}` |
|        2 | 15192 | `	}else{` |
|       26 | 15193 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15194 | `	}` |
|       28 | 15195 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15196 | `	return SXRET_OK;` |
|       19 | 15197 |  |
|        - | 15198 | `/*` |
|        - | 15199 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15200 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15201 | ` * Parameters` |
|        - | 15202 | ` *  callback` |
|        - | 15203 | ` *   The autoload function being unregistered.` |
|        - | 15204 | ` * Return` |
|        - | 15205 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15206 | ` */` |
|       32 | 15207 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15208 |  |
|       34 | 15209 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15210 | `	sxu32 n,nEntry;` |
|       34 | 15211 | `	if( nArg < 1 ){` |
|      ! 0 | 15212 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15213 | `		return SXRET_OK;` |
|        - | 15214 | `	}` |
|       34 | 15215 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15216 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15217 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15218 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15219 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15220 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15221 | `			sxu32 i;` |
|       32 | 15222 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15223 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15224 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15225 | `			}` |
|        - | 15226 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15227 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15228 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15229 | `			return SXRET_OK;` |
|        - | 15230 | `		}` |
|        3 | 15231 | `	}` |
|        3 | 15232 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15233 | `	return SXRET_OK;` |
|       18 | 15234 |  |
|        - | 15235 | `/*` |
|        - | 15236 | ` * array spl_autoload_functions(void)` |
|        - | 15237 | ` *  Return all registered __autoload() functions.` |
|        - | 15238 | ` * Return` |
|        - | 15239 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15240 | ` *  an empty array is returned.` |
|        - | 15241 | ` */` |
|       20 | 15242 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15243 |  |
|       21 | 15244 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15245 | `	ph7_value *pArray;` |
|        - | 15246 | `	sxu32 n,nEntry;` |
|       10 | 15247 | `	SXUNUSED(nArg);` |
|       10 | 15248 | `	SXUNUSED(apArg);` |
|       21 | 15249 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15250 | `	if( pArray == 0 ){` |
|      ! 0 | 15251 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15252 | `		return SXRET_OK;` |
|        - | 15253 | `	}` |
|       21 | 15254 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15255 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15256 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15257 | `		if( pEntry ){` |
|       15 | 15258 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15259 | `		}` |
|        8 | 15260 | `	}` |
|       21 | 15261 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15262 | `	return SXRET_OK;` |
|       11 | 15263 |  |
|        - | 15264 | `/*` |
|        - | 15265 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15266 | ` *  Default implementation of __autoload().` |
|        - | 15267 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15268 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15269 | ` * Parameters` |
|        - | 15270 | ` *  class` |
|        - | 15271 | ` *   The class name being searched.` |
|        - | 15272 | ` *  file_extensions` |
|        - | 15273 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15274 | ` */` |
|        2 | 15275 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15276 |  |
|        - | 15277 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15278 | `	SyBlob sPath;` |
|        - | 15279 | `	int nClass;` |
|        - | 15280 | `	sxi32 rc;` |
|        3 | 15281 | `	if( nArg < 1 ){` |
|      ! 0 | 15282 | `		return SXRET_OK;` |
|        - | 15283 | `	}` |
|        3 | 15284 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15285 | `	if( nClass < 1 ){` |
|      ! 0 | 15286 | `		return SXRET_OK;` |
|        - | 15287 | `	}` |
|        - | 15288 | `	/* Default extensions */` |
|        3 | 15289 | `	zExt = ".php,.inc";` |
|        3 | 15290 | `	if( nArg >= 2 ){` |
|        - | 15291 | `		int nExt;` |
|      ! 0 | 15292 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15293 | `		if( nExt < 1 ){` |
|      ! 0 | 15294 | `			zExt = ".php,.inc";` |
|      ! 0 | 15295 | `		}` |
|      ! 0 | 15296 | `	}` |
|        3 | 15297 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15298 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15299 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15300 | `	zCur = zExt;` |
|        7 | 15301 | `	while( zCur < zEnd ){` |
|        - | 15302 | `		const char *zComma;` |
|        - | 15303 | `		SyString sFile;` |
|        - | 15304 | `		int i;` |
|        - | 15305 | `		/* Find next comma or end */` |
|        5 | 15306 | `		zComma = zCur;` |
|       21 | 15307 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15308 | `			zComma++;` |
|        1 | 15309 | `		}` |
|        - | 15310 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15311 | `		SyBlobReset(&sPath);` |
|       69 | 15312 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15313 | `			char c = zClass[i];` |
|       65 | 15314 | `			if( c == '\\' ){` |
|      ! 0 | 15315 | `				c = '/';` |
|       65 | 15316 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15317 | `				c = c + ('a' - 'A');` |
|        6 | 15318 | `			}` |
|       65 | 15319 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15320 | `		}` |
|        - | 15321 | `		/* Append extension */` |
|        5 | 15322 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15323 | `		/* Try to include the file */` |
|        5 | 15324 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15325 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15326 | `		if( rc == SXRET_OK ){` |
|        - | 15327 | `			/* File included successfully */` |
|      ! 0 | 15328 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15329 | `			return SXRET_OK;` |
|        - | 15330 | `		}` |
|        - | 15331 | `		/* Move past the comma */` |
|        5 | 15332 | `		zCur = zComma;` |
|        5 | 15333 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15334 | `			zCur++;` |
|        1 | 15335 | `		}` |
|        1 | 15336 | `	}` |
|        3 | 15337 | `	SyBlobRelease(&sPath);` |
|        3 | 15338 | `	return SXRET_OK;` |
|        2 | 15339 |  |
|        - | 15340 | `/* Table of built-in VM functions. */` |
|        - | 15341 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15342 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15343 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15344 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15345 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15346 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15347 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15348 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15349 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15350 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15351 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15352 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15353 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15354 | `	    /* Constants management */` |
|        - | 15355 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15356 | `	{ "define",   vm_builtin_define               },` |
|        - | 15357 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15358 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15359 | `	   /* Class/Object functions */` |
|        - | 15360 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15361 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15362 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15363 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15364 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15365 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15366 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15367 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15368 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15369 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15370 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15371 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15372 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15373 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15374 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15375 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15376 | `	   /* SPL Autoloading */` |
|        - | 15377 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15378 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15379 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15380 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15381 | `	   /* Random numbers/strings generators */` |
|        - | 15382 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15383 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15384 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15385 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15386 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15387 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15388 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15389 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15390 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15391 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15392 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15393 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15394 | `	   /* Language constructs functions */` |
|        - | 15395 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15396 | `	{ "print", vm_builtin_print                   },` |
|        - | 15397 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15398 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15399 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15400 | `	  /* Variable handling functions */` |
|        - | 15401 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15402 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15403 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15404 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15405 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15406 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15407 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15408 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15409 | `	  /* Ouput control functions */` |
|        - | 15410 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15411 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15412 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15413 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15414 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15415 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15416 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15417 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15418 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15419 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15420 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15421 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15422 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15423 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15424 | `	  /* Assertion functions */` |
|        - | 15425 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15426 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15427 | `	  /* Error reporting functions */` |
|        - | 15428 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15429 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15430 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15431 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15432 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15433 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15434 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15435 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15436 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15437 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15438 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15439 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15440 | `	  /* Release info */` |
|        - | 15441 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15442 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15443 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15444 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15445 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15446 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15447 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15448 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15449 | `	  /* hashmap */` |
|        - | 15450 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15451 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15452 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15453 | `	  /* URL related function */` |
|        - | 15454 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15455 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15456 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15457 | `	   /* XML processing functions */` |
|        - | 15458 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15459 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15460 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15461 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15462 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15463 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15464 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15465 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15466 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15467 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15468 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15469 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15470 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15471 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15472 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15473 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15474 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15475 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15476 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15477 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15478 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15479 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15480 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15481 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15482 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15483 | `	   /* Command line processing */` |
|        - | 15484 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15485 | `	   /* JSON encoding/decoding */` |
|        - | 15486 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15487 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15488 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15489 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15490 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15491 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15492 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15493 | `	   /* Files/URI inclusion facility */` |
|        - | 15494 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15495 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15496 | `	{ "include",      vm_builtin_include          },` |
|        - | 15497 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15498 | `	{ "require",      vm_builtin_require          },` |
|        - | 15499 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15500 | `};` |
|        - | 15501 | `/*` |
|        - | 15502 | ` * Register the built-in VM functions defined above.` |
|        - | 15503 | ` */` |
|     2820 | 15504 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15505 |  |
|        - | 15506 | `	sxi32 rc;` |
|        - | 15507 | `	sxu32 n;` |
|   380702 | 15508 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15509 | `		/* Note that these special functions have access` |
|        - | 15510 | `		 * to the underlying virtual machine as their` |
|        - | 15511 | `		 * private data.` |
|        - | 15512 | `		 */` |
|   377882 | 15513 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   377882 | 15514 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15515 | `			return rc;` |
|        - | 15516 | `		}` |
|   188942 | 15517 | `	}` |
|     2822 | 15518 | `	return SXRET_OK;` |
|     1412 | 15519 |  |
|        - | 15520 | `/*` |
|        - | 15521 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15522 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15523 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15524 | ` */` |
|   182018 | 15525 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15526 |  |
|   182020 | 15527 | `	if( !iLoadable ){` |
|   179940 | 15528 | `		return pClass;` |
|        - | 15529 | `	}` |
|     2086 | 15530 | `	while(pClass){` |
|     2082 | 15531 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2078 | 15532 | `			return pClass;` |
|        - | 15533 | `		}` |
|        5 | 15534 | `		pClass = pClass->pNextName;` |
|        1 | 15535 | `	}` |
|        5 | 15536 | `	return 0;` |
|    91011 | 15537 |  |
|        - | 15538 | `/*` |
|        - | 15539 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15540 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15541 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15542 | ` * registered in the VM's class table.` |
|        - | 15543 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15544 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15545 | ` */` |
|       38 | 15546 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15547 |  |
|        - | 15548 | `	VmAutoloadCB *pEntry;` |
|        - | 15549 | `	ph7_value sArg,sResult;` |
|        - | 15550 | `	SyHashEntry *pHashEntry;` |
|        - | 15551 | `	ph7_class *pClass;` |
|        - | 15552 | `	sxu32 n,nEntry;` |
|       40 | 15553 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15554 | `	if( nEntry < 1 ){` |
|       26 | 15555 | `		return 0;` |
|        - | 15556 | `	}` |
|        - | 15557 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15558 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15559 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15560 | `	}` |
|        - | 15561 | `	/* Mark this class as being autoloaded */` |
|       14 | 15562 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15563 | `	/* Prepare the class name argument */` |
|       14 | 15564 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15565 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15566 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15567 | `	pClass = 0;` |
|       28 | 15568 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15569 | `		ph7_value *apArg[1];` |
|       24 | 15570 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15571 | `		if( pEntry == 0 ){` |
|      ! 0 | 15572 | `			continue;` |
|        - | 15573 | `		}` |
|       24 | 15574 | `		apArg[0] = &sArg;` |
|       24 | 15575 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15576 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15577 | `			continue;` |
|        - | 15578 | `		}` |
|        - | 15579 | `		/* Check if the class is now available */` |
|       24 | 15580 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15581 | `		if( pHashEntry ){` |
|       10 | 15582 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15583 | `			if( pClass ){` |
|       10 | 15584 | `				break;` |
|        - | 15585 | `			}` |
|      ! 0 | 15586 | `		}` |
|        9 | 15587 | `	}` |
|       14 | 15588 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15589 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15590 | `	/* Remove reentrancy guard */` |
|       14 | 15591 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15592 | `	return pClass;` |
|       21 | 15593 |  |
|        - | 15594 | `/*` |
|        - | 15595 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15596 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15597 | ` */` |
|       18 | 15598 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15599 |  |
|       20 | 15600 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15601 |  |
|        - | 15602 | `/*` |
|        - | 15603 | ` * Check if the given name refer to an installed class.` |
|        - | 15604 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15605 | ` */` |
|   182030 | 15606 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15607 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15608 | `	const char *zName,  /* Name of the target class */` |
|        - | 15609 | `	sxu32 nByte,        /* zName length */` |
|        - | 15610 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15611 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15612 | `						 */` |
|        - | 15613 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15614 | `	)` |
|        2 | 15615 |  |
|        - | 15616 | `	SyHashEntry *pEntry;` |
|        - | 15617 | `	ph7_class *pClass;` |
|    91015 | 15618 | `	SXUNUSED(iNest);` |
|        - | 15619 | `	/* Exact class lookup.` |
|        - | 15620 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15621 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   182032 | 15622 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   182032 | 15623 | `	if( pEntry == 0 ){` |
|        - | 15624 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15625 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15626 | `	}` |
|   182012 | 15627 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   182012 | 15628 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    91017 | 15629 |  |
|        - | 15630 | `/*` |
|        - | 15631 | ` * Reference Table Implementation` |
|        - | 15632 | ` * Status: stable <chm@symisc.net>` |
|        - | 15633 | ` * Intro` |
|        - | 15634 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15635 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15636 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15637 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15638 | ` *  Refer to the official for more information on this powerful` |
|        - | 15639 | ` *  extension.` |
|        - | 15640 | ` */` |
|        - | 15641 | `/*` |
|        - | 15642 | ` * Allocate a new reference entry.` |
|        - | 15643 | ` */` |
|  3202014 | 15644 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15645 |  |
|        - | 15646 | `	VmRefObj *pRef;` |
|        - | 15647 | `	/* Allocate a new instance */` |
|  3202016 | 15648 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3202016 | 15649 | `	if( pRef == 0 ){` |
|      ! 0 | 15650 | `		return 0;` |
|        - | 15651 | `	}` |
|        - | 15652 | `	/* Zero the structure */` |
|  3202016 | 15653 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15654 | `	/* Initialize fields */` |
|  3202016 | 15655 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3202016 | 15656 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3202016 | 15657 | `	pRef->nIdx = nIdx;` |
|  3202016 | 15658 | `	return pRef;` |
|  1601009 | 15659 |  |
|        - | 15660 | `/*` |
|        - | 15661 | ` * Default hash function used by the reference table` |
|        - | 15662 | ` * for lookup/insertion operations.` |
|        - | 15663 | ` */` |
| 17538728 | 15664 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15665 |  |
|        - | 15666 | `	/* Calculate the hash based on the memory object index */` |
| 17538730 | 15667 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15668 |  |
|        - | 15669 | `/*` |
|        - | 15670 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15671 | ` * in the reference table.` |
|        - | 15672 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15673 | ` * otherwise.` |
|        - | 15674 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15675 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15676 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15677 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15678 | ` * Refer to the official for more information on this powerful` |
|        - | 15679 | ` * extension.` |
|        - | 15680 | ` */` |
|  9546546 | 15681 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15682 |  |
|        - | 15683 | `	VmRefObj *pRef;` |
|        - | 15684 | `	sxu32 nBucket;` |
|        - | 15685 | `	/* Point to the appropriate bucket */` |
|  9546548 | 15686 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15687 | `	/* Perform the lookup */` |
|  9546548 | 15688 | `	pRef = pVm->apRefObj[nBucket];` |
| 20994917 | 15689 | `	for(;;){` |
| 41972363 | 15690 | `		if( pRef == 0 ){` |
|  3307260 | 15691 | `			break;` |
|        - | 15692 | `		}` |
| 38665105 | 15693 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15694 | `			/* Entry found */` |
|  6239290 | 15695 | `			return pRef;` |
|        - | 15696 | `		}` |
|        - | 15697 | `		/* Point to the next entry */` |
| 32425817 | 15698 | `		pRef = pRef->pNextCollide;` |
|        2 | 15699 | `	}` |
|        - | 15700 | `	/* No such entry,return NULL */` |
|  3307260 | 15701 | `	return 0;` |
|  4773275 | 15702 |  |
|        - | 15703 | `/*` |
|        - | 15704 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15705 | ` *` |
|        - | 15706 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15707 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15708 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15709 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15710 | ` * Refer to the official for more information on this powerful` |
|        - | 15711 | ` * extension.` |
|        - | 15712 | ` */` |
|  3202014 | 15713 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15714 |  |
|        - | 15715 | `	sxu32 nBucket;` |
|  3202016 | 15716 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15717 | `		VmRefObj **apNew;` |
|        - | 15718 | `		sxu32 nNew;` |
|        - | 15719 | `		/* Allocate a larger table */` |
|     4472 | 15720 | `		nNew = pVm->nRefSize << 1;` |
|     4472 | 15721 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4472 | 15722 | `		if( apNew ){` |
|     4472 | 15723 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15724 | `			sxu32 n;` |
|        - | 15725 | `			/* Zero the structure */` |
|     4472 | 15726 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15727 | `			/* Rehash all referenced entries */` |
|  2847974 | 15728 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15729 | `				/* Remove old collision links */` |
|  2843504 | 15730 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15731 | `				/* Point to the appropriate bucket */` |
|  2843504 | 15732 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15733 | `				/* Insert the entry  */` |
|  2843504 | 15734 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843504 | 15735 | `				if( apNew[nBucket] ){` |
|  2301116 | 15736 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15737 | `				}` |
|  2843504 | 15738 | `				apNew[nBucket] = pEntry;` |
|        - | 15739 | `				/* Point to the next entry */` |
|  2843504 | 15740 | `				pEntry = pEntry->pNext;` |
|  1421753 | 15741 | `			}` |
|        - | 15742 | `			/* Release the old table */` |
|     4472 | 15743 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15744 | `			/* Install the new one */` |
|     4472 | 15745 | `			pVm->apRefObj = apNew;` |
|     4472 | 15746 | `			pVm->nRefSize = nNew;` |
|     2235 | 15747 | `		}` |
|     2235 | 15748 | `	}` |
|        - | 15749 | `	/* Point to the appropriate bucket */` |
|  3202016 | 15750 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15751 | `	/* Insert the entry */` |
|  3202016 | 15752 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3202016 | 15753 | `	if( pVm->apRefObj[nBucket] ){` |
|  2614331 | 15754 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307173 | 15755 | `	}` |
|  3202016 | 15756 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3202016 | 15757 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3202016 | 15758 | `	pVm->nRefUsed++;` |
|  3202016 | 15759 | `	return SXRET_OK;` |
|        2 | 15760 |  |
|        - | 15761 | `/*` |
|        - | 15762 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15763 | ` * the reference table.` |
|        - | 15764 | ` * This function is invoked when the user perform an unset` |
|        - | 15765 | ` * call [i.e: unset($var); ].` |
|        - | 15766 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15767 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15768 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15769 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15770 | ` * Refer to the official for more information on this powerful` |
|        - | 15771 | ` * extension.` |
|        - | 15772 | ` */` |
|  3160862 | 15773 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15774 |  |
|        - | 15775 | `	ph7_hashmap_node **apNode;` |
|        - | 15776 | `	SyHashEntry **apEntry;` |
|        - | 15777 | `	sxu32 n;` |
|        - | 15778 | `	/* Point to the reference table */` |
|  3160864 | 15779 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3160864 | 15780 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15781 | `	/* Unlink the entry from the reference table */` |
|  3271976 | 15782 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   111114 | 15783 | `		if( apEntry[n] ){` |
|   111064 | 15784 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55531 | 15785 | `		}` |
|    55558 | 15786 | `	}` |
|  6210610 | 15787 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3049748 | 15788 | `		if( apNode[n] ){` |
|     6816 | 15789 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3407 | 15790 | `		}` |
|  1524875 | 15791 | `	}` |
|  3160864 | 15792 | `	if( pRef->pPrevCollide ){` |
|  1214198 | 15793 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   607741 | 15794 | `	}else{` |
|  1946668 | 15795 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15796 | `	}` |
|  3160864 | 15797 | `	if( pRef->pNextCollide ){` |
|  1801376 | 15798 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   900684 | 15799 | `	}` |
|  3160864 | 15800 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15801 | `	/* Release the node */` |
|  3160864 | 15802 | `	SySetRelease(&pRef->aReference);` |
|  3160864 | 15803 | `	SySetRelease(&pRef->aArrEntries);` |
|  3160864 | 15804 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3160864 | 15805 | `	pVm->nRefUsed--;` |
|  3160864 | 15806 | `	return SXRET_OK;` |
|        2 | 15807 |  |
|        - | 15808 | `/*` |
|        - | 15809 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15810 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15811 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15812 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15813 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15814 | ` * Refer to the official for more information on this powerful` |
|        - | 15815 | ` * extension.` |
|        - | 15816 | ` */` |
|  3237430 | 15817 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15818 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15819 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15820 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15821 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15822 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15823 | `	)` |
|        2 | 15824 |  |
|  3237432 | 15825 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15826 | `	VmRefObj *pRef;` |
|        - | 15827 | `	/* Check if the referenced object already exists */` |
|  3237432 | 15828 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3237432 | 15829 | `	if( pRef == 0 ){` |
|        - | 15830 | `		/* Create a new entry */` |
|  3202016 | 15831 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3202016 | 15832 | `		if( pRef == 0 ){` |
|      ! 0 | 15833 | `			return SXERR_MEM;` |
|        - | 15834 | `		}` |
|  3202016 | 15835 | `		pRef->iFlags = iFlags;` |
|        - | 15836 | `		/* Install the entry */` |
|  3202016 | 15837 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1601007 | 15838 | `	}` |
|  3237432 | 15839 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3237432 | 15840 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15841 | `		VmSlot sRef;` |
|        - | 15842 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15843 | `		 * be deleted when we leave this frame.` |
|        - | 15844 | `		 */` |
|   105354 | 15845 | `		sRef.nIdx = nIdx;` |
|   105354 | 15846 | `		sRef.pUserData = pEntry;` |
|   105354 | 15847 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15848 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15849 | `		}` |
|    52676 | 15850 | `	}` |
|  3237432 | 15851 | `	if( pEntry ){` |
|        - | 15852 | `		/* Address of the hash-entry */` |
|   140546 | 15853 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70272 | 15854 | `	}` |
|  3237432 | 15855 | `	if( pMapEntry ){` |
|        - | 15856 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3088498 | 15857 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1544248 | 15858 | `	}` |
|  3237432 | 15859 | `	return SXRET_OK;` |
|  1618717 | 15860 |  |
|        - | 15861 | `/*` |
|        - | 15862 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15863 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15864 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15865 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15866 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15867 | ` * Refer to the official for more information on this powerful` |
|        - | 15868 | ` * extension.` |
|        - | 15869 | ` */` |
|  3148248 | 15870 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15871 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15872 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15873 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15874 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15875 | `	)` |
|        2 | 15876 |  |
|        - | 15877 | `	VmRefObj *pRef;` |
|        - | 15878 | `	sxu32 n;` |
|        - | 15879 | `	/* Check if the referenced object already exists */` |
|  3148250 | 15880 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3148250 | 15881 | `	if( pRef == 0 ){` |
|        - | 15882 | `		/* Not such entry */` |
|   105240 | 15883 | `		return SXERR_NOTFOUND;` |
|        - | 15884 | `	}` |
|        - | 15885 | `	/* Remove the desired entry */` |
|  3043012 | 15886 | `	if( pEntry ){` |
|        - | 15887 | `		SyHashEntry **apEntry;` |
|       74 | 15888 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 15889 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 15890 | `			if( apEntry[n] == pEntry ){` |
|        - | 15891 | `				/* Nullify the entry */` |
|       74 | 15892 | `				apEntry[n] = 0;` |
|        - | 15893 | `				/*` |
|        - | 15894 | `				 * NOTE:` |
|        - | 15895 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15896 | `				 * we avoid wasting spaces.` |
|        - | 15897 | `				 */` |
|       36 | 15898 | `			}` |
|       97 | 15899 | `		}` |
|       36 | 15900 | `	}` |
|  3043012 | 15901 | `	if( pMapEntry ){` |
|        - | 15902 | `		ph7_hashmap_node **apNode;` |
|  3042940 | 15903 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6085972 | 15904 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3043034 | 15905 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15906 | `				/* nullify the entry */` |
|  3042940 | 15907 | `				apNode[n] = 0;` |
|  1521469 | 15908 | `			}` |
|  1521518 | 15909 | `		}` |
|  1521469 | 15910 | `	}` |
|  3043012 | 15911 | `	return SXRET_OK;` |
|  1574126 | 15912 |  |
|        - | 15913 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15914 | `/*` |
|        - | 15915 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15916 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15917 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15918 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15919 | ` * For more information on how to register IO stream devices,please` |
|        - | 15920 | ` * refer to the official documentation.` |
|        - | 15921 | ` */` |
|    29088 | 15922 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15923 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15924 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15925 | `	int nByte              /* *pzDevice length*/` |
|        - | 15926 | `	)` |
|        2 | 15927 |  |
|        - | 15928 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15929 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15930 | `	SyString sDev,sCur;` |
|        - | 15931 | `	sxu32 n,nEntry;` |
|        - | 15932 | `	int rc;` |
|        - | 15933 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29090 | 15934 | `	zNext = zCur = zIn = *pzDevice;` |
|    29090 | 15935 | `	zEnd = &zIn[nByte];` |
|  1858206 | 15936 | `	while( zIn < zEnd ){` |
|  1829120 | 15937 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15938 | `			/* Got one */` |
|        3 | 15939 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15940 | `			break;` |
|        - | 15941 | `		}` |
|        - | 15942 | `		/* Advance the cursor */` |
|  1829118 | 15943 | `		zIn++;` |
|        2 | 15944 | `	}` |
|    29090 | 15945 | `	if( zIn >= zEnd ){` |
|        - | 15946 | `		/* No such scheme,return the default stream */` |
|    29088 | 15947 | `		return pVm->pDefStream;` |
|        - | 15948 | `	}` |
|        3 | 15949 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15950 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15951 | `	SyStringFullTrim(&sDev);` |
|        - | 15952 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15953 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15954 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15955 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15956 | `		pStream = apStream[n];` |
|        3 | 15957 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15958 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15959 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15960 | `		if( rc == 0 ){` |
|        - | 15961 | `			/* Stream device found */` |
|        3 | 15962 | `			*pzDevice = zNext;` |
|        3 | 15963 | `			return pStream;` |
|        - | 15964 | `		}` |
|      ! 0 | 15965 | `	}` |
|        - | 15966 | `	/* No such stream,return NULL */` |
|      ! 0 | 15967 | `	return 0;` |
|    14546 | 15968 |  |
|        - | 15969 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15970 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15971 |  |
