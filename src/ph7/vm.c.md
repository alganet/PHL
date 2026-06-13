# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6547/8394 lines (78.00%)

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
|   916208 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   141 |  |
|   916210 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   916176 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   916166 |   148 | `	return FALSE;` |
|   458128 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   335432 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   335434 |   162 | `	sxu8 bReal = FALSE;` |
|   335434 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   335434 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335364 |   166 | `		return FALSE;` |
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
|   167740 |   183 |  |
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
|   757600 |   366 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   367 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   368 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   369 | `	SyString *pName     /* Function name */` |
|        - |   370 | `	)` |
|        2 |   371 |  |
|        - |   372 | `	SyHashEntry *pEntry;` |
|        - |   373 | `	sxi32 rc;` |
|   757602 |   374 | `	if( pName == 0 ){` |
|        - |   375 | `		/* Use the built-in name */` |
|    41812 |   376 | `		pName = &pFunc->sName;` |
|    20905 |   377 | `	}` |
|        - |   378 | `	/* Check for duplicates (functions with the same name) first */` |
|   757602 |   379 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   757602 |   380 | `	if( pEntry ){` |
|   561664 |   381 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   561664 |   382 | `		if( pLink != pFunc ){` |
|        - |   383 | `			/* Link */` |
|      188 |   384 | `			pFunc->pNextName = pLink;` |
|      188 |   385 | `			pEntry->pUserData = pFunc;` |
|       93 |   386 | `		}` |
|   561664 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|        - |   389 | `	/* First time seen */` |
|   195940 |   390 | `	pFunc->pNextName = 0;` |
|   195940 |   391 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   195940 |   392 | `	return rc;` |
|   378802 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   396 | ` */` |
|    79360 |   397 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   398 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   399 | `	ph7_class *pClass /* Target Class */` |
|        - |   400 | `	)` |
|        2 |   401 |  |
|    79362 |   402 | `	SyString *pName = &pClass->sName;` |
|        - |   403 | `	SyHashEntry *pEntry;` |
|        - |   404 | `	sxi32 rc;` |
|        - |   405 | `	/* Check for duplicates */` |
|    79362 |   406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    79362 |   407 | `	if( pEntry ){` |
|       31 |   408 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   409 | `		/* Link entry with the same name */` |
|       31 |   410 | `		pClass->pNextName = pLink;` |
|       31 |   411 | `		pEntry->pUserData = pClass;` |
|       31 |   412 | `		return SXRET_OK;` |
|        - |   413 | `	}` |
|    79332 |   414 | `	pClass->pNextName = 0;` |
|        - |   415 | `	/* Perform a simple hashtable insertion */` |
|    79332 |   416 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    79332 |   417 | `	return rc;` |
|    39682 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Instruction builder interface.` |
|        - |   421 | ` */` |
|  4248912 |   422 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4248914 |   434 | `	sInstr.iOp = (sxu8)iOp;` |
|  4248914 |   435 | `	sInstr.iP1 = iP1;` |
|  4248914 |   436 | `	sInstr.iP2 = iP2;` |
|  4248914 |   437 | `	sInstr.p3  = p3;` |
|  4248914 |   438 | `	if( pIndex ){` |
|        - |   439 | `		/* Instruction index in the bytecode array */` |
|   230744 |   440 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   115371 |   441 | `	}` |
|        - |   442 | `	/* Finally,record the instruction */` |
|  4248914 |   443 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4248914 |   444 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   445 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   446 | `		/* Fall throw */` |
|      ! 0 |   447 | `	}` |
|  4248914 |   448 | `	return rc;` |
|        2 |   449 |  |
|        - |   450 | `/*` |
|        - |   451 | ` * Swap the current bytecode container with the given one.` |
|        - |   452 | ` */` |
|   551520 |   453 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   454 |  |
|   551522 |   455 | `	if( pContainer == 0 ){` |
|        - |   456 | `		/* Point to the default container */` |
|      ! 0 |   457 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   458 | `	}else{` |
|        - |   459 | `		/* Change container */` |
|   551522 |   460 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   461 | `	}` |
|   551522 |   462 | `	return SXRET_OK;` |
|        2 |   463 |  |
|        - |   464 | `/*` |
|        - |   465 | ` * Return the current bytecode container.` |
|        - |   466 | ` */` |
|   275760 |   467 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   468 |  |
|   275762 |   469 | `	return pVm->pByteContainer;` |
|        2 |   470 |  |
|        - |   471 | `/*` |
|        - |   472 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   473 | ` */` |
|   227532 |   474 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   475 |  |
|        - |   476 | `	VmInstr *pInstr;` |
|   227534 |   477 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   227534 |   478 | `	return pInstr;` |
|        2 |   479 |  |
|        - |   480 | `/*` |
|        - |   481 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   482 | ` */` |
|  1276468 |   483 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   484 |  |
|  1276470 |   485 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Pop the last VM instruction.` |
|        - |   489 | ` */` |
|   210488 |   490 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   491 |  |
|   210490 |   492 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   493 |  |
|        - |   494 | `/*` |
|        - |   495 | ` * Peek the last VM instruction.` |
|        - |   496 | ` */` |
|   836786 |   497 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   498 |  |
|   836788 |   499 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   500 |  |
|    33368 |   501 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   502 |  |
|        - |   503 | `	VmInstr *aInstr;` |
|        - |   504 | `	sxu32 n;` |
|    33370 |   505 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33370 |   506 | `	if( n < 2 ){` |
|      ! 0 |   507 | `		return 0;` |
|        - |   508 | `	}` |
|    33370 |   509 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33370 |   510 | `	return &aInstr[n - 2];` |
|    16686 |   511 |  |
|        - |   512 | `/*` |
|        - |   513 | ` * Allocate a new virtual machine frame.` |
|        - |   514 | ` */` |
|    22300 |   515 | `static VmFrame * VmNewFrame(` |
|        - |   516 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   517 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   518 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   519 | `	)` |
|        2 |   520 |  |
|        - |   521 | `	VmFrame *pFrame;` |
|        - |   522 | `	/* Allocate a new vm frame */` |
|    22302 |   523 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    22302 |   524 | `	if( pFrame == 0 ){` |
|      ! 0 |   525 | `		return 0;` |
|        - |   526 | `	}` |
|        - |   527 | `	/* Zero the structure */` |
|    22302 |   528 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   529 | `	/* Initialize frame fields */` |
|    22302 |   530 | `	pFrame->pUserData = pUserData;` |
|    22302 |   531 | `	pFrame->pThis = pThis;` |
|    22302 |   532 | `	pFrame->pVm = pVm;` |
|    22302 |   533 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    22302 |   534 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    22302 |   535 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    22302 |   536 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    22302 |   537 | `	return pFrame;` |
|    11152 |   538 |  |
|        - |   539 | `/* Forward declaration */` |
|        - |   540 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   541 | `/*` |
|        - |   542 | ` * Enter a VM frame.` |
|        - |   543 | ` */` |
|    22254 |   544 | `static sxi32 VmEnterFrame(` |
|        - |   545 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   546 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   547 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   548 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   549 | `	)` |
|        2 |   550 |  |
|        - |   551 | `	VmFrame *pFrame;` |
|        - |   552 | `	/* Allocate a new frame */` |
|    22256 |   553 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    22256 |   554 | `	if( pFrame == 0 ){` |
|      ! 0 |   555 | `		return SXERR_MEM;` |
|        - |   556 | `	}` |
|        - |   557 | `	/* Link to the list of active VM frame */` |
|    22256 |   558 | `	pFrame->pParent = pVm->pFrame;` |
|    22256 |   559 | `	pVm->pFrame = pFrame;` |
|    22256 |   560 | `	if( ppFrame ){` |
|        - |   561 | `		/* Write a pointer to the new VM frame */` |
|    19122 |   562 | `		*ppFrame = pFrame;` |
|     9560 |   563 | `	}` |
|    22256 |   564 | `	return SXRET_OK;` |
|    11129 |   565 |  |
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
|    19110 |   609 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   610 |  |
|    19112 |   611 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    19112 |   612 | `	if( pCurFrame ){` |
|        - |   613 | `		/* Unlink from the list of active VM frame */` |
|    19112 |   614 | `		pVm->pFrame = pCurFrame->pParent;` |
|    19112 |   615 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   616 | `			VmSlot  *aSlot;` |
|        - |   617 | `			sxu32 n;` |
|        - |   618 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18752 |   619 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   123804 |   620 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   621 | `				/* Unset the local variable */` |
|   105054 |   622 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    52528 |   623 | `			}` |
|        - |   624 | `			/* Remove local reference */` |
|    18752 |   625 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   123878 |   626 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   105128 |   627 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    52565 |   628 | `			}` |
|     9375 |   629 | `		}` |
|        - |   630 | `		/* Release internal containers */` |
|    19112 |   631 | `		SyHashRelease(&pCurFrame->hVar);` |
|    19112 |   632 | `		SySetRelease(&pCurFrame->sArg);` |
|    19112 |   633 | `		SySetRelease(&pCurFrame->sLocal);` |
|    19112 |   634 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   635 | `		/* Release the whole structure */` |
|    19112 |   636 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9555 |   637 | `	}` |
|    19112 |   638 |  |
|        - |   639 | `/*` |
|        - |   640 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   641 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   642 | ` * should be skipped when looking for the real execution context.` |
|        - |   643 | ` */` |
|  7108698 |   644 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   645 |  |
|  7110854 |   646 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2156 |   647 | `		pFrame = pFrame->pParent;` |
|        2 |   648 | `	}` |
|  7108700 |   649 | `	return pFrame;` |
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
|   275528 |   788 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   789 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   790 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   791 | `	)` |
|        2 |   792 |  |
|        - |   793 | `	ph7_class_method *pMeth;` |
|        - |   794 | `	ph7_class_attr *pAttr;` |
|        - |   795 | `	SyHashEntry *pEntry;` |
|        - |   796 | `	sxi32 rc;` |
|        - |   797 | `	/* Reset the loop cursor */` |
|   275530 |   798 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   799 | `	/* Process only static and constant attribute */` |
|   815872 |   800 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   801 | `		/* Extract the current attribute */` |
|   402580 |   802 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   402580 |   803 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   275530 |   848 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   849 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   850 | `		 */` |
|   191324 |   851 | `		return SXRET_OK;` |
|        - |   852 | `	}` |
|        - |   853 | `	/* Create constructor alias if not yet done */` |
|    84208 |   854 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   855 | `		/* User constructor with the same base class name */` |
|     6658 |   856 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6658 |   857 | `		if( pEntry ){` |
|      ! 0 |   858 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   859 | `			/* Create the alias */` |
|      ! 0 |   860 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   861 | `		}` |
|     3328 |   862 | `	}` |
|        - |   863 | `	/* Install the methods now */` |
|    84208 |   864 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   842109 |   865 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   715800 |   866 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   715800 |   867 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   715792 |   868 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   715792 |   869 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   870 | `				return rc;` |
|        - |   871 | `			}` |
|   357895 |   872 | `		}` |
|        2 |   873 | `	}` |
|        - |   874 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    84208 |   875 | `	pClass->bMounted = TRUE;` |
|    84208 |   876 | `	return SXRET_OK;` |
|   137766 |   877 |  |
|        - |   878 | `/*` |
|        - |   879 | ` * Allocate a private frame for attributes of the given` |
|        - |   880 | ` * class instance (Object in the PHP jargon).` |
|        - |   881 | ` */` |
|     2072 |   882 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   883 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   884 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   885 | `	)` |
|        2 |   886 |  |
|     2074 |   887 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   888 | `	ph7_class_attr *pAttr;` |
|        - |   889 | `	SyHashEntry *pEntry;` |
|        - |   890 | `	sxi32 rc;` |
|        - |   891 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2074 |   892 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8526 |   893 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   894 | `		VmClassAttr *pVmAttr;` |
|        - |   895 | `		/* Extract the current attribute */` |
|     6454 |   896 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6454 |   897 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6454 |   898 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   899 | `			return SXERR_MEM;` |
|        - |   900 | `		}` |
|     6454 |   901 | `		pVmAttr->pAttr = pAttr;` |
|     6454 |   902 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   903 | `			ph7_value *pMemObj;` |
|        - |   904 | `			/* Reserve a memory object for this attribute */` |
|     6428 |   905 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6428 |   906 | `			if( pMemObj == 0 ){` |
|      ! 0 |   907 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   908 | `				return SXERR_MEM;` |
|        - |   909 | `			}` |
|     6428 |   910 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6428 |   911 | `			pVmAttr->iState = 0;` |
|     6428 |   912 | `			pVmAttr->pOwner = pClass;` |
|     6428 |   913 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   914 | `				/* Initialize attribute default value (any complex expression) */` |
|     2210 |   915 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5324 |   916 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   917 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   918 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   919 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   920 | `			}` |
|     6428 |   921 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6428 |   922 | `			if( rc != SXRET_OK ){` |
|        - |   923 | `				VmSlot sSlot;` |
|        - |   924 | `				/* Restore memory object */` |
|      ! 0 |   925 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   926 | `				sSlot.pUserData = 0;` |
|      ! 0 |   927 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   928 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   929 | `				return SXERR_MEM;` |
|        - |   930 | `			}` |
|        - |   931 | `			/* Install attribute in the reference table */` |
|     6428 |   932 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   933 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   934 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   935 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6428 |   936 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|     3215 |   948 | `		}else{` |
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
|     2074 |   960 | `	return SXRET_OK;` |
|     1038 |   961 |  |
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
|   454862 |   973 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   974 |  |
|        - |   975 | `	ph7_value *pObj;` |
|        - |   976 | `	sxi32 rc;` |
|   454864 |   977 | `	if( pIndex ){` |
|        - |   978 | `		/* Object index in the object table */` |
|   445462 |   979 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   222730 |   980 | `	}` |
|        - |   981 | `	/* Reserve a slot for the new object */` |
|   454864 |   982 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   454864 |   983 | `	if( rc != SXRET_OK ){` |
|        - |   984 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   985 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   986 | `		 */` |
|      ! 0 |   987 | `		return 0;` |
|        - |   988 | `	}` |
|   454864 |   989 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   454864 |   990 | `	return pObj;` |
|   227433 |   991 |  |
|        - |   992 | `/*` |
|        - |   993 | ` * Reserve a memory object.` |
|        - |   994 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   995 | ` */` |
|  2151624 |   996 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   997 |  |
|        - |   998 | `	ph7_value *pObj;` |
|        - |   999 | `	sxi32 rc;` |
|  2151626 |  1000 | `	if( pIndex ){` |
|        - |  1001 | `		/* Object index in the object table */` |
|  2151626 |  1002 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075812 |  1003 | `	}` |
|        - |  1004 | `	/* Reserve a slot for the new object */` |
|  2151626 |  1005 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151626 |  1006 | `	if( rc != SXRET_OK ){` |
|        - |  1007 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1008 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1009 | `		 */` |
|      ! 0 |  1010 | `		return 0;` |
|        - |  1011 | `	}` |
|  2151626 |  1012 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151626 |  1013 | `	return pObj;` |
|  1075814 |  1014 |  |
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
|        - |  1204 | `	"interface Iterator extends Traversable {"\` |
|        - |  1205 | `	"public function current();"\` |
|        - |  1206 | `	"public function key();"\` |
|        - |  1207 | `	"public function next();"\` |
|        - |  1208 | `	"public function rewind();"\` |
|        - |  1209 | `	"public function valid();"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1212 | `	"public function getIterator();"\` |
|        - |  1213 | `	"}"\` |
|        - |  1214 | `	"interface Serializable {"\` |
|        - |  1215 | `	"public function serialize();"\` |
|        - |  1216 | `	"public function unserialize(string $serialized);"\` |
|        - |  1217 | `	"}"\` |
|        - |  1218 | `	"/* Directory releated IO */"\` |
|        - |  1219 | `	"class Directory {"\` |
|        - |  1220 | `	"public $handle = null;"\` |
|        - |  1221 | `	"public $path  = null;"\` |
|        - |  1222 | `	"public function __construct(string $path)"\` |
|        - |  1223 | `	"{"\` |
|        - |  1224 | `	"   $this->handle = opendir($path);"\` |
|        - |  1225 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1226 | `	"      $this->path = $path;"\` |
|        - |  1227 | `	"   }"\` |
|        - |  1228 | `	"}"\` |
|        - |  1229 | `	"public function __destruct()"\` |
|        - |  1230 | `	"{"\` |
|        - |  1231 | `	"  if( $this->handle != null ){"\` |
|        - |  1232 | `	"       closedir($this->handle);"\` |
|        - |  1233 | `	"  }"\` |
|        - |  1234 | `	"}"\` |
|        - |  1235 | `	"public function read()"\` |
|        - |  1236 | `	"{"\` |
|        - |  1237 | `	"    return readdir($this->handle);"\` |
|        - |  1238 | `	"}"\` |
|        - |  1239 | `	"public function rewind()"\` |
|        - |  1240 | `	"{"\` |
|        - |  1241 | `	"    rewinddir($this->handle);"\` |
|        - |  1242 | `	"}"\` |
|        - |  1243 | `	"public function close()"\` |
|        - |  1244 | `	"{"\` |
|        - |  1245 | `	"    closedir($this->handle);"\` |
|        - |  1246 | `	"    $this->handle = null;"\` |
|        - |  1247 | `	"}"\` |
|        - |  1248 | `	"}"\` |
|        - |  1249 | `	"class Fiber {"\` |
|        - |  1250 | `	"  private $__ctx;"\` |
|        - |  1251 | `	"  private $__callable;"\` |
|        - |  1252 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1253 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1254 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1255 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1256 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1257 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1258 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1259 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1260 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1261 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1262 | `	"}"\` |
|        - |  1263 | `	"class Generator implements Iterator {"\` |
|        - |  1264 | `	"  private $__ctx;"\` |
|        - |  1265 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1266 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1267 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1268 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1269 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1270 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1271 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1272 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1273 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1274 | `	"}"\` |
|        - |  1275 | `	"class stdClass{"\` |
|        - |  1276 | `	"  public $value;"\` |
|        - |  1277 | `	" /* Magic methods */"\` |
|        - |  1278 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1279 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1280 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1281 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1282 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1283 | `	"}"\` |
|        - |  1284 | `	"function dir(string $path){"\` |
|        - |  1285 | `	"   return new Directory($path);"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"function Dir(string $path){"\` |
|        - |  1288 | `	"   return new Directory($path);"\` |
|        - |  1289 | `	"}"\` |
|        - |  1290 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1291 | `    "{"\` |
|        - |  1292 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1293 | `	"  $aDir = array();"\` |
|        - |  1294 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1295 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1296 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1297 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1298 | `	"   }"\` |
|        - |  1299 | `	"  closedir($pHandle);"\` |
|        - |  1300 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1301 | `	"      rsort($aDir);"\` |
|        - |  1302 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1303 | `	"      sort($aDir);"\` |
|        - |  1304 | `	"  }"\` |
|        - |  1305 | `	"  return $aDir;"\` |
|        - |  1306 | `	"}"\` |
|        - |  1307 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1308 | `	"/* Open the target directory */"\` |
|        - |  1309 | `	"$zDir = dirname($pattern);"\` |
|        - |  1310 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1311 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1312 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1313 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1314 | `	"	return FALSE;"\` |
|        - |  1315 | `	"}"\` |
|        - |  1316 | `	"$pattern = basename($pattern);"\` |
|        - |  1317 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1318 | `	"/* Loop throw available entries */"\` |
|        - |  1319 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1320 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1321 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1322 | `	"	if( $rc ){"\` |
|        - |  1323 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1324 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1325 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1326 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1327 | `	"		  }"\` |
|        - |  1328 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1329 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1330 | `	"		 continue;"\` |
|        - |  1331 | `	"	   }"\` |
|        - |  1332 | `	"	   /* Add the entry */"\` |
|        - |  1333 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1334 | `	"	}"\` |
|        - |  1335 | `	" }"\` |
|        - |  1336 | `	"/* Close the handle */"\` |
|        - |  1337 | `	"closedir($pHandle);"\` |
|        - |  1338 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1339 | `	"  /* Sort the array */"\` |
|        - |  1340 | `	"  sort($pArray);"\` |
|        - |  1341 | `	"}"\` |
|        - |  1342 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1343 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1344 | `	"  $pArray[] = $pattern;"\` |
|        - |  1345 | `	"}"\` |
|        - |  1346 | `	"/* Return the created array */"\` |
|        - |  1347 | `	"return $pArray;"\` |
|        - |  1348 | `   "}"\` |
|        - |  1349 | `   "/* Creates a temporary file */"\` |
|        - |  1350 | `   "function tmpfile(){"\` |
|        - |  1351 | `   "  /* Extract the temp directory */"\` |
|        - |  1352 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1353 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1354 | `   "    /* Use the current dir */"\` |
|        - |  1355 | `   "    $zTempDir = '.';"\` |
|        - |  1356 | `   "  }"\` |
|        - |  1357 | `   "  /* Create the file */"\` |
|        - |  1358 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1359 | `   "  return $pHandle;"\` |
|        - |  1360 | `   "}"\` |
|        - |  1361 | `   "/* Creates a temporary filename */"\` |
|        - |  1362 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1363 | `   "{"\` |
|        - |  1364 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1365 | `   "}"\` |
|        - |  1366 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1367 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1368 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1369 | `   "/* Copy arguments */"\` |
|        - |  1370 | `   "$nArgs = func_num_args();"\` |
|        - |  1371 | `   "$pNew = array();"\` |
|        - |  1372 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1373 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1374 | `    "}"\` |
|        - |  1375 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1376 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1377 | `	"/* Erase */"\` |
|        - |  1378 | `	"array_erase($pArray);"\` |
|        - |  1379 | `	"/* Unshift */"\` |
|        - |  1380 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1381 | `	"return sizeof($pArray);"\` |
|        - |  1382 | `    "}"\` |
|        - |  1383 | `	"function array_merge_recursive(){"\` |
|        - |  1384 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1385 | `    "$arrays = func_get_args();"\` |
|        - |  1386 | `    "$narrays = count($arrays);"\` |
|        - |  1387 | `    "$ret = array();"\` |
|        - |  1388 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1389 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1390 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1391 | `	 " }"\` |
|        - |  1392 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1393 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1394 | `     "  if( $keyIsInt ) {"\` |
|        - |  1395 | `     "   $ret[] = $value;"\` |
|        - |  1396 | `     "  } else {"\` |
|        - |  1397 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1398 | `     "    $cur = $ret[$key];"\` |
|        - |  1399 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1400 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1401 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1402 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1403 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1404 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1405 | `     "    } else {"\` |
|        - |  1406 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1407 | `     "    }"\` |
|        - |  1408 | `     "   } else {"\` |
|        - |  1409 | `     "    $ret[$key] = $value;"\` |
|        - |  1410 | `     "   }"\` |
|        - |  1411 | `     "  }"\` |
|        - |  1412 | `     " }"\` |
|        - |  1413 | `	 " }"\` |
|        - |  1414 | `	 " return $ret;"\` |
|        - |  1415 | `    "}"\` |
|        - |  1416 | `	"function max(){"\` |
|        - |  1417 | `    "  $pArgs = func_get_args();"\` |
|        - |  1418 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1419 | `	"  return null;"\` |
|        - |  1420 | `    " }"\` |
|        - |  1421 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1422 | `    " $pArg = $pArgs[0];"\` |
|        - |  1423 | `	" if( !is_array($pArg) ){"\` |
|        - |  1424 | `	"   return $pArg; "\` |
|        - |  1425 | `	" }"\` |
|        - |  1426 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1427 | `	"   return null;"\` |
|        - |  1428 | `	" }"\` |
|        - |  1429 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1430 | `	" reset($pArg);"\` |
|        - |  1431 | `	" $max = current($pArg);"\` |
|        - |  1432 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1433 | `	"   if( $val > $max ){"\` |
|        - |  1434 | `	"     $max = $val;"\` |
|        - |  1435 | `    " }"\` |
|        - |  1436 | `	" }"\` |
|        - |  1437 | `	" return $max;"\` |
|        - |  1438 | `    " }"\` |
|        - |  1439 | `    " $max = $pArgs[0];"\` |
|        - |  1440 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1441 | `    " $val = $pArgs[$i];"\` |
|        - |  1442 | `	"if( $val > $max ){"\` |
|        - |  1443 | `	" $max = $val;"\` |
|        - |  1444 | `	"}"\` |
|        - |  1445 | `    " }"\` |
|        - |  1446 | `	" return $max;"\` |
|        - |  1447 | `    "}"\` |
|        - |  1448 | `	"function min(){"\` |
|        - |  1449 | `    "  $pArgs = func_get_args();"\` |
|        - |  1450 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1451 | `	"  return null;"\` |
|        - |  1452 | `    " }"\` |
|        - |  1453 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1454 | `    " $pArg = $pArgs[0];"\` |
|        - |  1455 | `	" if( !is_array($pArg) ){"\` |
|        - |  1456 | `	"   return $pArg; "\` |
|        - |  1457 | `	" }"\` |
|        - |  1458 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1459 | `	"   return null;"\` |
|        - |  1460 | `	" }"\` |
|        - |  1461 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1462 | `	" reset($pArg);"\` |
|        - |  1463 | `	" $min = current($pArg);"\` |
|        - |  1464 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1465 | `	"   if( $val < $min ){"\` |
|        - |  1466 | `	"     $min = $val;"\` |
|        - |  1467 | `    " }"\` |
|        - |  1468 | `	" }"\` |
|        - |  1469 | `	" return $min;"\` |
|        - |  1470 | `    " }"\` |
|        - |  1471 | `    " $min = $pArgs[0];"\` |
|        - |  1472 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1473 | `    " $val = $pArgs[$i];"\` |
|        - |  1474 | `	"if( $val < $min ){"\` |
|        - |  1475 | `	" $min = $val;"\` |
|        - |  1476 | `	" }"\` |
|        - |  1477 | `    " }"\` |
|        - |  1478 | `	" return $min;"\` |
|        - |  1479 | `	"}"\` |
|        - |  1480 | `	"function fileowner(string $file){"\` |
|        - |  1481 | `    " $a = stat($file);"\` |
|        - |  1482 | `	" if( !is_array($a) ){"\` |
|        - |  1483 | `	"	return false;"\` |
|        - |  1484 | `	" }"\` |
|        - |  1485 | `	" return $a['uid'];"\` |
|        - |  1486 | `    "}"\` |
|        - |  1487 | `    "function filegroup(string $file){"\` |
|        - |  1488 | `	" $a = stat($file);"\` |
|        - |  1489 | `	" if( !is_array($a) ){"\` |
|        - |  1490 | `	"	return false;"\` |
|        - |  1491 | `	" }"\` |
|        - |  1492 | `	" return $a['gid'];"\` |
|        - |  1493 | `    "}"\` |
|        - |  1494 | `	 "function fileinode(string $file){"\` |
|        - |  1495 | `	" $a = stat($file);"\` |
|        - |  1496 | `	" if( !is_array($a) ){"\` |
|        - |  1497 | `	"	return false;"\` |
|        - |  1498 | `	" }"\` |
|        - |  1499 | `	" return $a['ino'];"\` |
|        - |  1500 | `    "}"` |
|        - |  1501 |  |
|        - |  1502 | `/*` |
|        - |  1503 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1504 | ` * start compiling the target PHP program.` |
|        - |  1505 | ` */` |
|     3134 |  1506 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1507 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1508 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1509 | `	 )` |
|        2 |  1510 |  |
|        - |  1511 | `	SyString sBuiltin;` |
|        - |  1512 | `	ph7_value *pObj;` |
|        - |  1513 | `	sxi32 rc;` |
|        - |  1514 | `	/* Zero the structure */` |
|     3136 |  1515 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1516 | `	/* Initialize VM fields */` |
|     3136 |  1517 | `	pVm->pEngine = &(*pEngine);` |
|     3136 |  1518 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1519 | `	/* Instructions containers */` |
|     3136 |  1520 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3136 |  1521 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3136 |  1522 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1523 | `	/* Object containers */` |
|     3136 |  1524 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3136 |  1525 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1526 | `	/* Virtual machine internal containers */` |
|     3136 |  1527 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3136 |  1528 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3136 |  1529 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3136 |  1530 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3136 |  1531 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3136 |  1532 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3136 |  1533 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3136 |  1534 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3136 |  1535 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3136 |  1536 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3136 |  1537 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3136 |  1538 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3136 |  1539 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3136 |  1540 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3136 |  1541 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3136 |  1542 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3136 |  1543 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3136 |  1544 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3136 |  1545 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3136 |  1546 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3136 |  1547 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3136 |  1548 | `	pVm->pPendingException = 0;` |
|        - |  1549 | `	/* Configuration containers */` |
|     3136 |  1550 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1551 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1552 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3136 |  1553 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3136 |  1554 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3136 |  1555 | `	pVm->iResponseStatus = 200;` |
|     3136 |  1556 | `	pVm->bHeadersSent = 0;` |
|     3136 |  1557 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1558 | `	/* Error callbacks containers */` |
|     3136 |  1559 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3136 |  1560 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3136 |  1561 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3136 |  1562 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3136 |  1563 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1564 | `	/* Set a default recursion limit */` |
|        - |  1565 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3136 |  1566 | `	pVm->nMaxDepth = 32;` |
|        - |  1567 | `#else` |
|        - |  1568 | `	pVm->nMaxDepth = 16;` |
|        - |  1569 | `#endif` |
|        - |  1570 | `	/* Default assertion flags */` |
|     3136 |  1571 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1572 | `	/* JSON return status */` |
|     3136 |  1573 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1574 | `	/* PRNG context */` |
|     3136 |  1575 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1576 | `	/* Install the null constant */` |
|     3136 |  1577 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1578 | `	if( pObj == 0 ){` |
|      ! 0 |  1579 | `		rc = SXERR_MEM;` |
|      ! 0 |  1580 | `		goto Err;` |
|        - |  1581 | `	}` |
|     3136 |  1582 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1583 | `	/* Install the boolean TRUE constant */` |
|     3136 |  1584 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1585 | `	if( pObj == 0 ){` |
|      ! 0 |  1586 | `		rc = SXERR_MEM;` |
|      ! 0 |  1587 | `		goto Err;` |
|        - |  1588 | `	}` |
|     3136 |  1589 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1590 | `	/* Install the boolean FALSE constant */` |
|     3136 |  1591 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3136 |  1592 | `	if( pObj == 0 ){` |
|      ! 0 |  1593 | `		rc = SXERR_MEM;` |
|      ! 0 |  1594 | `		goto Err;` |
|        - |  1595 | `	}` |
|     3136 |  1596 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1597 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1598 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1599 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3136 |  1600 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3136 |  1601 | `	if( pObj == 0 ){` |
|      ! 0 |  1602 | `		rc = SXERR_MEM;` |
|      ! 0 |  1603 | `		goto Err;` |
|        - |  1604 | `	}` |
|     3136 |  1605 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1606 | `	/* Create the global frame */` |
|     3136 |  1607 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3136 |  1608 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1609 | `		goto Err;` |
|        - |  1610 | `	}` |
|        - |  1611 | `	/* Initialize the code generator */` |
|     3136 |  1612 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3136 |  1613 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1614 | `		goto Err;` |
|        - |  1615 | `	}` |
|        - |  1616 | `	/* VM correctly initialized,set the magic number */` |
|     3136 |  1617 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3136 |  1618 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1619 | `	/* Compile the built-in library */` |
|     3136 |  1620 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1621 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3136 |  1622 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1623 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3136 |  1624 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3136 |  1625 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3136 |  1626 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3136 |  1627 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|        - |  1628 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3136 |  1629 | `	pVm->pCoalesceObj = 0;` |
|     3136 |  1630 | `	pVm->bCoalesceArmed = 0;` |
|     3136 |  1631 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1632 | `	/* Register Fiber internal C functions */` |
|     3136 |  1633 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3136 |  1634 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3136 |  1635 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3136 |  1636 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3136 |  1637 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3136 |  1638 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3136 |  1639 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3136 |  1640 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3136 |  1641 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3136 |  1642 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1643 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3136 |  1644 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3136 |  1645 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3136 |  1646 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3136 |  1647 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3136 |  1648 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3136 |  1649 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3136 |  1650 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3136 |  1651 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3136 |  1652 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3136 |  1653 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1654 | `	/* Reset the code generator */` |
|     3136 |  1655 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3136 |  1656 | `	return SXRET_OK;` |
|      ! 0 |  1657 | `Err:` |
|      ! 0 |  1658 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1659 | `	return rc;` |
|     1569 |  1660 |  |
|        - |  1661 | `/*` |
|        - |  1662 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1663 | ` * routine which store the output in an internal blob.` |
|        - |  1664 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1665 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1666 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1667 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1668 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1669 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1670 | ` * to finish executing and extracting the output.` |
|        - |  1671 | ` */` |
|       38 |  1672 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1673 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1674 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1675 | `	void *pUserData     /* User private data */` |
|        - |  1676 | `	)` |
|      ! 0 |  1677 |  |
|        - |  1678 | `	 sxi32 rc;` |
|        - |  1679 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1680 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1681 | `	 return rc;` |
|      ! 0 |  1682 |  |
|        - |  1683 | `/*` |
|        - |  1684 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1685 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1686 | ` */` |
|    20214 |  1687 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1688 |  |
|    20216 |  1689 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20216 |  1690 | `	if( xCons != VmObConsumer ){` |
|     8198 |  1691 | `		pVm->nOutputLen += nLen;` |
|     8198 |  1692 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1022 |  1693 | `			pVm->bHeadersSent = 1;` |
|      510 |  1694 | `		}` |
|     4098 |  1695 | `	}` |
|    20216 |  1696 |  |
|        - |  1697 | `#define VM_STACK_GUARD 16` |
|        - |  1698 | `/*` |
|        - |  1699 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1700 | ` * our compiled PHP program.` |
|        - |  1701 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1702 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1703 | ` */` |
|    44736 |  1704 | `static ph7_value * VmNewOperandStack(` |
|        - |  1705 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1706 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1707 | `	)` |
|        2 |  1708 |  |
|        - |  1709 | `	ph7_value *pStack;` |
|        - |  1710 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1711 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1712 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1713 | `  ** on the maximum stack depth required.` |
|        - |  1714 | `  **` |
|        - |  1715 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1716 | `  */` |
|    44738 |  1717 | `	nInstr += VM_STACK_GUARD;` |
|    44738 |  1718 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    44738 |  1719 | `	if( pStack == 0 ){` |
|      ! 0 |  1720 | `		return 0;` |
|        - |  1721 | `	}` |
|        - |  1722 | `	/* Initialize the operand stack */` |
|  3024538 |  1723 | `	while( nInstr > 0 ){` |
|  2979802 |  1724 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2979802 |  1725 | `		--nInstr;` |
|        2 |  1726 | `	}` |
|        - |  1727 | `	/* Ready for bytecode execution */` |
|    44738 |  1728 | `	return pStack;` |
|    22370 |  1729 |  |
|        - |  1730 | `/* Forward declaration */` |
|        - |  1731 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1732 | `/*` |
|        - |  1733 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1734 | ` * This routine gets called by the PH7 engine after` |
|        - |  1735 | ` * successful compilation of the target PHP program.` |
|        - |  1736 | ` */` |
|     2820 |  1737 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1738 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1739 | `	)` |
|        2 |  1740 |  |
|        - |  1741 | `	SyHashEntry *pEntry;` |
|        - |  1742 | `	sxi32 rc;` |
|     2822 |  1743 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1744 | `		/* Initialize your VM first */` |
|      ! 0 |  1745 | `		return SXERR_CORRUPT;` |
|        - |  1746 | `	}` |
|        - |  1747 | `	/* Mark the VM ready for byte-code execution */` |
|     2822 |  1748 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1749 | `	/* Release the code generator now we have compiled our program */` |
|     2822 |  1750 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1751 | `	/* Emit the DONE instruction */` |
|     2822 |  1752 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2822 |  1753 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1754 | `		return SXERR_MEM;` |
|        - |  1755 | `	}` |
|        - |  1756 | `	/* Script return value */` |
|     2822 |  1757 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1758 | `	/* Allocate a new operand stack */` |
|     2822 |  1759 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2822 |  1760 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1761 | `		return SXERR_MEM;` |
|        - |  1762 | `	}` |
|        - |  1763 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1764 | `	 * private data. */` |
|     2822 |  1765 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2822 |  1766 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1767 | `	/* Allocate the reference table */` |
|     2822 |  1768 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2822 |  1769 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2822 |  1770 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1771 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1772 | `		return SXERR_MEM;` |
|        - |  1773 | `	}` |
|        - |  1774 | `	/* Zero the reference table */` |
|     2822 |  1775 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1776 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2822 |  1777 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2822 |  1778 | `	if( rc != SXRET_OK ){` |
|        - |  1779 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1780 | `		return rc;` |
|        - |  1781 | `	}` |
|        - |  1782 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2822 |  1783 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2822 |  1784 | `	if( rc != SXRET_OK ){` |
|        - |  1785 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1786 | `		return rc;` |
|        - |  1787 | `	}` |
|        - |  1788 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2822 |  1789 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1790 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2822 |  1791 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1792 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2822 |  1793 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1794 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1795 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2822 |  1796 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2822 |  1797 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1798 | `#endif` |
|        - |  1799 | `	/* Initialize and install static and constants class attributes */` |
|     2822 |  1800 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    73666 |  1801 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    70846 |  1802 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    70846 |  1803 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1804 | `			return rc;` |
|        - |  1805 | `		}` |
|        2 |  1806 | `	}` |
|        - |  1807 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2822 |  1808 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1809 | `	/* VM is ready for bytecode execution */` |
|     2822 |  1810 | `	return SXRET_OK;` |
|     1412 |  1811 |  |
|        - |  1812 | `/*` |
|        - |  1813 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1814 | ` */` |
|      ! 0 |  1815 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1816 |  |
|      ! 0 |  1817 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1818 | `		return SXERR_CORRUPT;` |
|        - |  1819 | `	}` |
|        - |  1820 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1821 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1822 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1823 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1824 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1825 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1826 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1827 | `	pVm->bHttpContext = 0;` |
|        - |  1828 | `	/* Set the ready flag */` |
|      ! 0 |  1829 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1830 | `	return SXRET_OK;` |
|      ! 0 |  1831 |  |
|        - |  1832 | `/*` |
|        - |  1833 | ` * Release a Virtual Machine.` |
|        - |  1834 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1835 | ` */` |
|     2820 |  1836 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1837 |  |
|        - |  1838 | `	/* Set the stale magic number */` |
|     2822 |  1839 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1840 | `	/* Release the private memory subsystem */` |
|     2822 |  1841 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2822 |  1842 | `	return SXRET_OK;` |
|        2 |  1843 |  |
|        - |  1844 | `/*` |
|        - |  1845 | ` * Initialize a foreign function call context.` |
|        - |  1846 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1847 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1848 | ` * functions.` |
|        - |  1849 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1850 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1851 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1852 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1853 | ` */` |
|   695242 |  1854 | `static sxi32 VmInitCallContext(` |
|        - |  1855 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1856 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1857 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1858 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1859 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1860 | `	)` |
|        2 |  1861 |  |
|   695244 |  1862 | `	pOut->pFunc = pFunc;` |
|   695244 |  1863 | `	pOut->pVm   = pVm;` |
|   695244 |  1864 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   695244 |  1865 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1866 | `	/* Assume a null return value */` |
|   695244 |  1867 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   695244 |  1868 | `	pOut->pRet = pRet;` |
|   695244 |  1869 | `	pOut->iFlags = iFlags;` |
|   695244 |  1870 | `	return SXRET_OK;` |
|        2 |  1871 |  |
|        - |  1872 | `/*` |
|        - |  1873 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1874 | ` * left behind.` |
|        - |  1875 | ` */` |
|   695242 |  1876 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1877 |  |
|        - |  1878 | `	sxu32 n;` |
|   695244 |  1879 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8594 |  1880 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25076 |  1881 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16484 |  1882 | `			if( apObj[n] == 0 ){` |
|        - |  1883 | `				/* Already released */` |
|      384 |  1884 | `				continue;` |
|        - |  1885 | `			}` |
|    16102 |  1886 | `			PH7_MemObjRelease(apObj[n]);` |
|    16102 |  1887 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8052 |  1888 | `		}` |
|     8594 |  1889 | `		SySetRelease(&pCtx->sVar);` |
|     4296 |  1890 | `	}` |
|   695244 |  1891 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1892 | `		ph7_aux_data *aAux;` |
|        - |  1893 | `		void *pChunk;` |
|        - |  1894 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1895 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1896 | `		 */` |
|        9 |  1897 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1898 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1899 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1900 | `			/* Release the chunk */` |
|       25 |  1901 | `			if( pChunk ){` |
|       25 |  1902 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1903 | `			}` |
|       13 |  1904 | `		}` |
|        9 |  1905 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1906 | `	}` |
|   695244 |  1907 |  |
|        - |  1908 | `/*` |
|        - |  1909 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1910 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1911 | ` */` |
|      382 |  1912 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1913 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1914 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1915 | `	)` |
|        2 |  1916 |  |
|      384 |  1917 | `	if( pValue == 0 ){` |
|        - |  1918 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1919 | `		return;` |
|        - |  1920 | `	}` |
|      384 |  1921 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      384 |  1922 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1923 | `		sxu32 n;` |
|     1282 |  1924 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1282 |  1925 | `			if( apObj[n] == pValue ){` |
|      384 |  1926 | `				PH7_MemObjRelease(pValue);` |
|      384 |  1927 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1928 | `				/* Mark as released */` |
|      384 |  1929 | `				apObj[n] = 0;` |
|      384 |  1930 | `				break;` |
|        - |  1931 | `			}` |
|      451 |  1932 | `		}` |
|      191 |  1933 | `	}` |
|      193 |  1934 |  |
|        - |  1935 | `/*` |
|        - |  1936 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1937 | ` */` |
|  3942592 |  1938 | `static void VmPopOperand(` |
|        - |  1939 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1940 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1941 | `	)` |
|        2 |  1942 |  |
|  3942594 |  1943 | `	ph7_value *pTos = *ppTos;` |
|  8399470 |  1944 | `	while( nPop > 0 ){` |
|  4456878 |  1945 | `		PH7_MemObjRelease(pTos);` |
|  4456878 |  1946 | `		pTos--;` |
|  4456878 |  1947 | `		nPop--;` |
|        2 |  1948 | `	}` |
|        - |  1949 | `	/* Top of the stack */` |
|  3942594 |  1950 | `	*ppTos = pTos;` |
|  3942594 |  1951 |  |
|        - |  1952 | `/*` |
|        - |  1953 | ` * Reserve a memory object.` |
|        - |  1954 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1955 | ` */` |
|  3203670 |  1956 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1957 |  |
|  3203672 |  1958 | `	ph7_value *pObj = 0;` |
|        - |  1959 | `	VmSlot *pSlot;` |
|        - |  1960 | `	sxu32 nIdx;` |
|        - |  1961 | `	/* Check for a free slot */` |
|  3203672 |  1962 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3203672 |  1963 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3203672 |  1964 | `	if( pSlot ){` |
|  1052048 |  1965 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1052048 |  1966 | `		nIdx = pSlot->nIdx;` |
|   526023 |  1967 | `	}` |
|  3203672 |  1968 | `	if( pObj == 0 ){` |
|        - |  1969 | `		/* Reserve a new memory object */` |
|  2151626 |  1970 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151626 |  1971 | `		if( pObj == 0 ){` |
|      ! 0 |  1972 | `			return 0;` |
|        - |  1973 | `		}` |
|  1075812 |  1974 | `	}` |
|        - |  1975 | `	/* Set a null default value */` |
|  3203672 |  1976 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3203672 |  1977 | `	pObj->nIdx = nIdx;` |
|  3203672 |  1978 | `	return pObj;` |
|  1601837 |  1979 |  |
|        - |  1980 | `/*` |
|        - |  1981 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1982 | ` */` |
|    35188 |  1983 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1984 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1985 | `	const char *zKey,  /* Entry key */` |
|        - |  1986 | `	sxu32 nByte,       /* Key length */` |
|        - |  1987 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1988 | `	)` |
|        2 |  1989 |  |
|        - |  1990 | `	ph7_value sKey;` |
|        - |  1991 | `	sxi32 rc;` |
|    35190 |  1992 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35190 |  1993 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1994 | `	/* Perform the insertion */` |
|    35190 |  1995 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35190 |  1996 | `	PH7_MemObjRelease(&sKey);` |
|    35190 |  1997 | `	return rc;` |
|        2 |  1998 |  |
|        - |  1999 | `/*` |
|        - |  2000 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2001 | ` * Return a pointer to the variable value on success.` |
|        - |  2002 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2003 | ` */` |
|  3664726 |  2004 | `static ph7_value * VmExtractMemObj(` |
|        - |  2005 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2006 | `	const SyString *pName, /* Variable name */` |
|        - |  2007 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2008 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2009 | `	)` |
|        2 |  2010 |  |
|  3664728 |  2011 | `	int bNullify = FALSE;` |
|        - |  2012 | `	SyHashEntry *pEntry;` |
|        - |  2013 | `	VmFrame *pFrame;` |
|        - |  2014 | `	ph7_value *pObj;` |
|        - |  2015 | `	sxu32 nIdx;` |
|        - |  2016 | `	sxi32 rc;` |
|        - |  2017 | `	/* Point to the top active frame */` |
|  3664728 |  2018 | `	pFrame = pVm->pFrame;` |
|  3664728 |  2019 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2020 | `	/* Perform the lookup */` |
|  3664728 |  2021 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2022 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2023 | `		pName = &sAnnon;` |
|        - |  2024 | `		/* Always nullify the object */` |
|      ! 0 |  2025 | `		bNullify = TRUE;` |
|      ! 0 |  2026 | `		bDup = FALSE;` |
|      ! 0 |  2027 | `	}` |
|        - |  2028 | `	/* Check the superglobals table first */` |
|  3664728 |  2029 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3664728 |  2030 | `	if( pEntry == 0 ){` |
|        - |  2031 | `		/* Query the top active frame */` |
|  3664688 |  2032 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3664688 |  2033 | `		if( pEntry == 0 ){` |
|   113018 |  2034 | `			char *zName = (char *)pName->zString;` |
|        - |  2035 | `			VmSlot sLocal;` |
|   113018 |  2036 | `			if( !bCreate ){` |
|        - |  2037 | `				/* Do not create the variable,return NULL instead */` |
|      958 |  2038 | `				return 0;` |
|        - |  2039 | `			}` |
|        - |  2040 | `			/* No such variable,automatically create a new one and install` |
|        - |  2041 | `			 * it in the current frame.` |
|        - |  2042 | `			 */` |
|   112062 |  2043 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   112062 |  2044 | `			if( pObj == 0 ){` |
|      ! 0 |  2045 | `				return 0;` |
|        - |  2046 | `			}` |
|   112062 |  2047 | `			nIdx = pObj->nIdx;` |
|   112062 |  2048 | `			if( bDup ){` |
|        - |  2049 | `				/* Duplicate name */` |
|      230 |  2050 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      230 |  2051 | `				if( zName == 0 ){` |
|      ! 0 |  2052 | `					return 0;` |
|        - |  2053 | `				}` |
|      114 |  2054 | `			}` |
|        - |  2055 | `			/* Link to the top active VM frame */` |
|   112062 |  2056 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   112062 |  2057 | `			if( rc != SXRET_OK ){` |
|        - |  2058 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2059 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2060 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2061 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2062 | `				return 0;` |
|        - |  2063 | `			}` |
|   112062 |  2064 | `			if( pFrame->pParent != 0 ){` |
|        - |  2065 | `				/* Local variable */` |
|   105102 |  2066 | `				sLocal.nIdx = nIdx;` |
|   105102 |  2067 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    52552 |  2068 | `			}else{` |
|        - |  2069 | `				/* Register in the $GLOBALS array */` |
|     6962 |  2070 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2071 | `			}` |
|        - |  2072 | `			/* Install in the reference table */` |
|   112062 |  2073 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2074 | `			/* Save object index */` |
|   112062 |  2075 | `			pObj->nIdx = nIdx;` |
|    56032 |  2076 | `		}else{` |
|        - |  2077 | `			/* Extract variable contents */` |
|  3551672 |  2078 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3551672 |  2079 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3551672 |  2080 | `			if( bNullify && pObj ){` |
|      ! 0 |  2081 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2082 | `			}` |
|        - |  2083 | `		}` |
|  1831977 |  2084 | `	}else{` |
|        - |  2085 | `		/* Superglobal */` |
|       42 |  2086 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2087 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2088 | `	}` |
|  3663772 |  2089 | `	return pObj;` |
|  1832475 |  2090 |  |
|        - |  2091 | `/*` |
|        - |  2092 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2093 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2094 | ` */` |
|     3124 |  2095 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2096 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2097 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2098 | `	sxu32 nByte        /* zName length */` |
|        - |  2099 | `	)` |
|        2 |  2100 |  |
|        - |  2101 | `	SyHashEntry *pEntry;` |
|        - |  2102 | `	ph7_value *pValue;` |
|        - |  2103 | `	sxu32 nIdx;` |
|        - |  2104 | `	/* Query the superglobal table */` |
|     3126 |  2105 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3126 |  2106 | `	if( pEntry == 0 ){` |
|        - |  2107 | `		/* No such entry */` |
|      ! 0 |  2108 | `		return 0;` |
|        - |  2109 | `	}` |
|        - |  2110 | `	/* Extract the superglobal index in the global object pool */` |
|     3126 |  2111 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2112 | `	/* Extract the variable value  */` |
|     3126 |  2113 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3126 |  2114 | `	return pValue;` |
|     1564 |  2115 |  |
|        - |  2116 | `/*` |
|        - |  2117 | ` * Perform a raw hashmap insertion.` |
|        - |  2118 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2119 | ` */` |
|     3154 |  2120 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2121 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2122 | `	const char *zKey,   /* Entry key */` |
|        - |  2123 | `	int nKeylen,        /* zKey length*/` |
|        - |  2124 | `	const char *zData,  /* Entry data */` |
|        - |  2125 | `	int nLen            /* zData length */` |
|        - |  2126 | `	)` |
|        2 |  2127 |  |
|        - |  2128 | `	ph7_value sKey,sValue;` |
|        - |  2129 | `	sxi32 rc;` |
|     3156 |  2130 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3156 |  2131 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3156 |  2132 | `	if( zKey ){` |
|     3134 |  2133 | `		if( nKeylen < 0 ){` |
|     3082 |  2134 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1540 |  2135 | `		}` |
|     3134 |  2136 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1566 |  2137 | `	}` |
|     3156 |  2138 | `	if( zData ){` |
|     3156 |  2139 | `		if( nLen < 0 ){` |
|        - |  2140 | `			/* Compute length automatically */` |
|      144 |  2141 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2142 | `		}` |
|     3156 |  2143 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1577 |  2144 | `	}` |
|        - |  2145 | `	/* Perform the insertion */` |
|     3156 |  2146 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3156 |  2147 | `	PH7_MemObjRelease(&sKey);` |
|     3156 |  2148 | `	PH7_MemObjRelease(&sValue);` |
|     3156 |  2149 | `	return rc;` |
|        2 |  2150 |  |
|        - |  2151 | `/*` |
|        - |  2152 | ` * Configure a working virtual machine instance.` |
|        - |  2153 | ` *` |
|        - |  2154 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2155 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2156 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2157 | ` * The second argument to this function is an integer configuration option` |
|        - |  2158 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2159 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2160 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2161 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2162 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2163 | ` */` |
|    45450 |  2164 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2165 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2166 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2167 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2168 | `	)` |
|        2 |  2169 |  |
|    45452 |  2170 | `	sxi32 rc = SXRET_OK;` |
|    45452 |  2171 | `	switch(nOp){` |
|     1402 |  2172 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2806 |  2173 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2806 |  2174 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2175 | `		/* VM output consumer callback */` |
|        - |  2176 | `#ifdef UNTRUST` |
|        - |  2177 | `		if( xConsumer == 0 ){` |
|        - |  2178 | `			rc = SXERR_CORRUPT;` |
|        - |  2179 | `			break;` |
|        - |  2180 | `		}` |
|        - |  2181 | `#endif` |
|        - |  2182 | `		/* Install the output consumer */` |
|     2806 |  2183 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2806 |  2184 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2806 |  2185 | `		break;` |
|        - |  2186 | `							   }` |
|     1410 |  2187 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2188 | `		/* Import path */` |
|        - |  2189 | `		  const char *zPath;` |
|        - |  2190 | `		  SyString sPath;` |
|     2822 |  2191 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2192 | `#if defined(UNTRUST)` |
|        - |  2193 | `		  if( zPath == 0 ){` |
|        - |  2194 | `			  rc = SXERR_EMPTY;` |
|        - |  2195 | `			  break;` |
|        - |  2196 | `		  }` |
|        - |  2197 | `#endif` |
|     2822 |  2198 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2199 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2200 | `#ifdef __WINNT__` |
|        2 |  2201 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2202 | `#endif` |
|     5642 |  2203 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2204 | `		  /* Remove leading and trailing white spaces */` |
|     2822 |  2205 | `		  SyStringFullTrim(&sPath);` |
|     2822 |  2206 | `		  if( sPath.nByte > 0 ){` |
|        - |  2207 | `			  /* Store the path in the corresponding conatiner */` |
|     2822 |  2208 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1410 |  2209 | `		  }` |
|     2822 |  2210 | `		  break;` |
|        - |  2211 | `									 }` |
|     1410 |  2212 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2213 | `		/* Run-Time Error report */` |
|     2822 |  2214 | `		pVm->bErrReport = 1;` |
|     2822 |  2215 | `		break;` |
|      ! 0 |  2216 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2217 | `		/* Recursion depth */` |
|      ! 0 |  2218 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2219 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2220 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2221 | `		}` |
|      ! 0 |  2222 | `		break;` |
|        - |  2223 | `									   }` |
|      ! 0 |  2224 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2225 | `		/* VM output length in bytes */` |
|      ! 0 |  2226 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2227 | `#ifdef UNTRUST` |
|        - |  2228 | `		if( pOut == 0 ){` |
|        - |  2229 | `			rc = SXERR_CORRUPT;` |
|        - |  2230 | `			break;` |
|        - |  2231 | `		}` |
|        - |  2232 | `#endif` |
|      ! 0 |  2233 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2234 | `		break;` |
|        - |  2235 | `							   }` |
|        - |  2236 |  |
|    14100 |  2237 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2238 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2239 | `		/* Create a new superglobal/global variable */` |
|    28202 |  2240 | `		const char *zName = va_arg(ap,const char *);` |
|    28202 |  2241 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2242 | `		SyHashEntry *pEntry;` |
|        - |  2243 | `		ph7_value *pObj;` |
|        - |  2244 | `		sxu32 nByte;` |
|        - |  2245 | `		sxu32 nIdx;` |
|        - |  2246 | `#ifdef UNTRUST` |
|        - |  2247 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2248 | `			rc = SXERR_CORRUPT;` |
|        - |  2249 | `			break;` |
|        - |  2250 | `		}` |
|        - |  2251 | `#endif` |
|    28202 |  2252 | `		nByte = SyStrlen(zName);` |
|    28202 |  2253 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2254 | `			/* Check if the superglobal is already installed */` |
|    28202 |  2255 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14102 |  2256 | `		}else{` |
|        - |  2257 | `			/* Query the top active VM frame */` |
|      ! 0 |  2258 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2259 | `		}` |
|    28202 |  2260 | `		if( pEntry ){` |
|        - |  2261 | `			/* Variable already installed */` |
|      ! 0 |  2262 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2263 | `			/* Extract contents */` |
|      ! 0 |  2264 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2265 | `			if( pObj ){` |
|        - |  2266 | `				/* Overwrite old contents */` |
|      ! 0 |  2267 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2268 | `			}` |
|      ! 0 |  2269 | `		}else{` |
|        - |  2270 | `			/* Install a new variable */` |
|    28202 |  2271 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28202 |  2272 | `			if( pObj == 0 ){` |
|      ! 0 |  2273 | `				rc = SXERR_MEM;` |
|      ! 0 |  2274 | `				break;` |
|        - |  2275 | `			}` |
|    28202 |  2276 | `			nIdx = pObj->nIdx;` |
|        - |  2277 | `			/* Copy value */` |
|    28202 |  2278 | `			PH7_MemObjStore(pValue,pObj);` |
|    28202 |  2279 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2280 | `				/* Install the superglobal */` |
|    28202 |  2281 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14102 |  2282 | `			}else{` |
|        - |  2283 | `				/* Install in the current frame */` |
|      ! 0 |  2284 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2285 | `			}` |
|    28202 |  2286 | `			if( rc == SXRET_OK ){` |
|        - |  2287 | `				SyHashEntry *pRef;` |
|    28202 |  2288 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28202 |  2289 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14102 |  2290 | `				}else{` |
|      ! 0 |  2291 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2292 | `				}` |
|        - |  2293 | `				/* Install in the reference table */` |
|    28202 |  2294 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28202 |  2295 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2296 | `					/* Register in the $GLOBALS array */` |
|    28202 |  2297 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14100 |  2298 | `				}` |
|    14100 |  2299 | `			}` |
|        - |  2300 | `		}` |
|    28202 |  2301 | `		break;` |
|        - |  2302 | `									}` |
|     1540 |  2303 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2304 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2305 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2306 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2307 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2308 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2309 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3082 |  2310 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3082 |  2311 | `		const char *zValue = va_arg(ap,const char *);` |
|     3082 |  2312 | `		int nLen = va_arg(ap,int);` |
|        - |  2313 | `		ph7_hashmap *pMap;` |
|        - |  2314 | `		ph7_value *pValue;` |
|     3082 |  2315 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2316 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2317 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3081 |  2318 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2319 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2320 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3080 |  2321 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2322 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2323 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3080 |  2324 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2325 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2326 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3080 |  2327 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2328 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2329 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3080 |  2330 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2331 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2332 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2333 | `		}else{` |
|        - |  2334 | `			/* Extract the $_SERVER superglobal */` |
|     3080 |  2335 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2336 | `		}` |
|     3082 |  2337 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2338 | `			/* No such entry */` |
|      ! 0 |  2339 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2340 | `			break;` |
|        - |  2341 | `		}` |
|        - |  2342 | `		/* Point to the hashmap */` |
|     3082 |  2343 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2344 | `		/* Perform the insertion */` |
|     3082 |  2345 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3082 |  2346 | `		break;` |
|        - |  2347 | `								   }` |
|       11 |  2348 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2349 | `		/* Script arguments */` |
|       24 |  2350 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2351 | `		ph7_hashmap *pMap;` |
|        - |  2352 | `		ph7_value *pValue;` |
|        - |  2353 | `		sxu32 n;` |
|       24 |  2354 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2355 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2356 | `			break;` |
|        - |  2357 | `		}` |
|        - |  2358 | `		/* Extract the $argv array */` |
|       24 |  2359 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2360 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2361 | `			/* No such entry */` |
|      ! 0 |  2362 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2363 | `			break;` |
|        - |  2364 | `		}` |
|        - |  2365 | `		/* Point to the hashmap */` |
|       24 |  2366 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2367 | `		/* Perform the insertion */` |
|       24 |  2368 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2369 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2370 | `		if( rc == SXRET_OK ){` |
|       24 |  2371 | `			if( pMap->nEntry > 1 ){` |
|        - |  2372 | `				/* Append space separator first */` |
|       18 |  2373 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2374 | `			}` |
|       24 |  2375 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2376 | `		}` |
|       24 |  2377 | `		break;` |
|        - |  2378 | `								  }` |
|      ! 0 |  2379 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2380 | `		/* error_log() consumer */` |
|      ! 0 |  2381 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2382 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2383 | `		break;` |
|        - |  2384 | `										}` |
|      ! 0 |  2385 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2386 | `		/* Script return value */` |
|      ! 0 |  2387 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2388 | `#ifdef UNTRUST` |
|        - |  2389 | `		if( ppValue == 0 ){` |
|        - |  2390 | `			rc = SXERR_CORRUPT;` |
|        - |  2391 | `			break;` |
|        - |  2392 | `		}` |
|        - |  2393 | `#endif` |
|      ! 0 |  2394 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2395 | `		break;` |
|        - |  2396 | `								   }` |
|     2820 |  2397 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2398 | `		/* Register an IO stream device */` |
|     5642 |  2399 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2400 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8460 |  2401 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5642 |  2402 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2403 | `				/* Invalid stream */` |
|      ! 0 |  2404 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2405 | `				break;` |
|        - |  2406 | `		}` |
|     5642 |  2407 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2408 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2822 |  2409 | `			pVm->pDefStream = pStream;` |
|     1410 |  2410 | `		}` |
|        - |  2411 | `		/* Insert in the appropriate container */` |
|     5642 |  2412 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5642 |  2413 | `		break;` |
|        - |  2414 | `								  }` |
|        8 |  2415 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2416 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2417 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2418 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2419 | `#ifdef UNTRUST` |
|        - |  2420 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2421 | `			rc = SXERR_CORRUPT;` |
|        - |  2422 | `			break;` |
|        - |  2423 | `		}` |
|        - |  2424 | `#endif` |
|       16 |  2425 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2426 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2427 | `		break;` |
|        - |  2428 | `									   }` |
|        8 |  2429 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2430 | `		/* Raw HTTP request*/` |
|       16 |  2431 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2432 | `		int nByte = va_arg(ap,int);` |
|       16 |  2433 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2434 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2435 | `			break;` |
|        - |  2436 | `		}` |
|       16 |  2437 | `		if( nByte < 0 ){` |
|        - |  2438 | `			/* Compute length automatically */` |
|      ! 0 |  2439 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2440 | `		}` |
|        - |  2441 | `		/* Process the request */` |
|       16 |  2442 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2443 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2444 | `		if( rc == SXRET_OK ){` |
|       16 |  2445 | `			pVm->bHttpContext = 1;` |
|        8 |  2446 | `		}` |
|       16 |  2447 | `		break;` |
|        - |  2448 | `									}` |
|        8 |  2449 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2450 | `		/* Extract HTTP response status code */` |
|       16 |  2451 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2452 | `		if( pStatus ){` |
|       16 |  2453 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2454 | `		}` |
|       16 |  2455 | `		break;` |
|        - |  2456 | `										}` |
|        8 |  2457 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2458 | `		/* Iterate response headers via callback */` |
|        - |  2459 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2460 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2461 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2462 | `		if( xCallback ){` |
|       16 |  2463 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2464 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2465 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2466 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2467 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2468 | `							   pUserData);` |
|       12 |  2469 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2470 | `					break;` |
|        - |  2471 | `				}` |
|        6 |  2472 | `			}` |
|        8 |  2473 | `		}` |
|       16 |  2474 | `		break;` |
|        - |  2475 | `										 }` |
|      ! 0 |  2476 | `	default:` |
|        - |  2477 | `		/* Unknown configuration option */` |
|      ! 0 |  2478 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2479 | `		break;` |
|        - |  2480 | `	}` |
|    45452 |  2481 | `	return rc;` |
|        2 |  2482 |  |
|        - |  2483 | `/* Forward declaration */` |
|        - |  2484 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2485 | `/*` |
|        - |  2486 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2487 | ` * format.` |
|        - |  2488 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2489 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2490 | ` * (STDOUT).` |
|        - |  2491 | ` */` |
|        2 |  2492 | `static sxi32 VmByteCodeDump(` |
|        - |  2493 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2494 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2495 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2496 | `	)` |
|        1 |  2497 |  |
|        - |  2498 | `	static const char zDump[] = {` |
|        - |  2499 | `		"====================================================\n"` |
|        - |  2500 | `		"PH7 VM Dump\n"` |
|        - |  2501 | `		"====================================================\n"` |
|        - |  2502 | `	};` |
|        - |  2503 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2504 | `	sxi32 rc = SXRET_OK;` |
|        - |  2505 | `	sxu32 n;` |
|        - |  2506 | `	/* Point to the PH7 instructions */` |
|        3 |  2507 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2508 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2509 | `	n = 0;` |
|        3 |  2510 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2511 | `	/* Dump instructions */` |
|        7 |  2512 | `	for(;;){` |
|       15 |  2513 | `		if( pInstr >= pEnd ){` |
|        - |  2514 | `			/* No more instructions */` |
|        3 |  2515 | `			break;` |
|        - |  2516 | `		}` |
|        - |  2517 | `		/* Format and call the consumer callback */` |
|       19 |  2518 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2519 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2520 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2521 | `		if( rc != SXRET_OK ){` |
|        - |  2522 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2523 | `			return rc;` |
|        - |  2524 | `		}` |
|       13 |  2525 | `		++n;` |
|       13 |  2526 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2527 | `	}` |
|        3 |  2528 | `	return rc;` |
|        2 |  2529 |  |
|        - |  2530 | `/* Forward declaration */` |
|        - |  2531 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2532 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2533 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2534 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2535 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2536 | `/*` |
|        - |  2537 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2538 | ` * consumer callback.` |
|        - |  2539 | ` */` |
|      600 |  2540 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2541 |  |
|      601 |  2542 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      601 |  2543 | `	sxi32 rc = SXRET_OK;` |
|        - |  2544 | `	/* Append a new line */` |
|        - |  2545 | `#ifdef __WINNT__` |
|        1 |  2546 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2547 | `#else` |
|      600 |  2548 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2549 | `#endif` |
|        - |  2550 | `	/* Invoke the output consumer callback */` |
|      601 |  2551 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      601 |  2552 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      601 |  2553 | `	return rc;` |
|        1 |  2554 |  |
|        - |  2555 | `/*` |
|        - |  2556 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2557 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2558 | ` * information.` |
|        - |  2559 | ` */` |
|      148 |  2560 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2561 |  |
|      150 |  2562 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2563 | `		ph7_value apArg[4];` |
|        - |  2564 | `		ph7_value *apArgPtr[4];` |
|        - |  2565 | `		ph7_value sResult;` |
|        - |  2566 | `		SyString sErr;` |
|        - |  2567 | `		/* Prepare arguments */` |
|       76 |  2568 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2569 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2570 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2571 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2572 | `		if( pFile ){` |
|       76 |  2573 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2574 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2575 | `		}else{` |
|      ! 0 |  2576 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2577 | `		}` |
|       76 |  2578 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2579 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2580 | `		/* Set up pointer array */` |
|       76 |  2581 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2582 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2583 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2584 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2585 | `		/* Call the handler */` |
|       76 |  2586 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2587 | `		/* Check return value */` |
|       76 |  2588 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2589 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2590 | `		}` |
|        - |  2591 | `		/* Release */` |
|       76 |  2592 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2593 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2594 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2595 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2596 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2597 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2598 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2599 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2600 | `	}` |
|        - |  2601 | `	/* No handler, always call error handler */` |
|       75 |  2602 | `	return TRUE;` |
|       76 |  2603 |  |
|      110 |  2604 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2605 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2606 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2607 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2608 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2609 | `	)` |
|        2 |  2610 |  |
|      112 |  2611 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2612 | `	SyString *pFile;` |
|        - |  2613 | `	char *zErr;` |
|      112 |  2614 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2615 | `	if( !pVm->bErrReport ){` |
|        - |  2616 | `		/* Don't bother reporting errors */` |
|        3 |  2617 | `		return SXRET_OK;` |
|        - |  2618 | `	}` |
|        - |  2619 | `	/* Reset the working buffer */` |
|      110 |  2620 | `	SyBlobReset(pWorker);` |
|        - |  2621 | `	/* Peek the processed file if available */` |
|      110 |  2622 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2623 | `	if( pFile ){` |
|        - |  2624 | `		/* Append file name */` |
|      110 |  2625 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2626 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2627 | `	}` |
|        - |  2628 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2629 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2630 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2631 | `	 * E_DEPRECATED). */` |
|      110 |  2632 | `	zErr = "Error:  ";` |
|      110 |  2633 | `	switch(iErr){` |
|       19 |  2634 | `	case PH7_CTX_WARNING:` |
|       40 |  2635 | `		zErr = "Warning:  ";` |
|       40 |  2636 | `		break;` |
|        6 |  2637 | `	case PH7_CTX_NOTICE:` |
|       14 |  2638 | `		zErr = "Notice:  ";` |
|       12 |  2639 | `		break;` |
|       29 |  2640 | `	default:` |
|        - |  2641 | `		/* keep iErr unchanged */` |
|       58 |  2642 | `		break;` |
|        - |  2643 | `	}` |
|      110 |  2644 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2645 | `	if( pFuncName ){` |
|        - |  2646 | `		/* Append function name first */` |
|       23 |  2647 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2648 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2649 | `	}` |
|      110 |  2650 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2651 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2652 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2653 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2654 | `	}` |
|      110 |  2655 | `	return rc;` |
|       57 |  2656 |  |
|        - |  2657 | `/*` |
|        - |  2658 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2659 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2660 | ` * information.` |
|        - |  2661 | ` */` |
|       40 |  2662 | `static sxi32 VmThrowErrorAp(` |
|        - |  2663 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2664 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2665 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2666 | `	const char *zFormat, /* Format message */` |
|        - |  2667 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2668 | `	)` |
|        2 |  2669 |  |
|       42 |  2670 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2671 | `	SyBlob sMsg;` |
|        - |  2672 | `	SyString *pFile;` |
|        - |  2673 | `	char *zErr;` |
|       42 |  2674 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2675 | `	if( !pVm->bErrReport ){` |
|        - |  2676 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2677 | `		return SXRET_OK;` |
|        - |  2678 | `	}` |
|        - |  2679 | `	/* Reset the working buffer */` |
|       42 |  2680 | `	SyBlobReset(pWorker);` |
|        - |  2681 | `	/* Peek the processed file if available */` |
|       42 |  2682 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2683 | `	if( pFile ){` |
|        - |  2684 | `		/* Append file name */` |
|       42 |  2685 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2686 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2687 | `	}` |
|        - |  2688 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2689 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2690 | `	 * the correct errno value. */` |
|       42 |  2691 | `	zErr = "Error:  ";` |
|       42 |  2692 | `	switch(iErr){` |
|        4 |  2693 | `	case PH7_CTX_WARNING:` |
|        9 |  2694 | `		zErr = "Warning:  ";` |
|        9 |  2695 | `		break;` |
|        3 |  2696 | `	case PH7_CTX_NOTICE:` |
|        7 |  2697 | `		zErr = "Notice:  ";` |
|        6 |  2698 | `		break;` |
|       13 |  2699 | `	default:` |
|        - |  2700 | `		/* do not change iErr */` |
|       26 |  2701 | `		break;` |
|        - |  2702 | `	}` |
|       42 |  2703 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2704 | `	if( pFuncName ){` |
|        - |  2705 | `		/* Append function name first */` |
|       26 |  2706 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2707 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2708 | `	}` |
|        - |  2709 | `	/* Format the raw message */` |
|       42 |  2710 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2711 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2712 | `	/* Check if a user error handler is installed */` |
|       42 |  2713 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2714 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2715 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2716 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2717 | `	}` |
|       42 |  2718 | `	SyBlobRelease(&sMsg);` |
|       42 |  2719 | `	return rc;` |
|       22 |  2720 |  |
|        - |  2721 | `/*` |
|        - |  2722 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2723 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2724 | ` * possible.` |
|        - |  2725 | ` */` |
|       38 |  2726 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2727 |  |
|        - |  2728 | `	ph7_class *pClass;` |
|       39 |  2729 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2730 | `	ph7_class_instance *pThis;` |
|        - |  2731 | `	ph7_class_method *pCons;` |
|        - |  2732 | `	ph7_value sArg;` |
|        - |  2733 | `	ph7_value *apArg[1];` |
|        - |  2734 | `	SyBlob sMsg;` |
|        - |  2735 | `	SyString sMsgStr;` |
|        - |  2736 | `	VmFrame *pFrame;` |
|        - |  2737 | `	sxi32 rc;` |
|       39 |  2738 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2739 | `	if( pClass == 0 ){` |
|      ! 0 |  2740 | `		return PH7_ABORT;` |
|        - |  2741 | `	}` |
|       39 |  2742 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2743 | `	if( pThis == 0 ){` |
|      ! 0 |  2744 | `		return PH7_ABORT;` |
|        - |  2745 | `	}` |
|       39 |  2746 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2747 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2748 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2749 | `	{` |
|       39 |  2750 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2751 | `		if( pOwner ){` |
|       39 |  2752 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2753 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2754 | `		}else{` |
|      ! 0 |  2755 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2756 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2757 | `		}` |
|        - |  2758 | `	}` |
|       39 |  2759 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2760 | `	if( pCons ){` |
|       39 |  2761 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2762 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2763 | `		apArg[0] = &sArg;` |
|       39 |  2764 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2765 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2766 | `	}` |
|       39 |  2767 | `	SyBlobRelease(&sMsg);` |
|       39 |  2768 | `	pFrame = pVm->pFrame;` |
|       39 |  2769 | `	if( pFrame ){` |
|       39 |  2770 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2771 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2772 | `	}` |
|       39 |  2773 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2774 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2775 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2776 | `		return PH7_ABORT;` |
|        - |  2777 | `	}` |
|       39 |  2778 | `	return PH7_EXCEPTION;` |
|       20 |  2779 |  |
|        - |  2780 |  |
|        - |  2781 | `/*` |
|        - |  2782 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2783 | ` */` |
|        4 |  2784 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2785 |  |
|        - |  2786 | `	ph7_class *pErrClass;` |
|        - |  2787 | `	ph7_class_instance *pThis;` |
|        - |  2788 | `	ph7_class_method *pCons;` |
|        - |  2789 | `	ph7_value sArg;` |
|        - |  2790 | `	ph7_value *apArg[1];` |
|        - |  2791 | `	SyBlob sMsg;` |
|        - |  2792 | `	SyString sMsgStr;` |
|        - |  2793 | `	VmFrame *pFrame;` |
|        - |  2794 | `	sxi32 rc;` |
|        5 |  2795 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2796 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2797 | `		return PH7_ABORT;` |
|        - |  2798 | `	}` |
|        5 |  2799 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2800 | `	if( pThis == 0 ){` |
|      ! 0 |  2801 | `		return PH7_ABORT;` |
|        - |  2802 | `	}` |
|        5 |  2803 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2804 | `	{` |
|        5 |  2805 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2806 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2807 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2808 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2809 | `	}` |
|        5 |  2810 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2811 | `	if( pCons ){` |
|        5 |  2812 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2813 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2814 | `		apArg[0] = &sArg;` |
|        5 |  2815 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2816 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2817 | `	}` |
|        5 |  2818 | `	SyBlobRelease(&sMsg);` |
|        5 |  2819 | `	pFrame = pVm->pFrame;` |
|        5 |  2820 | `	if( pFrame ){` |
|        5 |  2821 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2822 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2823 | `	}` |
|        5 |  2824 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2825 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2826 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2827 | `		return PH7_ABORT;` |
|        - |  2828 | `	}` |
|        5 |  2829 | `	return PH7_EXCEPTION;` |
|        3 |  2830 |  |
|        - |  2831 |  |
|        - |  2832 | `/*` |
|        - |  2833 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2834 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2835 | ` * For class types, instanceof is verified.` |
|        - |  2836 | ` *` |
|        - |  2837 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2838 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2839 | ` */` |
|        - |  2840 | `/*` |
|        - |  2841 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2842 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2843 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2844 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2845 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2846 | ` */` |
|       20 |  2847 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2848 |  |
|        - |  2849 | `	const char *z, *zEnd, *zTail;` |
|        - |  2850 | `	sxu32 n;` |
|        - |  2851 | `	sxu8 bReal;` |
|        - |  2852 | `	sxi32 rc;` |
|       22 |  2853 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2854 | `		return 0;` |
|        - |  2855 | `	}` |
|       22 |  2856 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2857 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2858 | `	zEnd = z + n;` |
|       22 |  2859 | `	if( n == 0 ){` |
|      ! 0 |  2860 | `		return 0;` |
|        - |  2861 | `	}` |
|       22 |  2862 | `	zTail = 0;` |
|       22 |  2863 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2864 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2865 | `		return 0;` |
|        - |  2866 | `	}` |
|        - |  2867 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2868 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2869 | `		zTail++;` |
|      ! 0 |  2870 | `	}` |
|       16 |  2871 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2872 |  |
|        - |  2873 |  |
|        - |  2874 | `/*` |
|        - |  2875 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2876 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2877 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2878 | ` *   0 if it's not strictly numeric.` |
|        - |  2879 | ` */` |
|       16 |  2880 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2881 |  |
|        - |  2882 | `	const char *z, *zEnd, *zTail;` |
|        - |  2883 | `	sxu32 n;` |
|       18 |  2884 | `	sxu8 bReal = 0;` |
|        - |  2885 | `	sxi32 rc;` |
|       18 |  2886 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2887 | `		return 0;` |
|        - |  2888 | `	}` |
|       18 |  2889 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2890 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2891 | `	zEnd = z + n;` |
|       18 |  2892 | `	if( n == 0 ) return 0;` |
|       18 |  2893 | `	zTail = 0;` |
|       18 |  2894 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2895 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2896 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2897 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2898 | `	return bReal ? 2 : 1;` |
|       10 |  2899 |  |
|        - |  2900 |  |
|        - |  2901 | `/*` |
|        - |  2902 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2903 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2904 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2905 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2906 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2907 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2908 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2909 | ` * throw.` |
|        - |  2910 | ` *` |
|        - |  2911 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2912 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2913 | ` */` |
|       98 |  2914 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2915 |  |
|        - |  2916 | `	sxu32 i;` |
|        - |  2917 | `	ph7_type_alt *aAlts;` |
|        - |  2918 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2919 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2920 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2921 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2922 | `	}` |
|       88 |  2923 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2924 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2925 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2926 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2927 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2928 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2929 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2930 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2931 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2932 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2933 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2934 | `	}` |
|        - |  2935 | `	/* Object handling */` |
|       88 |  2936 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2937 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2938 | `		if( bHasClassAlt ){` |
|       14 |  2939 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2940 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2941 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2942 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2943 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2944 | `			}` |
|       26 |  2945 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2946 | `				ph7_class *pExpected;` |
|        - |  2947 | `				SyString *pCN;` |
|       22 |  2948 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2949 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2950 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2951 | `					pExpected = pSelfNow;` |
|       22 |  2952 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2953 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2954 | `				}else{` |
|       22 |  2955 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2956 | `				}` |
|       22 |  2957 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2958 | `					return SXRET_OK;` |
|        - |  2959 | `				}` |
|        8 |  2960 | `			}` |
|        2 |  2961 | `		}` |
|        9 |  2962 | `		return SXERR_INVALID;` |
|        - |  2963 | `	}` |
|        - |  2964 | `	/* Array handling */` |
|       72 |  2965 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2966 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2967 | `	}` |
|        - |  2968 | `	/* Scalar handling — exact match first */` |
|       66 |  2969 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2970 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2971 | `	}` |
|       42 |  2972 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2973 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2974 | `	}` |
|       38 |  2975 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2976 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2977 | `	}` |
|       18 |  2978 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2979 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2980 | `	}` |
|       18 |  2981 | `	if( bStrict ){` |
|        - |  2982 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2983 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2984 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2985 | `			return SXRET_OK;` |
|        - |  2986 | `		}` |
|      ! 0 |  2987 | `		return SXERR_INVALID;` |
|        - |  2988 | `	}` |
|        - |  2989 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2990 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2991 | `	 * to match PHP's union RFC. */` |
|        - |  2992 | `	{` |
|       18 |  2993 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2994 | `		if( bHasInt ){` |
|        - |  2995 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2996 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2997 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2998 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2999 | `				return SXRET_OK;` |
|        - |  3000 | `			}` |
|       18 |  3001 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3002 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3003 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3004 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3005 | `					return SXRET_OK;` |
|        - |  3006 | `				}` |
|      ! 0 |  3007 | `			}` |
|       18 |  3008 | `			if( kind == 1 ){` |
|        9 |  3009 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3010 | `				return SXRET_OK;` |
|        - |  3011 | `			}` |
|        4 |  3012 | `		}` |
|       10 |  3013 | `		if( bHasFloat ){` |
|       10 |  3014 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3015 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3016 | `				return SXRET_OK;` |
|        - |  3017 | `			}` |
|       10 |  3018 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3019 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3020 | `				return SXRET_OK;` |
|        - |  3021 | `			}` |
|        1 |  3022 | `		}` |
|        3 |  3023 | `		if( bHasString ){` |
|      ! 0 |  3024 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3025 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3026 | `				return SXRET_OK;` |
|        - |  3027 | `			}` |
|      ! 0 |  3028 | `		}` |
|        3 |  3029 | `		if( bHasBool ){` |
|      ! 0 |  3030 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3031 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3032 | `				return SXRET_OK;` |
|        - |  3033 | `			}` |
|      ! 0 |  3034 | `		}` |
|        - |  3035 | `	}` |
|        3 |  3036 | `	return SXERR_INVALID;` |
|       51 |  3037 |  |
|        - |  3038 |  |
|        - |  3039 | `/*` |
|        - |  3040 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3041 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3042 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3043 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3044 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3045 | ` */` |
|       36 |  3046 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3047 |  |
|       38 |  3048 | `	if( bStrict ){` |
|        - |  3049 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3050 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3051 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3052 | `			return SXRET_OK;` |
|        - |  3053 | `		}` |
|       10 |  3054 | `		return SXERR_INVALID;` |
|        - |  3055 | `	}` |
|        - |  3056 | `	{` |
|       28 |  3057 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3058 | `		if( xCast ) xCast(pVal);` |
|        - |  3059 | `	}` |
|       28 |  3060 | `	return SXRET_OK;` |
|       20 |  3061 |  |
|        - |  3062 |  |
|        - |  3063 | `/*` |
|        - |  3064 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3065 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3066 | ` *` |
|        - |  3067 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3068 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3069 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3070 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3071 | ` */` |
|       10 |  3072 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3073 |  |
|       12 |  3074 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       12 |  3075 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       12 |  3076 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       12 |  3077 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       12 |  3078 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        5 |  3079 | `		}` |
|       12 |  3080 | `		zBuf[nCopy] = 0;` |
|       12 |  3081 | `		return zBuf;` |
|        - |  3082 | `	}` |
|      ! 0 |  3083 | `	switch( nType ){` |
|      ! 0 |  3084 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3085 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3086 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3087 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3088 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3089 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3090 | `		default:             return "scalar";` |
|        - |  3091 | `	}` |
|        7 |  3092 |  |
|        - |  3093 |  |
|        - |  3094 | `/*` |
|        - |  3095 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3096 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3097 | ` */` |
|       18 |  3098 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3099 |  |
|       19 |  3100 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3101 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3102 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3103 | `	return zBuf;` |
|        1 |  3104 |  |
|        - |  3105 |  |
|    14524 |  3106 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3107 |  |
|        - |  3108 | `	SyHashEntry *pSlot;` |
|        - |  3109 | `	VmClassAttr *pVmAttr;` |
|        - |  3110 | `	ph7_class_attr *pAttr;` |
|    14526 |  3111 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14526 |  3112 | `	if( pSlot == 0 ){` |
|    14318 |  3113 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3114 | `	}` |
|      210 |  3115 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3116 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3117 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3118 | `		return SXRET_OK;` |
|        - |  3119 | `	}` |
|        - |  3120 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3121 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3122 | `	 * matching PHP's documented behavior. */` |
|      210 |  3123 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3124 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3125 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3126 |  |
|       16 |  3127 | `		if( rc == SXRET_OK ){` |
|        9 |  3128 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3129 | `			return SXRET_OK;` |
|        - |  3130 | `		}` |
|        7 |  3131 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3132 | `			char zBuf[128];` |
|        4 |  3133 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3134 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3135 | `		}` |
|        5 |  3136 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3137 | `	}` |
|        - |  3138 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3139 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3140 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3141 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3142 | `			return SXRET_OK;` |
|        - |  3143 | `		}` |
|        3 |  3144 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3145 | `	}` |
|        - |  3146 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3147 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3148 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3149 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3150 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3151 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3152 | `			return SXRET_OK;` |
|        - |  3153 | `		}` |
|        7 |  3154 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3155 | `	}` |
|      174 |  3156 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3157 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3158 | `		 * currently active on the self-stack. */` |
|       26 |  3159 | `		ph7_class *pExpected = 0;` |
|       26 |  3160 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3161 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3162 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3163 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3164 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3165 | `		}` |
|       26 |  3166 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3167 | `			pExpected = pSelfNow;` |
|       24 |  3168 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3169 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3170 | `		}else{` |
|       22 |  3171 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3172 | `		}` |
|       26 |  3173 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3174 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3175 | `		}` |
|       26 |  3176 | `		if( pExpected ){` |
|       22 |  3177 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3178 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3179 | `				char zBuf[128];` |
|        7 |  3180 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3181 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3182 | `			}` |
|        8 |  3183 | `		}` |
|       22 |  3184 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3185 | `		return SXRET_OK;` |
|        - |  3186 | `	}` |
|        - |  3187 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3188 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3189 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3190 | `		char zBuf[128];` |
|       10 |  3191 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3192 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3193 | `	}` |
|      144 |  3194 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3195 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3196 | `		if( xCast ){` |
|        - |  3197 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3198 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3199 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3200 | `			}` |
|       24 |  3201 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3202 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3203 | `			}` |
|        - |  3204 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3205 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3206 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3207 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3208 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3209 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3210 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3211 | `			}` |
|       12 |  3212 | `			xCast(pValue);` |
|        5 |  3213 | `		}` |
|        5 |  3214 | `	}` |
|      130 |  3215 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3216 | `	return SXRET_OK;` |
|     7264 |  3217 |  |
|        - |  3218 |  |
|        - |  3219 | `/*` |
|        - |  3220 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3221 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3222 | ` * information.` |
|        - |  3223 | ` * ------------------------------------` |
|        - |  3224 | ` * Simple boring wrapper function.` |
|        - |  3225 | ` * ------------------------------------` |
|        - |  3226 | ` */` |
|       16 |  3227 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3228 |  |
|        - |  3229 | `	va_list ap;` |
|        - |  3230 | `	sxi32 rc;` |
|       17 |  3231 | `	va_start(ap,zFormat);` |
|       17 |  3232 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3233 | `	va_end(ap);` |
|       17 |  3234 | `	return rc;` |
|        1 |  3235 |  |
|        - |  3236 | `/*` |
|        - |  3237 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3238 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3239 | ` */` |
|       36 |  3240 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3241 |  |
|        - |  3242 | `	ph7_class *pClass;` |
|        - |  3243 | `	ph7_class_instance *pThis;` |
|        - |  3244 | `	ph7_class_method *pCons;` |
|        - |  3245 | `	ph7_value sArg;` |
|        - |  3246 | `	ph7_value *apArg[1];` |
|        - |  3247 | `	SyBlob sMsg;` |
|        - |  3248 | `	SyString sMsgStr;` |
|        - |  3249 | `	VmFrame *pFrame;` |
|        - |  3250 | `	sxi32 rc;` |
|       38 |  3251 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       38 |  3252 | `	if( pClass == 0 ){` |
|      ! 0 |  3253 | `		return PH7_ABORT;` |
|        - |  3254 | `	}` |
|       38 |  3255 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       38 |  3256 | `	if( pThis == 0 ){` |
|      ! 0 |  3257 | `		return PH7_ABORT;` |
|        - |  3258 | `	}` |
|       38 |  3259 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       38 |  3260 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       18 |  3261 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       38 |  3262 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       38 |  3263 | `	if( pCons ){` |
|       38 |  3264 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       38 |  3265 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       38 |  3266 | `		apArg[0] = &sArg;` |
|       38 |  3267 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       38 |  3268 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  3269 | `	}` |
|       38 |  3270 | `	SyBlobRelease(&sMsg);` |
|       38 |  3271 | `	pFrame = pVm->pFrame;` |
|       38 |  3272 | `	if( pFrame ){` |
|       38 |  3273 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  3274 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  3275 | `	}` |
|       38 |  3276 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  3277 | `	PH7_ClassInstanceUnref(pThis);` |
|       38 |  3278 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3279 | `		return PH7_ABORT;` |
|        - |  3280 | `	}` |
|       34 |  3281 | `	return PH7_EXCEPTION;` |
|       20 |  3282 |  |
|        - |  3283 | `/*` |
|        - |  3284 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3285 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3286 | ` */` |
|        6 |  3287 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3288 |  |
|        - |  3289 | `	ph7_class *pClass;` |
|        - |  3290 | `	ph7_class_instance *pThis;` |
|        - |  3291 | `	ph7_class_method *pCons;` |
|        - |  3292 | `	ph7_value sArg;` |
|        - |  3293 | `	ph7_value *apArg[1];` |
|        - |  3294 | `	SyBlob sMsg;` |
|        - |  3295 | `	SyString sMsgStr;` |
|        - |  3296 | `	VmFrame *pFrame;` |
|        - |  3297 | `	sxi32 rc;` |
|        7 |  3298 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3299 | `	if( pClass == 0 ){` |
|      ! 0 |  3300 | `		return PH7_ABORT;` |
|        - |  3301 | `	}` |
|        7 |  3302 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3303 | `	if( pThis == 0 ){` |
|      ! 0 |  3304 | `		return PH7_ABORT;` |
|        - |  3305 | `	}` |
|        7 |  3306 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3307 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3308 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3309 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3310 | `	if( pCons ){` |
|        7 |  3311 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3312 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3313 | `		apArg[0] = &sArg;` |
|        7 |  3314 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3315 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3316 | `	}` |
|        7 |  3317 | `	SyBlobRelease(&sMsg);` |
|        7 |  3318 | `	pFrame = pVm->pFrame;` |
|        7 |  3319 | `	if( pFrame ){` |
|        7 |  3320 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3321 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3322 | `	}` |
|        7 |  3323 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3324 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3325 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3326 | `		return PH7_ABORT;` |
|        - |  3327 | `	}` |
|      ! 0 |  3328 | `	return PH7_EXCEPTION;` |
|        4 |  3329 |  |
|        - |  3330 | `/*` |
|        - |  3331 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3332 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3333 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3334 | ` */` |
|       16 |  3335 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3336 |  |
|       17 |  3337 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3338 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3339 | `	}` |
|       13 |  3340 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3341 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3342 | `		if( pThis && pThis->pClass ){` |
|        5 |  3343 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3344 | `			sxu32 n = pName->nByte;` |
|        5 |  3345 | `			if( n >= nBuf ){` |
|      ! 0 |  3346 | `				n = nBuf - 1;` |
|      ! 0 |  3347 | `			}` |
|        5 |  3348 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3349 | `			zBuf[n] = 0;` |
|        5 |  3350 | `			return zBuf;` |
|        - |  3351 | `		}` |
|      ! 0 |  3352 | `		return "object";` |
|        - |  3353 | `	}` |
|        9 |  3354 | `	return ph7_type_name(pVal);` |
|        9 |  3355 |  |
|        - |  3356 | `/*` |
|        - |  3357 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3358 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3359 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3360 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3361 | ` */` |
|       16 |  3362 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3363 |  |
|        - |  3364 | `	ph7_class *pClass;` |
|        - |  3365 | `	ph7_class_instance *pThis;` |
|        - |  3366 | `	ph7_class_method *pCons;` |
|        - |  3367 | `	ph7_value sArg;` |
|        - |  3368 | `	ph7_value *apArg[1];` |
|        - |  3369 | `	SyBlob sMsg;` |
|        - |  3370 | `	SyString sMsgStr;` |
|        - |  3371 | `	VmFrame *pFrame;` |
|        - |  3372 | `	sxi32 rc;` |
|       17 |  3373 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3374 | `	char zNameBuf[64];` |
|       17 |  3375 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3376 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3377 | `	if( pClass == 0 ){` |
|      ! 0 |  3378 | `		return PH7_ABORT;` |
|        - |  3379 | `	}` |
|       17 |  3380 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3381 | `	if( pThis == 0 ){` |
|      ! 0 |  3382 | `		return PH7_ABORT;` |
|        - |  3383 | `	}` |
|       17 |  3384 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3385 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3386 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3387 | `	if( pCons ){` |
|       17 |  3388 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3389 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3390 | `		apArg[0] = &sArg;` |
|       17 |  3391 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3392 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3393 | `	}` |
|       17 |  3394 | `	SyBlobRelease(&sMsg);` |
|       17 |  3395 | `	pFrame = pVm->pFrame;` |
|       17 |  3396 | `	if( pFrame ){` |
|       17 |  3397 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3398 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3399 | `	}` |
|       17 |  3400 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3401 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3402 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3403 | `		return PH7_ABORT;` |
|        - |  3404 | `	}` |
|       17 |  3405 | `	return PH7_EXCEPTION;` |
|        9 |  3406 |  |
|        - |  3407 | `/*` |
|        - |  3408 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3409 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3410 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3411 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3412 | ` */` |
|        - |  3413 | `/*` |
|        - |  3414 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3415 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3416 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3417 | ` */` |
|       24 |  3418 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3419 |  |
|        - |  3420 | `	sxu32 nCopy;` |
|       26 |  3421 | `	if( nBuf == 0 ) return "";` |
|       26 |  3422 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3423 | `		zBuf[0] = 0;` |
|      ! 0 |  3424 | `		return zBuf;` |
|        - |  3425 | `	}` |
|       26 |  3426 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3427 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3428 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3429 | `	zBuf[nCopy] = 0;` |
|       26 |  3430 | `	return zBuf;` |
|       14 |  3431 |  |
|        - |  3432 |  |
|      396 |  3433 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3434 |  |
|      398 |  3435 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3436 | `	const char *zGiven;` |
|        - |  3437 | `	char zBuf[128];` |
|        - |  3438 | `	char zTypeBuf[128];` |
|        - |  3439 | `	/* Untyped function: no enforcement. */` |
|      398 |  3440 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3441 | `		return SXRET_OK;` |
|        - |  3442 | `	}` |
|        - |  3443 | `	/* void return type: the function must not produce a value. */` |
|      398 |  3444 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3445 | `		if( pValue == 0 ){` |
|      134 |  3446 | `			return SXRET_OK;` |
|        - |  3447 | `		}` |
|        - |  3448 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3449 | `		 * still counts as "returned a value" here. */` |
|        3 |  3450 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3451 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3452 | `	}` |
|        - |  3453 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3454 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3455 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      264 |  3456 | `	if( pValue == 0 ){` |
|      ! 0 |  3457 | `		const char *zExpected = "value";` |
|      ! 0 |  3458 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3459 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3460 | `		}` |
|      ! 0 |  3461 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3462 | `	}` |
|        - |  3463 | ``	/* `mixed` accepts any explicitly returned value, including null. It is`` |
|        - |  3464 | `	 * parsed as a class-name atom (SXU32_HIGH, sReturnClass = "mixed") since` |
|        - |  3465 | `	 * it is not a scalar keyword, so short-circuit it here before the null /` |
|        - |  3466 | `	 * class-type checks below — which would otherwise demand an object. */` |
|      272 |  3467 | `	if( pFunc->nReturnType == SXU32_HIGH` |
|      143 |  3468 | `	 && pFunc->sReturnClass.nByte == 5` |
|       24 |  3469 | `	 && SyStrnicmp(pFunc->sReturnClass.zString,"mixed",5) == 0 ){` |
|       21 |  3470 | `		return SXRET_OK;` |
|        - |  3471 | `	}` |
|        - |  3472 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3473 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3474 | `	 * bNullable=0 here. */` |
|      244 |  3475 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3476 | `		sxi32 rcU;` |
|      ! 0 |  3477 | `		int bNullable = 0;` |
|      ! 0 |  3478 | `		const char *zExpected = "union";` |
|        - |  3479 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3480 | `		{` |
|        - |  3481 | `			sxu32 i;` |
|      ! 0 |  3482 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3483 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3484 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3485 | `			}` |
|        - |  3486 | `		}` |
|      ! 0 |  3487 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3488 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3489 | `			return SXRET_OK;` |
|        - |  3490 | `		}` |
|      ! 0 |  3491 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3492 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3493 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3494 | `			zGiven = "null";` |
|      ! 0 |  3495 | `		}else{` |
|      ! 0 |  3496 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3497 | `		}` |
|      ! 0 |  3498 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3499 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3500 | `		}` |
|      ! 0 |  3501 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3502 | `	}` |
|        - |  3503 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3504 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3505 | `	 * it into the TypeError message. */` |
|      244 |  3506 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3507 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3508 | `		const char *zExpected;` |
|        - |  3509 | `		ph7_class *pExpected;` |
|        6 |  3510 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3511 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3512 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3513 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3514 | `		}` |
|        6 |  3515 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3516 | `			pExpected = pSelfNow;` |
|        4 |  3517 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3518 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3519 | `		}else{` |
|        3 |  3520 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3521 | `		}` |
|        6 |  3522 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3523 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3524 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3525 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3526 | `		}` |
|        6 |  3527 | `		if( pExpected ){` |
|        6 |  3528 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3529 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3530 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3531 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3532 | `			}` |
|        2 |  3533 | `		}` |
|        6 |  3534 | `		return SXRET_OK;` |
|        - |  3535 | `	}` |
|        - |  3536 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3537 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3538 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3539 | `	 * via the type-text leading '?'. */` |
|      240 |  3540 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3541 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3542 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3543 | `			return SXRET_OK;` |
|        - |  3544 | `		}` |
|      ! 0 |  3545 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3546 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3547 | `			"null");` |
|        - |  3548 | `	}` |
|        - |  3549 | `	/* Exact match? Done. */` |
|      234 |  3550 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3551 | `		return SXRET_OK;` |
|        - |  3552 | `	}` |
|        - |  3553 | `	/* Object->scalar is never compatible. */` |
|        8 |  3554 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3555 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3556 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3557 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3558 | `			zGiven);` |
|        - |  3559 | `	}` |
|        - |  3560 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3561 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3562 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3563 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3564 | `			ph7_type_name(pValue));` |
|        - |  3565 | `	}` |
|        - |  3566 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3567 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3568 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3569 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3570 | `	if( !bStrict` |
|        5 |  3571 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3572 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3573 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3574 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3575 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3576 | `			"string");` |
|        - |  3577 | `	}` |
|        6 |  3578 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3579 | `		return SXRET_OK;` |
|        - |  3580 | `	}` |
|        4 |  3581 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3582 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3583 | `		ph7_type_name(pValue));` |
|      200 |  3584 |  |
|        - |  3585 | `/*` |
|        - |  3586 | ` * Report a fatal named-argument error.` |
|        - |  3587 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3588 | ` */` |
|        6 |  3589 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3590 |  |
|        7 |  3591 | `	const char *zFunc = 0;` |
|        7 |  3592 | `	int nFunc = 0;` |
|        7 |  3593 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3594 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3595 |  |
|        - |  3596 | `/*` |
|        - |  3597 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3598 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3599 | ` * information.` |
|        - |  3600 | ` * ------------------------------------` |
|        - |  3601 | ` * Simple boring wrapper function.` |
|        - |  3602 | ` * ------------------------------------` |
|        - |  3603 | ` */` |
|       24 |  3604 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3605 |  |
|        - |  3606 | `	sxi32 rc;` |
|       26 |  3607 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3608 | `	return rc;` |
|        2 |  3609 |  |
|        - |  3610 | `/*` |
|        - |  3611 | ` * Resolve function context from the current frame.` |
|        - |  3612 | ` */` |
|     1018 |  3613 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3614 |  |
|        - |  3615 | `	VmFrame *pFrame;` |
|        - |  3616 | `	ph7_vm_func *pFunc;` |
|     1019 |  3617 | `	*pzFuncName = 0;` |
|     1019 |  3618 | `	*pnFuncLen = 0;` |
|     1019 |  3619 | `	pFrame = pVm->pFrame;` |
|     1019 |  3620 | `	if( pFrame == 0 ){` |
|      ! 0 |  3621 | `		return;` |
|        - |  3622 | `	}` |
|     1019 |  3623 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3624 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3625 | `		return;` |
|        - |  3626 | `	}` |
|       25 |  3627 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3628 | `	if( pFunc == 0 ){` |
|      ! 0 |  3629 | `		return;` |
|        - |  3630 | `	}` |
|       25 |  3631 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3632 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3633 |  |
|        - |  3634 | `/*` |
|        - |  3635 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3636 | ` */` |
|      524 |  3637 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3638 |  |
|        - |  3639 | `	SyBlob sOut;` |
|        - |  3640 | `	SyString *pFile;` |
|      525 |  3641 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3642 | `		return PH7_OK;` |
|        - |  3643 | `	}` |
|      525 |  3644 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3645 | `		zClass = "Exception";` |
|      ! 0 |  3646 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3647 | `	}` |
|      525 |  3648 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3649 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3650 | `	}` |
|      525 |  3651 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3652 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3653 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3654 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3655 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3656 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3657 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3658 | `	}` |
|      525 |  3659 | `	if( pFile ){` |
|      525 |  3660 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  3661 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3662 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  3663 | `	}` |
|      525 |  3664 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  3665 | `	if( pFile ){` |
|      525 |  3666 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  3667 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3668 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3669 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3670 | `		}else{` |
|      501 |  3671 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3672 | `		}` |
|      262 |  3673 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3674 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3675 | `	}else{` |
|      ! 0 |  3676 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3677 | `	}` |
|      525 |  3678 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  3679 | `	if( pFile ){` |
|      525 |  3680 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  3681 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  3682 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3683 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  3684 | `	}` |
|      525 |  3685 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  3686 | `	SyBlobRelease(&sOut);` |
|      525 |  3687 | `	return PH7_ABORT;` |
|      263 |  3688 |  |
|        - |  3689 | `/*` |
|        - |  3690 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  3691 | ` *` |
|        - |  3692 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  3693 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  3694 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  3695 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  3696 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  3697 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  3698 | ` */` |
|      862 |  3699 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  3700 |  |
|      864 |  3701 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  3702 | `		if( pVm->pCoalesceObj ){` |
|        7 |  3703 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  3704 | `		}` |
|        7 |  3705 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  3706 | `		pVm->pCoalesceObj = 0;` |
|        7 |  3707 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  3708 | `	}` |
|      864 |  3709 |  |
|        - |  3710 | `/*` |
|        - |  3711 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  3712 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  3713 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  3714 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  3715 | ` *` |
|        - |  3716 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  3717 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  3718 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  3719 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  3720 | ` */` |
|        4 |  3721 | `static sxi32 VmThrowFromVm(` |
|        - |  3722 | `	ph7_vm *pVm,` |
|        - |  3723 | `	const char *zClass,` |
|        - |  3724 | `	const char *zMsg,` |
|        - |  3725 | `	sxu32 nMsg` |
|        1 |  3726 | `){` |
|        - |  3727 | `	ph7_class *pClass;` |
|        - |  3728 | `	ph7_class_instance *pThis;` |
|        - |  3729 | `	ph7_class_method *pCons;` |
|        - |  3730 | `	VmFrame *pFrame;` |
|        - |  3731 | `	sxi32 rc;` |
|        5 |  3732 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  3733 | `	if( pClass == 0 ){` |
|      ! 0 |  3734 | `		return SXERR_ABORT;` |
|        - |  3735 | `	}` |
|        5 |  3736 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  3737 | `	if( pThis == 0 ){` |
|      ! 0 |  3738 | `		return SXERR_ABORT;` |
|        - |  3739 | `	}` |
|        5 |  3740 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3741 | `	if( pCons ){` |
|        - |  3742 | `		ph7_value sArg;` |
|        - |  3743 | `		ph7_value *apArg[1];` |
|        - |  3744 | `		SyString sMsgStr;` |
|        5 |  3745 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  3746 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  3747 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  3748 | `		apArg[0] = &sArg;` |
|        5 |  3749 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  3750 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3751 | `	}` |
|        5 |  3752 | `	pFrame = pVm->pFrame;` |
|        5 |  3753 | `	if( pFrame ){` |
|        5 |  3754 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3755 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3756 | `	}` |
|        5 |  3757 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  3758 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3759 | `	return rc;` |
|        3 |  3760 |  |
|        - |  3761 | `/*` |
|        - |  3762 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3763 | ` */` |
|      574 |  3764 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3765 |  |
|        - |  3766 | `	ph7_vm *pVm;` |
|        - |  3767 | `	ph7_class *pClass;` |
|        - |  3768 | `	ph7_class_instance *pThis;` |
|        - |  3769 | `	ph7_class_method *pCons;` |
|        - |  3770 | `	ph7_value sArg;` |
|        - |  3771 | `	ph7_value *apArg[1];` |
|        - |  3772 | `	SyBlob sMsg;` |
|        - |  3773 | `	SyString sMsgStr;` |
|        - |  3774 | `	VmFrame *pFrame;` |
|        - |  3775 | `	va_list ap;` |
|        - |  3776 | `	sxi32 rc;` |
|        - |  3777 |  |
|      576 |  3778 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3779 | `		return PH7_ABORT;` |
|        - |  3780 | `	}` |
|      576 |  3781 | `	pVm = pCtx->pVm;` |
|      576 |  3782 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3783 | `		zClass = "Error";` |
|      ! 0 |  3784 | `	}` |
|      576 |  3785 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  3786 | `	if( pClass == 0 ){` |
|      ! 0 |  3787 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3788 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3789 | `			zClass` |
|        - |  3790 | `			);` |
|        - |  3791 | `	}` |
|      576 |  3792 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  3793 | `	if( pThis == 0 ){` |
|      ! 0 |  3794 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3795 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3796 | `			);` |
|        - |  3797 | `	}` |
|        - |  3798 |  |
|      576 |  3799 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  3800 | `	va_start(ap,zFormat);` |
|      576 |  3801 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  3802 | `	va_end(ap);` |
|        - |  3803 |  |
|      576 |  3804 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  3805 | `	if( pCons ){` |
|      576 |  3806 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  3807 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  3808 | `		apArg[0] = &sArg;` |
|      576 |  3809 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  3810 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  3811 | `	}` |
|      576 |  3812 | `	SyBlobRelease(&sMsg);` |
|        - |  3813 |  |
|      576 |  3814 | `	pFrame = pVm->pFrame;` |
|      576 |  3815 | `	if( pFrame ){` |
|      576 |  3816 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  3817 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  3818 | `	}` |
|      576 |  3819 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  3820 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  3821 | `	if( rc == SXERR_ABORT ){` |
|      491 |  3822 | `		return PH7_ABORT;` |
|        - |  3823 | `	}` |
|       86 |  3824 | `	return PH7_EXCEPTION;` |
|      289 |  3825 |  |
|        - |  3826 | `/*` |
|        - |  3827 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3828 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3829 | ` */` |
|      ! 0 |  3830 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3831 |  |
|        - |  3832 | `	ph7_vm *pVm;` |
|        - |  3833 | `	SyBlob sMsg;` |
|      ! 0 |  3834 | `	const char *zFuncName = 0;` |
|      ! 0 |  3835 | `	int nFuncLen = 0;` |
|        - |  3836 | `	va_list ap;` |
|        - |  3837 | `	sxi32 rc;` |
|        - |  3838 |  |
|      ! 0 |  3839 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3840 | `		return PH7_OK;` |
|        - |  3841 | `	}` |
|      ! 0 |  3842 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3843 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3844 | `		zClass = "Error";` |
|      ! 0 |  3845 | `	}` |
|        - |  3846 |  |
|      ! 0 |  3847 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3848 |  |
|      ! 0 |  3849 | `	va_start(ap,zFormat);` |
|      ! 0 |  3850 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3851 | `	va_end(ap);` |
|        - |  3852 |  |
|      ! 0 |  3853 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3854 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3855 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3856 | `	}` |
|      ! 0 |  3857 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3858 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3859 | `	}` |
|      ! 0 |  3860 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3861 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3862 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3863 | `	return rc;` |
|      ! 0 |  3864 |  |
|        - |  3865 | `/*` |
|        - |  3866 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3867 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3868 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3869 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3870 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3871 | ` * when VmByteCodeExec returns.` |
|        - |  3872 | ` */` |
|      144 |  3873 | `static sxi32 VmSuspendCtx(` |
|        - |  3874 | `	ph7_vm *pVm,` |
|        - |  3875 | `	ph7_exec_ctx *pCtx,` |
|        - |  3876 | `	sxi32 pc,` |
|        - |  3877 | `	sxi32 nTos` |
|        - |  3878 | `	)` |
|        2 |  3879 |  |
|       72 |  3880 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3881 | `	pCtx->pc = pc;` |
|      146 |  3882 | `	pCtx->nTos = nTos;` |
|      146 |  3883 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3884 | `	return PH7_SUSPEND;` |
|        2 |  3885 |  |
|        - |  3886 | `/*` |
|        - |  3887 | ` * Resolve named-argument mapping.` |
|        - |  3888 | ` *` |
|        - |  3889 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3890 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3891 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3892 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3893 | ` * every formal parameter that received a value.` |
|        - |  3894 | ` *` |
|        - |  3895 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3896 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3897 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3898 | ` */` |
|       98 |  3899 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3900 | `	ph7_vm *pVm,` |
|        - |  3901 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3902 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3903 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3904 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3905 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3906 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3907 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3908 |  |
|        2 |  3909 |  |
|      100 |  3910 | `	sxi32 posIdx = 0;` |
|        - |  3911 | `	sxu32 i;` |
|        - |  3912 | `	char zErrMsg[256];` |
|      100 |  3913 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3914 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3915 | `		aSlot[i] = -2;` |
|      100 |  3916 | `	}` |
|      290 |  3917 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3918 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3919 | `			/* Named argument — find formal by name */` |
|      184 |  3920 | `			int found = 0;` |
|        - |  3921 | `			sxu32 k;` |
|      304 |  3922 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3923 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3924 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3925 | `						pMap->aNames[i].zString,` |
|      402 |  3926 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3927 | `					if( aUsed[k] ){` |
|        7 |  3928 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3929 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3930 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3931 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3932 | `						return PH7_ABORT;` |
|        - |  3933 | `					}` |
|      168 |  3934 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3935 | `					aUsed[k] = 1;` |
|      168 |  3936 | `					found = 1;` |
|      168 |  3937 | `					break;` |
|        - |  3938 | `				}` |
|       62 |  3939 | `			}` |
|      180 |  3940 | `			if( !found ){` |
|       14 |  3941 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3942 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3943 | `				}else{` |
|        4 |  3944 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3945 | `						"Unknown named parameter $%.*s",` |
|        2 |  3946 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3947 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3948 | `					return PH7_ABORT;` |
|        - |  3949 | `				}` |
|        5 |  3950 | `			}` |
|       90 |  3951 | `		}else{` |
|        - |  3952 | `			/* Positional argument */` |
|       16 |  3953 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3954 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3955 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3956 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3957 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3958 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3959 | `					return PH7_ABORT;` |
|        - |  3960 | `				}` |
|       16 |  3961 | `				aSlot[i] = posIdx;` |
|       16 |  3962 | `				aUsed[posIdx] = 1;` |
|        7 |  3963 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3964 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3965 | `			}` |
|       16 |  3966 | `			posIdx++;` |
|        - |  3967 | `		}` |
|       97 |  3968 | `	}` |
|       93 |  3969 | `	return SXRET_OK;` |
|       51 |  3970 |  |
|        - |  3971 | `/*` |
|        - |  3972 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3973 | ` *` |
|        - |  3974 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3975 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3976 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3977 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3978 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3979 | ` * then the program execution is halted.` |
|        - |  3980 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3981 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3982 | ` * or to reset the VM to it's initial state.` |
|        - |  3983 | ` */` |
|    44834 |  3984 | `static sxi32 VmByteCodeExec(` |
|        - |  3985 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3986 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3987 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3988 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3989 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3990 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3991 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3992 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3993 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3994 | `	)` |
|        2 |  3995 |  |
|        - |  3996 | `	VmInstr *pInstr;` |
|        - |  3997 | `	ph7_value *pTos;` |
|        - |  3998 | `	SySet aArg;` |
|        - |  3999 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4000 | `	sxi32 pc;` |
|        - |  4001 | `	sxi32 rc;` |
|        - |  4002 | `	/* Argument container */` |
|    44836 |  4003 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    44836 |  4004 | `	if( nTos < 0 ){` |
|    41686 |  4005 | `		pTos = &pStack[-1];` |
|    20844 |  4006 | `	}else{` |
|     3152 |  4007 | `		pTos = &pStack[nTos];` |
|        - |  4008 | `	}` |
|    44836 |  4009 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    44836 |  4010 | `	pc = nPc;` |
|        - |  4011 | `/*` |
|        - |  4012 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4013 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4014 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4015 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4016 | ` */` |
|        - |  4017 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4018 | `	{ \` |
|        - |  4019 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4020 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4021 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4022 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4023 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4024 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4025 | `				break; \` |
|        - |  4026 | `			} \` |
|        - |  4027 | `			goto Exception; \` |
|        - |  4028 | `		} \` |
|        - |  4029 | `	}` |
|        - |  4030 | `	/* Execute as much as we can */` |
|  5896175 |  4031 | `	for(;;){` |
|        - |  4032 | `		/* Fetch the instruction to execute */` |
| 11791648 |  4033 | `		pInstr = &aInstr[pc];` |
| 11791648 |  4034 | `		rc = SXRET_OK;` |
|        - |  4035 | `/*` |
|        - |  4036 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4037 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4038 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4039 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4040 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4041 | ` */` |
| 11791648 |  4042 | `		switch(pInstr->iOp){` |
|        - |  4043 | `/*` |
|        - |  4044 | ` * DONE: P1 * *` |
|        - |  4045 | ` *` |
|        - |  4046 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4047 | ` * and return immediately.` |
|        - |  4048 | ` */` |
|    22039 |  4049 | `case PH7_OP_DONE:` |
|        - |  4050 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4051 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4052 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4053 | `	 * callback trampolines, and the main script. */` |
|    44078 |  4054 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      402 |  4055 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4056 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4057 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4058 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4059 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4060 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4061 | `		 * exception. */` |
|      398 |  4062 | `		ph7_value *pRetVal = 0;` |
|      398 |  4063 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      266 |  4064 | `			pRetVal = pTos;` |
|      132 |  4065 | `		}` |
|      398 |  4066 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      398 |  4067 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      392 |  4068 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4069 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4070 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4071 | `				pTos--;` |
|      ! 0 |  4072 | `			}` |
|      ! 0 |  4073 | `			goto Exception;` |
|        - |  4074 | `		}` |
|        - |  4075 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4076 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4077 | `		 * defensively we clear the pointer after a successful check). */` |
|      392 |  4078 | `		pEnforceRetFunc = 0;` |
|      195 |  4079 | `	}` |
|    44074 |  4080 | `	if( pInstr->iP1 ){` |
|        - |  4081 | `#ifdef UNTRUST` |
|        - |  4082 | `		if( pTos < pStack ){` |
|        - |  4083 | `			goto Abort;` |
|        - |  4084 | `		}` |
|        - |  4085 | `#endif` |
|    26782 |  4086 | `		if( pLastRef ){` |
|    16384 |  4087 | `			*pLastRef = pTos->nIdx;` |
|     8191 |  4088 | `		}` |
|    26782 |  4089 | `		if( pResult ){` |
|        - |  4090 | `			/* Execution result */` |
|    25306 |  4091 | `			PH7_MemObjStore(pTos,pResult);` |
|    12652 |  4092 | `		}` |
|    26782 |  4093 | `		VmPopOperand(&pTos,1);` |
|    30684 |  4094 | `	}else if( pLastRef ){` |
|        - |  4095 | `		/* Nothing referenced */` |
|     1936 |  4096 | `		*pLastRef = SXU32_HIGH;` |
|      967 |  4097 | `	}` |
|        - |  4098 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4099 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4100 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4101 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4102 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4103 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4104 | `	 * block can override it.` |
|        - |  4105 | `	 */` |
|    44076 |  4106 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4107 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4108 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4109 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4110 | `		pExc->pFrame = 0;` |
|        3 |  4111 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4112 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4113 | `			pExc->iFinallyDone = 1;` |
|        - |  4114 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4115 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4116 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4117 | `				goto Abort;` |
|        - |  4118 | `			}` |
|        1 |  4119 | `		}` |
|        1 |  4120 | `	}` |
|    44074 |  4121 | `	goto Done;` |
|        - |  4122 | `/*` |
|        - |  4123 | ` * HALT: P1 * *` |
|        - |  4124 | ` *` |
|        - |  4125 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4126 | ` * and abort immediately.` |
|        - |  4127 | ` */` |
|        7 |  4128 | `case PH7_OP_HALT:` |
|       15 |  4129 | `	if( pInstr->iP1 ){` |
|        - |  4130 | `#ifdef UNTRUST` |
|        - |  4131 | `		if( pTos < pStack ){` |
|        - |  4132 | `			goto Abort;` |
|        - |  4133 | `		}` |
|        - |  4134 | `#endif` |
|       15 |  4135 | `		if( pLastRef ){` |
|        3 |  4136 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4137 | `		}` |
|       15 |  4138 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4139 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4140 | `				/* Output the exit message */` |
|       16 |  4141 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4142 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4143 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4144 | `			}` |
|       10 |  4145 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4146 | `			/* Record exit status */` |
|        5 |  4147 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4148 | `		}` |
|       15 |  4149 | `		VmPopOperand(&pTos,1);` |
|        7 |  4150 | `	}else if( pLastRef ){` |
|        - |  4151 | `		/* Nothing referenced */` |
|      ! 0 |  4152 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4153 | `	}` |
|        - |  4154 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4155 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4156 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4157 | `	 */` |
|       15 |  4158 | `	pVm->bHaltRequested = 1;` |
|       15 |  4159 | `	goto Abort;` |
|        - |  4160 | `/*` |
|        - |  4161 | ` * JMP: * P2 *` |
|        - |  4162 | ` *` |
|        - |  4163 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4164 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4165 | ` */` |
|   251177 |  4166 | `case PH7_OP_JMP:` |
|   502400 |  4167 | `	pc = pInstr->iP2 - 1;` |
|   502400 |  4168 | `	break;` |
|        - |  4169 | `/*` |
|        - |  4170 | ` * JZ: P1 P2 *` |
|        - |  4171 | ` *` |
|        - |  4172 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4173 | ` * entry in the stack if P1 is zero.` |
|        - |  4174 | ` */` |
|   596373 |  4175 | `case PH7_OP_JZ:` |
|        - |  4176 | `#ifdef UNTRUST` |
|        - |  4177 | `	if( pTos < pStack ){` |
|        - |  4178 | `		goto Abort;` |
|        - |  4179 | `	}` |
|        - |  4180 | `#endif` |
|        - |  4181 | `	/* Get a boolean value */` |
|  1192836 |  4182 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4183 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4184 | `	}` |
|  1192836 |  4185 | `	if( !pTos->x.iVal ){` |
|        - |  4186 | `		/* Take the jump */` |
|   613646 |  4187 | `		pc = pInstr->iP2 - 1;` |
|   306822 |  4188 | `	}` |
|  1192836 |  4189 | `	if( !pInstr->iP1 ){` |
|   945322 |  4190 | `		VmPopOperand(&pTos,1);` |
|   472682 |  4191 | `	}` |
|  1192836 |  4192 | `	break;` |
|        - |  4193 | `/*` |
|        - |  4194 | ` * JNZ: P1 P2 *` |
|        - |  4195 | ` *` |
|        - |  4196 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4197 | ` * entry in the stack if P1 is zero.` |
|        - |  4198 | ` */` |
|    61356 |  4199 | `case PH7_OP_JNZ:` |
|        - |  4200 | `#ifdef UNTRUST` |
|        - |  4201 | `	if( pTos < pStack ){` |
|        - |  4202 | `		goto Abort;` |
|        - |  4203 | `	}` |
|        - |  4204 | `#endif` |
|        - |  4205 | `	/* Get a boolean value */` |
|   122714 |  4206 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4207 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4208 | `	}` |
|   122714 |  4209 | `	if( pTos->x.iVal ){` |
|        - |  4210 | `		/* Take the jump */` |
|     5586 |  4211 | `		pc = pInstr->iP2 - 1;` |
|     2792 |  4212 | `	}` |
|   122714 |  4213 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4214 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4215 | `	}` |
|   122714 |  4216 | `	break;` |
|        - |  4217 | `/*` |
|        - |  4218 | ` * NOOP: * * *` |
|        - |  4219 | ` *` |
|        - |  4220 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4221 | ` * destination.` |
|        - |  4222 | ` */` |
|      ! 0 |  4223 | `case PH7_OP_NOOP:` |
|      ! 0 |  4224 | `	break;` |
|        - |  4225 | `/*` |
|        - |  4226 | ` * POP: P1 * *` |
|        - |  4227 | ` *` |
|        - |  4228 | ` * Pop P1 elements from the operand stack.` |
|        - |  4229 | ` */` |
|   462346 |  4230 | `case PH7_OP_POP: {` |
|   924738 |  4231 | `	sxi32 n = pInstr->iP1;` |
|   924738 |  4232 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4233 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4234 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4235 | `	}` |
|   924738 |  4236 | `	VmPopOperand(&pTos,n);` |
|   924738 |  4237 | `	break;` |
|        - |  4238 | `				 }` |
|        - |  4239 | `/*` |
|        - |  4240 | ` * DUP: * * *` |
|        - |  4241 | ` *` |
|        - |  4242 | ` * Duplicate the top of the stack.` |
|        - |  4243 | ` */` |
|       41 |  4244 | `case PH7_OP_DUP:` |
|        - |  4245 | `#ifdef UNTRUST` |
|        - |  4246 | `	if( pTos < pStack ){` |
|        - |  4247 | `		goto Abort;` |
|        - |  4248 | `	}` |
|        - |  4249 | `#endif` |
|       84 |  4250 | `	pTos++;` |
|       84 |  4251 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4252 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4253 | `	break;` |
|        - |  4254 | `/*` |
|        - |  4255 | ` * NSSWITCH: * * P3` |
|        - |  4256 | ` *` |
|        - |  4257 | ` * Switch the active namespace at runtime.` |
|        - |  4258 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4259 | ` */` |
|     7810 |  4260 | `case PH7_OP_NSSWITCH:` |
|    15622 |  4261 | `	SyBlobReset(&pVm->sNamespace);` |
|    15622 |  4262 | `	if( pInstr->p3 ){` |
|      100 |  4263 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4264 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4265 | `	}` |
|        - |  4266 | `	/* Clear namespace-scoped use-const imports */` |
|    15622 |  4267 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15622 |  4268 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15622 |  4269 | `	break;` |
|        - |  4270 | `/* OP_USECONST P1 * P3` |
|        - |  4271 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4272 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4273 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4274 | ` */` |
|        7 |  4275 | `case PH7_OP_USECONST: {` |
|       16 |  4276 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4277 | `	if( azPair ){` |
|       16 |  4278 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4279 | `	}` |
|       16 |  4280 | `	break;` |
|        - |  4281 | `				}` |
|        - |  4282 | `/*` |
|        - |  4283 | ` * CVT_INT: * * *` |
|        - |  4284 | ` *` |
|        - |  4285 | ` * Force the top of the stack to be an integer.` |
|        - |  4286 | ` */` |
|       80 |  4287 | `case PH7_OP_CVT_INT:` |
|        - |  4288 | `#ifdef UNTRUST` |
|        - |  4289 | `	if( pTos < pStack ){` |
|        - |  4290 | `		goto Abort;` |
|        - |  4291 | `	}` |
|        - |  4292 | `#endif` |
|      162 |  4293 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4294 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4295 | `	}` |
|        - |  4296 | `	/* Invalidate any prior representation */` |
|      162 |  4297 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4298 | `	break;` |
|        - |  4299 | `/*` |
|        - |  4300 | ` * CVT_REAL: * * *` |
|        - |  4301 | ` *` |
|        - |  4302 | ` * Force the top of the stack to be a real.` |
|        - |  4303 | ` */` |
|        5 |  4304 | `case PH7_OP_CVT_REAL:` |
|        - |  4305 | `#ifdef UNTRUST` |
|        - |  4306 | `	if( pTos < pStack ){` |
|        - |  4307 | `		goto Abort;` |
|        - |  4308 | `	}` |
|        - |  4309 | `#endif` |
|       11 |  4310 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4311 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4312 | `	}` |
|        - |  4313 | `	/* Invalidate any prior representation */` |
|       11 |  4314 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4315 | `	break;` |
|        - |  4316 | `/*` |
|        - |  4317 | ` * CVT_STR: * * *` |
|        - |  4318 | ` *` |
|        - |  4319 | ` * Force the top of the stack to be a string.` |
|        - |  4320 | ` */` |
|      163 |  4321 | `case PH7_OP_CVT_STR:` |
|        - |  4322 | `#ifdef UNTRUST` |
|        - |  4323 | `	if( pTos < pStack ){` |
|        - |  4324 | `		goto Abort;` |
|        - |  4325 | `	}` |
|        - |  4326 | `#endif` |
|      328 |  4327 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4328 | `		PH7_MemObjToString(pTos);` |
|      153 |  4329 | `	}` |
|      328 |  4330 | `	break;` |
|        - |  4331 | `/*` |
|        - |  4332 | ` * CVT_BOOL: * * *` |
|        - |  4333 | ` *` |
|        - |  4334 | ` * Force the top of the stack to be a boolean.` |
|        - |  4335 | ` */` |
|        5 |  4336 | `case PH7_OP_CVT_BOOL:` |
|        - |  4337 | `#ifdef UNTRUST` |
|        - |  4338 | `	if( pTos < pStack ){` |
|        - |  4339 | `		goto Abort;` |
|        - |  4340 | `	}` |
|        - |  4341 | `#endif` |
|       11 |  4342 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4343 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4344 | `	}` |
|       11 |  4345 | `	break;` |
|        - |  4346 | `/*` |
|        - |  4347 | ` * CVT_NULL: * * *` |
|        - |  4348 | ` *` |
|        - |  4349 | ` * Nullify the top of the stack.` |
|        - |  4350 | ` */` |
|        3 |  4351 | `case PH7_OP_CVT_NULL:` |
|        - |  4352 | `#ifdef UNTRUST` |
|        - |  4353 | `	if( pTos < pStack ){` |
|        - |  4354 | `		goto Abort;` |
|        - |  4355 | `	}` |
|        - |  4356 | `#endif` |
|        7 |  4357 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4358 | `	break;` |
|        - |  4359 | `/*` |
|        - |  4360 | ` * CVT_NUMC: * * *` |
|        - |  4361 | ` *` |
|        - |  4362 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4363 | ` */` |
|      ! 0 |  4364 | `case PH7_OP_CVT_NUMC:` |
|        - |  4365 | `#ifdef UNTRUST` |
|        - |  4366 | `	if( pTos < pStack ){` |
|        - |  4367 | `		goto Abort;` |
|        - |  4368 | `	}` |
|        - |  4369 | `#endif` |
|        - |  4370 | `	/* Force a numeric cast */` |
|      ! 0 |  4371 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4372 | `	break;` |
|        - |  4373 | `/*` |
|        - |  4374 | ` * CVT_ARRAY: * * *` |
|        - |  4375 | ` *` |
|        - |  4376 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4377 | ` */` |
|       10 |  4378 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4379 | `#ifdef UNTRUST` |
|        - |  4380 | `	if( pTos < pStack ){` |
|        - |  4381 | `		goto Abort;` |
|        - |  4382 | `	}` |
|        - |  4383 | `#endif` |
|        - |  4384 | `	/* Force a hashmap cast */` |
|       21 |  4385 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4386 | `	if( rc != SXRET_OK ){` |
|        - |  4387 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4388 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4389 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4390 | `	}` |
|       21 |  4391 | `	break;` |
|        - |  4392 | `/*` |
|        - |  4393 | ` * CVT_OBJ: * * *` |
|        - |  4394 | ` *` |
|        - |  4395 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4396 | ` */` |
|        8 |  4397 | `case PH7_OP_CVT_OBJ:` |
|        - |  4398 | `#ifdef UNTRUST` |
|        - |  4399 | `	if( pTos < pStack ){` |
|        - |  4400 | `		goto Abort;` |
|        - |  4401 | `	}` |
|        - |  4402 | `#endif` |
|       17 |  4403 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4404 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4405 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4406 | `	}` |
|       17 |  4407 | `	break;` |
|        - |  4408 | `/*` |
|        - |  4409 | ` * ERR_CTRL * * *` |
|        - |  4410 | ` *` |
|        - |  4411 | ` * Error control operator.` |
|        - |  4412 | ` */` |
|    16032 |  4413 | `case PH7_OP_ERR_CTRL:` |
|        - |  4414 | `	/*` |
|        - |  4415 | `	 * TICKET 1433-038:` |
|        - |  4416 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4417 | `	 * use the public API,to control error output.` |
|        - |  4418 | `	 */` |
|    32064 |  4419 | `	break;` |
|        - |  4420 | `/*` |
|        - |  4421 | ` * IS_A * * *` |
|        - |  4422 | ` *` |
|        - |  4423 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4424 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4425 | ` * holding a class name or an object).` |
|        - |  4426 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4427 | ` */` |
|       66 |  4428 | `case PH7_OP_IS_A:{` |
|      134 |  4429 | `	ph7_value *pNos = &pTos[-1];` |
|      134 |  4430 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4431 | `#ifdef UNTRUST` |
|        - |  4432 | `	if( pNos < pStack ){` |
|        - |  4433 | `		goto Abort;` |
|        - |  4434 | `	}` |
|        - |  4435 | `#endif` |
|      134 |  4436 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      132 |  4437 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      132 |  4438 | `		ph7_class *pClass = 0;` |
|        - |  4439 | `		/* Extract the target class */` |
|      132 |  4440 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4441 | `			/* Instance already loaded */` |
|      ! 0 |  4442 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      132 |  4443 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      132 |  4444 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      132 |  4445 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4446 | `			/* Handle self/static/parent keywords */` |
|      132 |  4447 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4448 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      130 |  4449 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4450 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      129 |  4451 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4452 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4453 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4454 | `					pClass = pSelf->pBase;` |
|        2 |  4455 | `				}` |
|        3 |  4456 | `			}else{` |
|      122 |  4457 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4458 | `			}` |
|       65 |  4459 | `		}` |
|      132 |  4460 | `		if( pClass ){` |
|        - |  4461 | `			/* Perform the query */` |
|      132 |  4462 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       65 |  4463 | `		}` |
|       65 |  4464 | `	}` |
|        - |  4465 | `	/* Push result */` |
|      134 |  4466 | `	VmPopOperand(&pTos,1);` |
|      134 |  4467 | `	PH7_MemObjRelease(pTos);` |
|      134 |  4468 | `	pTos->x.iVal = iRes;` |
|      134 |  4469 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      134 |  4470 | `	break;` |
|        - |  4471 | `				 }` |
|        - |  4472 |  |
|        - |  4473 | `/*` |
|        - |  4474 | ` * LOADC P1 P2 *` |
|        - |  4475 | ` *` |
|        - |  4476 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4477 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4478 | ` */` |
|  1013639 |  4479 | `case PH7_OP_LOADC: {` |
|        - |  4480 | `	ph7_value *pObj;` |
|        - |  4481 | `	/* Reserve a room */` |
|  2027324 |  4482 | `	pTos++;` |
|  3031158 |  4483 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2027324 |  4484 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4485 | `			SyHashEntry *pEntry;` |
|        - |  4486 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4487 | `			{` |
|        - |  4488 | `				SyHashEntry *pConstImport;` |
|    29555 |  4489 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19702 |  4490 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19704 |  4491 | `				if( pConstImport ){` |
|       11 |  4492 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4493 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4494 | `					if( pEntry ){` |
|       11 |  4495 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4496 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4497 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4498 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4499 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4500 | `						break;` |
|        - |  4501 | `					}` |
|        - |  4502 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4503 | `				}` |
|        - |  4504 | `			}` |
|        - |  4505 | `			/* Candidate for expansion via user defined callbacks */` |
|    19694 |  4506 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19694 |  4507 | `			if( pEntry ){` |
|    19688 |  4508 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4509 | `				/* Set a NULL default value */` |
|    19688 |  4510 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19688 |  4511 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4512 | `				/* Invoke the callback and deal with the expanded value */` |
|    19688 |  4513 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4514 | `				/* Mark as constant */` |
|    19688 |  4515 | `				pTos->nIdx = SXU32_HIGH;` |
|    19688 |  4516 | `				break;` |
|        - |  4517 | `			}` |
|        - |  4518 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4519 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4520 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4521 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4522 | `			{` |
|        8 |  4523 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4524 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4525 | `				sxu32 j;` |
|        8 |  4526 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4527 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4528 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4529 | `				}` |
|        8 |  4530 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4531 | `					/* Try current_namespace\name */` |
|      ! 0 |  4532 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4533 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4534 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4535 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4536 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4537 | `					if( pEntry ){` |
|      ! 0 |  4538 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4539 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4540 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4541 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4542 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4543 | `						break;` |
|        - |  4544 | `					}` |
|        - |  4545 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4546 | `				}` |
|        8 |  4547 | `				if( isQualified ){` |
|        - |  4548 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4549 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4550 | `					SyBlob sErr;` |
|        3 |  4551 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4552 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4553 | `					if( pErrFile ){` |
|        3 |  4554 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4555 | `					}` |
|        3 |  4556 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4557 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4558 | `					SyBlobRelease(&sErr);` |
|        3 |  4559 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4560 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4561 | `					goto LoadC_Done;` |
|        - |  4562 | `				}` |
|        - |  4563 | `			}` |
|        2 |  4564 | `		}` |
|  2007626 |  4565 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1003836 |  4566 | `	}else{` |
|        - |  4567 | `		/* Set a NULL value */` |
|      ! 0 |  4568 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4569 | `	}` |
|  1003791 |  4570 | `LoadC_Done:` |
|        - |  4571 | `	/* Mark as constant */` |
|  2007628 |  4572 | `	pTos->nIdx = SXU32_HIGH;` |
|  2007628 |  4573 | `	break;` |
|        - |  4574 | `				  }` |
|        - |  4575 | `/*` |
|        - |  4576 | ` * LOAD: P1 * P3` |
|        - |  4577 | ` *` |
|        - |  4578 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4579 | ` * from the P3 operand.` |
|        - |  4580 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4581 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4582 | ` */` |
|  1574940 |  4583 | `case PH7_OP_LOAD:{` |
|        - |  4584 | `	ph7_value *pObj;` |
|        - |  4585 | `	SyString sName;` |
|  3150102 |  4586 | `	if( pInstr->p3 == 0 ){` |
|        - |  4587 | `		/* Take the variable name from the top of the stack */` |
|        - |  4588 | `#ifdef UNTRUST` |
|        - |  4589 | `		if( pTos < pStack ){` |
|        - |  4590 | `			goto Abort;` |
|        - |  4591 | `		}` |
|        - |  4592 | `#endif` |
|        - |  4593 | `		/* Force a string cast */` |
|       19 |  4594 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4595 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4596 | `		}` |
|       19 |  4597 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4598 | `	}else{` |
|  3150084 |  4599 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4600 | `		/* Reserve a room for the target object */` |
|  3150084 |  4601 | `		pTos++;` |
|        - |  4602 | `	}` |
|        - |  4603 | `	/* Extract the requested memory object */` |
|  3150102 |  4604 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3150102 |  4605 | `	if( pObj == 0 ){` |
|      836 |  4606 | `		if( pInstr->iP1 ){` |
|        - |  4607 | `			/* Variable not found,load NULL */` |
|      836 |  4608 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4609 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4610 | `			}else{` |
|      836 |  4611 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4612 | `			}` |
|      836 |  4613 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1575359 |  4614 | `			break;` |
|      ! 0 |  4615 | `		}else{` |
|        - |  4616 | `			/* Fatal error */` |
|      ! 0 |  4617 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4618 | `			goto Abort;` |
|        - |  4619 | `		}` |
|        - |  4620 | `	}` |
|        - |  4621 | `	/* Load variable contents */` |
|  3149268 |  4622 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3149268 |  4623 | `	pTos->nIdx = pObj->nIdx;` |
|  3149268 |  4624 | `	break;` |
|        - |  4625 | `				   }` |
|        - |  4626 | `/*` |
|        - |  4627 | ` * LOAD_MAP P1 * *` |
|        - |  4628 | ` *` |
|        - |  4629 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4630 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4631 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4632 | ` */` |
|    22772 |  4633 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4634 | `	ph7_hashmap *pMap;` |
|        - |  4635 | `	/* Allocate a new hashmap instance */` |
|    45546 |  4636 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45546 |  4637 | `	if( pMap == 0 ){` |
|      ! 0 |  4638 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4639 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4640 | `		goto Abort;` |
|        - |  4641 | `	}` |
|    45546 |  4642 | `	if( pInstr->iP1 > 0 ){` |
|     2780 |  4643 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2780 |  4644 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4645 | `		/* Perform the insertion */` |
|     8446 |  4646 | `		while( pEntry < pTos ){` |
|     5684 |  4647 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4648 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4649 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4650 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  4651 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  4652 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  4653 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4654 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4655 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4656 | `						 * map dangling. */` |
|      ! 0 |  4657 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4658 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4659 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4660 | `						break;` |
|        - |  4661 | `					}` |
|       27 |  4662 | `				}else{` |
|        - |  4663 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  4664 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  4665 | `					break;` |
|        1 |  4666 | `				}` |
|     5642 |  4667 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4668 | `				/* Insertion by reference */` |
|      151 |  4669 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  4670 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  4671 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4672 | `					);` |
|       51 |  4673 | `			}else{` |
|        - |  4674 | `				/* Standard insertion */` |
|     8273 |  4675 | `				PH7_HashmapInsert(pMap,` |
|     5514 |  4676 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2757 |  4677 | `					&pEntry[1]` |
|        - |  4678 | `				);` |
|        - |  4679 | `			}` |
|        - |  4680 | `			/* Next pair on the stack */` |
|     5668 |  4681 | `			pEntry += 2;` |
|        2 |  4682 | `		}` |
|        - |  4683 | `		/* Pop P1 elements */` |
|     2780 |  4684 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2780 |  4685 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4686 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  4687 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  4688 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4689 | `				goto Abort;` |
|        - |  4690 | `			}` |
|        - |  4691 | `			{` |
|       17 |  4692 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  4693 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  4694 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  4695 | `					break;` |
|        - |  4696 | `				}` |
|        - |  4697 | `			}` |
|       15 |  4698 | `			goto Exception;` |
|        - |  4699 | `		}` |
|     1381 |  4700 | `	}` |
|        - |  4701 | `	/* Push the hashmap */` |
|    45530 |  4702 | `	pTos++;` |
|    45530 |  4703 | `	pTos->nIdx = SXU32_HIGH;` |
|    45530 |  4704 | `	pTos->x.pOther = pMap;` |
|    45530 |  4705 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45530 |  4706 | `	break;` |
|        - |  4707 | `					  }` |
|        - |  4708 | `/*` |
|        - |  4709 | ` * LOAD_LIST: P1 * *` |
|        - |  4710 | ` *` |
|        - |  4711 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4712 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4713 | ` * Caveats:` |
|        - |  4714 | ` *  This implementation support only a single nesting level.` |
|        - |  4715 | ` */` |
|       48 |  4716 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4717 | `	ph7_value *pEntry;` |
|       98 |  4718 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4719 | `		/* Empty list,break immediately */` |
|      ! 0 |  4720 | `		break;` |
|        - |  4721 | `	}` |
|       98 |  4722 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4723 | `#ifdef UNTRUST` |
|        - |  4724 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4725 | `		goto Abort;` |
|        - |  4726 | `	}` |
|        - |  4727 | `#endif` |
|       98 |  4728 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4729 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4730 | `		ph7_hashmap_node *pNode;` |
|        - |  4731 | `		ph7_value sKey,*pObj;` |
|        - |  4732 | `		/* Start Copying */` |
|       91 |  4733 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4734 | `		while( pEntry <= pTos ){` |
|      193 |  4735 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4736 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4737 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4738 | `					if( rc == SXRET_OK ){` |
|        - |  4739 | `						/* Store node value */` |
|      165 |  4740 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4741 | `					}else{` |
|        - |  4742 | `						/* Undefined array key */` |
|        - |  4743 | `						char zMsg[128];` |
|      ! 0 |  4744 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4745 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4746 | `						PH7_MemObjRelease(pObj);` |
|        - |  4747 | `					}` |
|       82 |  4748 | `				}` |
|       82 |  4749 | `			}` |
|      193 |  4750 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4751 | `			pEntry++;` |
|        1 |  4752 | `		}` |
|       46 |  4753 | `	}else{` |
|        - |  4754 | `		/* Source is not an array */` |
|        - |  4755 | `		ph7_value *pObj;` |
|       18 |  4756 | `		while( pEntry <= pTos ){` |
|       12 |  4757 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4758 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4759 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4760 | `				}` |
|        5 |  4761 | `			}` |
|       12 |  4762 | `			pEntry++;` |
|        2 |  4763 | `		}` |
|        8 |  4764 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4765 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4766 | `			const char *zType = "unknown";` |
|        3 |  4767 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4768 | `			char zMsg[256];` |
|        3 |  4769 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4770 | `				zType = "string";` |
|        1 |  4771 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4772 | `				zType = "int";` |
|      ! 0 |  4773 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4774 | `				zType = "float";` |
|      ! 0 |  4775 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4776 | `				zType = "object";` |
|      ! 0 |  4777 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4778 | `				zType = "resource";` |
|      ! 0 |  4779 | `			}` |
|        3 |  4780 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4781 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4782 | `		}` |
|        - |  4783 | `	}` |
|       98 |  4784 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4785 | `	break;` |
|        - |  4786 | `					   }` |
|        - |  4787 | `/*` |
|        - |  4788 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4789 | ` *` |
|        - |  4790 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4791 | ` * from the stack.` |
|        - |  4792 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4793 | ` * instead.` |
|        - |  4794 | ` */` |
|   250617 |  4795 | `case PH7_OP_LOAD_IDX: {` |
|   501280 |  4796 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   501280 |  4797 | `	ph7_hashmap *pMap = 0;` |
|        - |  4798 | `	ph7_value *pIdx;` |
|   501280 |  4799 | `	pIdx = 0;` |
|   501280 |  4800 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4801 | `		if( !pInstr->iP2){` |
|        - |  4802 | `			/* No available index,load NULL */` |
|      ! 0 |  4803 | `			if( pTos >= pStack ){` |
|      ! 0 |  4804 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4805 | `			}else{` |
|        - |  4806 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4807 | `				pTos++;` |
|      ! 0 |  4808 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4809 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4810 | `			}` |
|        - |  4811 | `			/* Emit a notice */` |
|      ! 0 |  4812 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4813 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4814 | `			break;` |
|        - |  4815 | `		}` |
|      ! 0 |  4816 | `	}else{` |
|   501280 |  4817 | `		pIdx = pTos;` |
|   501280 |  4818 | `		pTos--;` |
|        - |  4819 | `	}` |
|   501280 |  4820 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4821 | `		/* String access */` |
|   387716 |  4822 | `		if( pIdx ){` |
|        - |  4823 | `			sxu32 nOfft;` |
|   387716 |  4824 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4825 | `				/* Force an int cast */` |
|      ! 0 |  4826 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4827 | `			}` |
|   387716 |  4828 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  4829 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4830 | `				/* Invalid offset,load null */` |
|      ! 0 |  4831 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4832 | `			}else{` |
|   387716 |  4833 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  4834 | `				int c = zData[nOfft];` |
|   387716 |  4835 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  4836 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  4837 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4838 | `			}` |
|   193881 |  4839 | `		}else{` |
|        - |  4840 | `			/* No available index,load NULL */` |
|      ! 0 |  4841 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4842 | `		}` |
|   387716 |  4843 | `		break;` |
|        - |  4844 | `	}` |
|   113566 |  4845 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4846 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  4847 | `		 * iP2 codes:` |
|        - |  4848 | `		 *   0 = read       → offsetGet` |
|        - |  4849 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  4850 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  4851 | `		 *   4 = isset()    → offsetExists` |
|        - |  4852 | `		 *   5 = unset()    → offsetUnset` |
|        - |  4853 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  4854 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  4855 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  4856 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  4857 | `			ph7_class_method *pMeth;` |
|        - |  4858 | `			ph7_value sResult;` |
|        - |  4859 | `			ph7_value *apArg[1];` |
|      124 |  4860 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  4861 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  4862 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4863 | `					"Cannot use [] for reading");` |
|      ! 0 |  4864 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4865 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4866 | `				break;` |
|        - |  4867 | `			}` |
|      124 |  4868 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  4869 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  4870 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  4871 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4872 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  4873 | `				apArg[0] = pIdx;` |
|       51 |  4874 | `				if( pMeth ){` |
|       51 |  4875 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  4876 | `				}` |
|       99 |  4877 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  4878 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4879 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  4880 | `				apArg[0] = pIdx;` |
|        9 |  4881 | `				if( pMeth ){` |
|        9 |  4882 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  4883 | `				}` |
|        5 |  4884 | `			}else{` |
|       66 |  4885 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4886 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  4887 | `				apArg[0] = pIdx;` |
|       66 |  4888 | `				if( pMeth ){` |
|       66 |  4889 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  4890 | `				}` |
|        - |  4891 | `			}` |
|      124 |  4892 | `			if( pInstr->iP2 == 4 ){` |
|        - |  4893 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  4894 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  4895 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  4896 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  4897 | `				PH7_MemObjRelease(pTos);` |
|       33 |  4898 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  4899 | `				if( bExists ){` |
|       17 |  4900 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  4901 | `					pTos->x.iVal = 1;` |
|        9 |  4902 | `				}else{` |
|       17 |  4903 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  4904 | `				}` |
|      108 |  4905 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  4906 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  4907 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  4908 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4909 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4910 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  4911 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  4912 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  4913 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  4914 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  4915 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  4916 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  4917 | `				PH7_MemObjRelease(pTos);` |
|       11 |  4918 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  4919 | `				if( !bExists ){` |
|        3 |  4920 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  4921 | `				}else{` |
|        9 |  4922 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4923 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  4924 | `					ph7_value sValue;` |
|        9 |  4925 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4926 | `					apArg[0] = pIdx;` |
|        9 |  4927 | `					if( pGet ){` |
|        9 |  4928 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  4929 | `					}` |
|        9 |  4930 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  4931 | `					PH7_MemObjRelease(&sValue);` |
|        - |  4932 | `				}` |
|       11 |  4933 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  4934 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  4935 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  4936 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  4937 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  4938 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  4939 | `				 *     and push NULL.` |
|        - |  4940 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  4941 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  4942 | `				int bShouldArm = !bExists;` |
|        - |  4943 | `				ph7_value sValue;` |
|        9 |  4944 | `				PH7_MemObjRelease(&sResult);` |
|        - |  4945 | `				/* Reset any prior arming defensively */` |
|        9 |  4946 | `				VmCoalesceDisarm(pVm);` |
|        9 |  4947 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4948 | `				if( bExists ){` |
|        5 |  4949 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4950 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  4951 | `					apArg[0] = pIdx;` |
|        5 |  4952 | `					if( pGet ){` |
|        5 |  4953 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  4954 | `					}` |
|        5 |  4955 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  4956 | `						bShouldArm = 1;` |
|        1 |  4957 | `					}` |
|        2 |  4958 | `				}` |
|        9 |  4959 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4960 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4961 | `				if( bShouldArm ){` |
|        - |  4962 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  4963 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  4964 | `					 * intervening expression evaluation. */` |
|        7 |  4965 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  4966 | `					if( pIdx ){` |
|        7 |  4967 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  4968 | `					}` |
|        7 |  4969 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  4970 | `					pInst->iRef++;` |
|        7 |  4971 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  4972 | `				}else{` |
|        3 |  4973 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  4974 | `				}` |
|        9 |  4975 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  4976 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  4977 | `				break;` |
|      ! 0 |  4978 | `			}else{` |
|        - |  4979 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  4980 | `				PH7_MemObjRelease(pTos);` |
|       66 |  4981 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  4982 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4983 | `			}` |
|      106 |  4984 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  4985 | `			if( pIdx ){` |
|      106 |  4986 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  4987 | `			}` |
|      106 |  4988 | `			break;` |
|        - |  4989 | `		}` |
|        - |  4990 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  4991 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  4992 | `		if( pInst ){` |
|        - |  4993 | `			char zMsg[256];` |
|        3 |  4994 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  4995 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  4996 | `				"Cannot use object of type %.*s as array",` |
|        2 |  4997 | `				(int)pName->nByte,pName->zString);` |
|        3 |  4998 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  4999 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5000 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5001 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5002 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5003 | `			break;` |
|        - |  5004 | `		}` |
|      ! 0 |  5005 | `	}` |
|   113442 |  5006 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5007 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5008 | `			ph7_value *pObj;` |
|        3 |  5009 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5010 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5011 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5012 | `			}` |
|        1 |  5013 | `		}` |
|        1 |  5014 | `	}` |
|   113442 |  5015 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   113442 |  5016 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   113442 |  5017 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5018 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5019 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5020 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5021 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5022 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5023 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  5024 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  5025 | `		}` |
|        - |  5026 | `		/* Point to the hashmap */` |
|   113442 |  5027 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   113442 |  5028 | `		if( pIdx ){` |
|        - |  5029 | `			/* Load the desired entry */` |
|   113442 |  5030 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56720 |  5031 | `		}` |
|   113442 |  5032 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5033 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5034 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5035 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5036 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5037 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5038 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5039 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5040 | `			 * correct for the outermost write. */` |
|       19 |  5041 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5042 | `			if( !needWrite && pNode ){` |
|       13 |  5043 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5044 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5045 | `					needWrite = 1;` |
|        3 |  5046 | `				}` |
|        6 |  5047 | `			}` |
|       19 |  5048 | `			if( needWrite ){` |
|       13 |  5049 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5050 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5051 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5052 | `					 * into the new map's storage. */` |
|        7 |  5053 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5054 | `					if( pIdx ){` |
|        7 |  5055 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5056 | `					}` |
|        3 |  5057 | `				}` |
|        6 |  5058 | `			}` |
|        9 |  5059 | `		}` |
|   113442 |  5060 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5061 | `			/* Create a new empty entry */` |
|      273 |  5062 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5063 | `			if( rc == SXRET_OK ){` |
|        - |  5064 | `				/* Point to the last inserted entry */` |
|      273 |  5065 | `				pNode = pMap->pLast;` |
|      136 |  5066 | `			}` |
|      136 |  5067 | `		}` |
|    56720 |  5068 | `	}` |
|   113442 |  5069 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5070 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5071 | `		char zMsg[128];` |
|      ! 0 |  5072 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5073 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5074 | `		}` |
|      ! 0 |  5075 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5076 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5077 | `	}` |
|   113442 |  5078 | `	if( pIdx ){` |
|   113442 |  5079 | `		PH7_MemObjRelease(pIdx);` |
|    56720 |  5080 | `	}` |
|   113442 |  5081 | `	if( rc == SXRET_OK ){` |
|        - |  5082 | `		/* Load entry contents */` |
|    50304 |  5083 | `		if( pMap->iRef < 2 ){` |
|        - |  5084 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5085 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5086 | `			 */` |
|       28 |  5087 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5088 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5089 | `		}else{` |
|    50278 |  5090 | `			pTos->nIdx = pNode->nValIdx;` |
|    50278 |  5091 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50278 |  5092 | `			PH7_HashmapUnref(pMap);` |
|        - |  5093 | `		}` |
|    25153 |  5094 | `	}else{` |
|        - |  5095 | `		/* No such entry,load NULL */` |
|    63140 |  5096 | `		PH7_MemObjRelease(pTos);` |
|    63140 |  5097 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5098 | `	}` |
|   113442 |  5099 | `	break;` |
|        - |  5100 | `					  }` |
|        - |  5101 | `/*` |
|        - |  5102 | ` * LOAD_CLOSURE * * P3` |
|        - |  5103 | ` *` |
|        - |  5104 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5105 | ` * name in the stack.` |
|        - |  5106 | ` */` |
|       61 |  5107 | `case PH7_OP_LOAD_CLOSURE:{` |
|      124 |  5108 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      124 |  5109 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5110 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5111 | `		ph7_vm_func *pClosure;` |
|        - |  5112 | `		char *zName;` |
|        - |  5113 | `		sxu32 mLen;` |
|        - |  5114 | `		sxu32 n;` |
|        - |  5115 | `		/* Create a new VM function */` |
|      124 |  5116 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5117 | `		/* Generate an unique closure name */` |
|      124 |  5118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      124 |  5119 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5120 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5121 | `			goto Abort;` |
|        - |  5122 | `		}` |
|      124 |  5123 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      124 |  5124 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5125 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5126 | `		}` |
|        - |  5127 | `		/* Zero the stucture */` |
|      124 |  5128 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5129 | `		/* Perform a structure assignment on read-only items */` |
|      124 |  5130 | `		pClosure->aArgs = pFunc->aArgs;` |
|      124 |  5131 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      124 |  5132 | `		pClosure->aStatic = pFunc->aStatic;` |
|      124 |  5133 | `		pClosure->iFlags = pFunc->iFlags;` |
|      124 |  5134 | `		pClosure->pUserData = pFunc->pUserData;` |
|      124 |  5135 | `		pClosure->sSignature = pFunc->sSignature;` |
|      124 |  5136 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      124 |  5137 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      124 |  5138 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      124 |  5139 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      124 |  5140 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5141 | `		/* Register the closure */` |
|      124 |  5142 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5143 | `		/* Set up closure environment */` |
|      124 |  5144 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      124 |  5145 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      312 |  5146 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5147 | `			ph7_value *pValue;` |
|      190 |  5148 | `			pEnv = &aEnv[n];` |
|      190 |  5149 | `			sEnv.sName  = pEnv->sName;` |
|      190 |  5150 | `			sEnv.iFlags = pEnv->iFlags;` |
|      190 |  5151 | `			sEnv.nIdx = SXU32_HIGH;` |
|      190 |  5152 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      190 |  5153 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5154 | `				/* Pass by reference */` |
|      ! 0 |  5155 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5156 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5157 | `					);` |
|      ! 0 |  5158 | `			}` |
|        - |  5159 | `			/* Standard pass by value */` |
|      190 |  5160 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      190 |  5161 | `			if( pValue ){` |
|        - |  5162 | `				/* Copy imported value */` |
|       72 |  5163 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5164 | `			}` |
|        - |  5165 | `			/* Insert the imported variable */` |
|      190 |  5166 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       96 |  5167 | `		}` |
|        - |  5168 | `		/* Finally,load the closure name on the stack */` |
|      124 |  5169 | `		pTos++;` |
|      124 |  5170 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       61 |  5171 | `	}` |
|      124 |  5172 | `	break;` |
|        - |  5173 | `						 }` |
|        - |  5174 | `/*` |
|        - |  5175 | ` * STORE * P2 P3` |
|        - |  5176 | ` *` |
|        - |  5177 | ` * Perform a store (Assignment) operation.` |
|        - |  5178 | ` */` |
|   145880 |  5179 | `case PH7_OP_STORE: {` |
|        - |  5180 | `	ph7_value *pObj;` |
|        - |  5181 | `	SyString sName;` |
|        - |  5182 | `#ifdef UNTRUST` |
|        - |  5183 | `	if( pTos < pStack ){` |
|        - |  5184 | `		goto Abort;` |
|        - |  5185 | `	}` |
|        - |  5186 | `#endif` |
|   291762 |  5187 | `	if( pInstr->iP2 ){` |
|        - |  5188 | `		sxu32 nIdx;` |
|        - |  5189 | `		sxi32 rcT;` |
|        - |  5190 | `		/* Member store operation */` |
|     5168 |  5191 | `		nIdx = pTos->nIdx;` |
|     5168 |  5192 | `		VmPopOperand(&pTos,1);` |
|     5168 |  5193 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5194 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5195 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5196 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5197 | `		}else{` |
|        - |  5198 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5199 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5164 |  5200 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5164 |  5201 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5202 | `				goto Abort;` |
|        - |  5203 | `			}` |
|     5164 |  5204 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5205 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5206 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5207 | `				 * propagate out of the VM loop. */` |
|       37 |  5208 | `				VmPopOperand(&pTos,1);` |
|        - |  5209 | `				{` |
|       37 |  5210 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5211 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5212 | `						pc = pFrm2->iExceptionJump - 1;` |
|   145899 |  5213 | `						break;` |
|        - |  5214 | `					}` |
|        - |  5215 | `				}` |
|      ! 0 |  5216 | `				goto Exception;` |
|        - |  5217 | `			}` |
|        - |  5218 | `			/* Point to the desired memory object */` |
|     5128 |  5219 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5128 |  5220 | `			if( pObj ){` |
|        - |  5221 | `				/* Perform the store operation */` |
|     5128 |  5222 | `				PH7_MemObjStore(pTos,pObj);` |
|     2563 |  5223 | `			}` |
|        - |  5224 | `		}` |
|     5132 |  5225 | `		break;` |
|   286596 |  5226 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5227 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5228 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5229 | `			/* Force a string cast */` |
|      ! 0 |  5230 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5231 | `		}` |
|        7 |  5232 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5233 | `		pTos--;` |
|        - |  5234 | `#ifdef UNTRUST` |
|        - |  5235 | `		if( pTos < pStack  ){` |
|        - |  5236 | `			goto Abort;` |
|        - |  5237 | `		}` |
|        - |  5238 | `#endif` |
|        4 |  5239 | `	}else{` |
|   286590 |  5240 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5241 | `	}` |
|        - |  5242 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   286596 |  5243 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   286596 |  5244 | `	if( pObj == 0 ){` |
|      ! 0 |  5245 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5246 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5247 | `		goto Abort;` |
|        - |  5248 | `	}` |
|   286596 |  5249 | `	if( !pInstr->p3 ){` |
|        7 |  5250 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5251 | `	}` |
|        - |  5252 | `	/* Perform the store operation */` |
|   286596 |  5253 | `	PH7_MemObjStore(pTos,pObj);` |
|   286596 |  5254 | `	break;` |
|        - |  5255 | `				   }` |
|        - |  5256 | `/*` |
|        - |  5257 | ` * STORE_IDX:   P1 * P3` |
|        - |  5258 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5259 | ` *` |
|        - |  5260 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5261 | ` */` |
|    96879 |  5262 | `case PH7_OP_STORE_IDX:` |
|        - |  5263 | `case PH7_OP_STORE_IDX_REF: {` |
|   193760 |  5264 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5265 | `	ph7_value *pKey;` |
|        - |  5266 | `	sxu32 nIdx;` |
|   193760 |  5267 | `	if( pInstr->iP1 ){` |
|        - |  5268 | `		/* Key is next on stack */` |
|    63288 |  5269 | `		pKey = pTos;` |
|    63288 |  5270 | `		pTos--;` |
|    31645 |  5271 | `	}else{` |
|   130474 |  5272 | `		pKey = 0;` |
|        - |  5273 | `	}` |
|   193760 |  5274 | `	nIdx = pTos->nIdx;` |
|        - |  5275 | `	{` |
|        - |  5276 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5277 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5278 | `		 * the backing variable slot at nIdx. */` |
|   193760 |  5279 | `		ph7_class_instance *pInst = 0;` |
|   193760 |  5280 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5281 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   193744 |  5282 | `		}else if( nIdx != SXU32_HIGH ){` |
|   193728 |  5283 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   193728 |  5284 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5285 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5286 | `			}` |
|    96863 |  5287 | `		}` |
|   193760 |  5288 | `		if( pInst ){` |
|       34 |  5289 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5290 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5291 | `				ph7_class_method *pMeth;` |
|        - |  5292 | `				ph7_value sNullKey;` |
|        - |  5293 | `				ph7_value *apArg[2];` |
|       32 |  5294 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5295 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5296 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5297 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5298 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5299 | `					break;` |
|        - |  5300 | `				}` |
|       32 |  5301 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5302 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5303 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5304 | `				VmPopOperand(&pTos,1);` |
|       32 |  5305 | `				if( pKey == 0 ){` |
|        7 |  5306 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5307 | `					apArg[0] = &sNullKey;` |
|        4 |  5308 | `				}else{` |
|       26 |  5309 | `					apArg[0] = pKey;` |
|        - |  5310 | `				}` |
|       32 |  5311 | `				apArg[1] = pTos;` |
|       32 |  5312 | `				if( pMeth ){` |
|       32 |  5313 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5314 | `				}` |
|       32 |  5315 | `				if( pKey ){` |
|       26 |  5316 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5317 | `				}else{` |
|        7 |  5318 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5319 | `				}` |
|        - |  5320 | `				/* Pop the value */` |
|       32 |  5321 | `				VmPopOperand(&pTos,1);` |
|       32 |  5322 | `				break;` |
|        - |  5323 | `			}` |
|        - |  5324 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5325 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5326 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5327 | `			 * a few lines below). Match PHP. */` |
|        - |  5328 | `			{` |
|        - |  5329 | `				char zMsg[256];` |
|        3 |  5330 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5331 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5332 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5333 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5334 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5335 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5336 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5337 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5338 | `				break;` |
|        - |  5339 | `			}` |
|        - |  5340 | `		}` |
|        - |  5341 | `	}` |
|   193728 |  5342 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5343 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5344 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5345 | `		 * checking true sharing count, then re-add after separation. */` |
|   193676 |  5346 | `		if( nIdx != SXU32_HIGH ){` |
|   193676 |  5347 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   290513 |  5348 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   193676 |  5349 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5350 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5351 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5352 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5353 | `				 * refcounts if the backing array was already separated. */` |
|   193676 |  5354 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   193676 |  5355 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   193676 |  5356 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   193676 |  5357 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   193676 |  5358 | `					pTos->x.pOther = pMap;` |
|    96839 |  5359 | `				}else{` |
|        - |  5360 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5361 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5362 | `					pMap = pCur;` |
|        - |  5363 | `				}` |
|    96839 |  5364 | `			}else{` |
|      ! 0 |  5365 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5366 | `			}` |
|    96839 |  5367 | `		}else{` |
|      ! 0 |  5368 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5369 | `		}` |
|   193676 |  5370 | `		if( pMap->iRef < 2 ){` |
|        - |  5371 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5372 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5373 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5374 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5375 | `			pMap->iRef = 2;` |
|      ! 0 |  5376 | `		}` |
|    96839 |  5377 | `	}else{` |
|        - |  5378 | `		ph7_value *pObj;` |
|       53 |  5379 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5380 | `		if( pObj == 0 ){` |
|      ! 0 |  5381 | `			if( pKey ){` |
|      ! 0 |  5382 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5383 | `			}` |
|      ! 0 |  5384 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5385 | `			break;` |
|        - |  5386 | `		}` |
|        - |  5387 | `		/* Phase#1: Load the array */` |
|       53 |  5388 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5389 | `			VmPopOperand(&pTos,1);` |
|       53 |  5390 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5391 | `				/* Force a string cast */` |
|      ! 0 |  5392 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5393 | `			}` |
|       53 |  5394 | `			if( pKey == 0 ){` |
|        - |  5395 | `				/* Append string */` |
|        3 |  5396 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5397 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5398 | `				}` |
|        2 |  5399 | `			}else{` |
|        - |  5400 | `				sxu32 nOfft;` |
|       51 |  5401 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5402 | `					/* Force an int cast */` |
|       51 |  5403 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5404 | `				}` |
|       51 |  5405 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5406 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5407 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5408 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5409 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5410 | `				}else{` |
|      ! 0 |  5411 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5412 | `						/* Perform an append operation */` |
|      ! 0 |  5413 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5414 | `					}` |
|        - |  5415 | `				}` |
|        - |  5416 | `			}` |
|       53 |  5417 | `			if( pKey ){` |
|       51 |  5418 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5419 | `			}` |
|       53 |  5420 | `			break;` |
|      ! 0 |  5421 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5422 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5423 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5424 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5425 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5426 | `				goto Abort;` |
|        - |  5427 | `			}` |
|      ! 0 |  5428 | `		}` |
|        - |  5429 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5430 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5431 | `	}` |
|   193676 |  5432 | `	VmPopOperand(&pTos,1);` |
|        - |  5433 | `	/* Phase#2: Perform the insertion */` |
|   193676 |  5434 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5435 | `		/* Insertion by reference */` |
|       15 |  5436 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5437 | `	}else{` |
|   193662 |  5438 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5439 | `	}` |
|   193676 |  5440 | `	if( pKey ){` |
|    63212 |  5441 | `		PH7_MemObjRelease(pKey);` |
|    31605 |  5442 | `	}` |
|   193676 |  5443 | `	break;` |
|        - |  5444 | `					   }` |
|        - |  5445 | `/*` |
|        - |  5446 | ` * INCR: P1 * *` |
|        - |  5447 | ` *` |
|        - |  5448 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5449 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5450 | ` * the stack and increment after that.` |
|        - |  5451 | ` */` |
|   167681 |  5452 | `case PH7_OP_INCR:` |
|        - |  5453 | `#ifdef UNTRUST` |
|        - |  5454 | `	if( pTos < pStack ){` |
|        - |  5455 | `		goto Abort;` |
|        - |  5456 | `	}` |
|        - |  5457 | `#endif` |
|   335408 |  5458 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335408 |  5459 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5460 | `			ph7_value *pObj;` |
|   335408 |  5461 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335408 |  5462 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5463 | `					/* Perl-style string increment.` |
|        - |  5464 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5465 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5466 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5467 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5468 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5469 | `					}` |
|       49 |  5470 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5471 | `					if( pInstr->iP1 ){` |
|        - |  5472 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5473 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5474 | `					}` |
|       25 |  5475 | `				}else{` |
|        - |  5476 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5477 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5478 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5479 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5480 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5481 | `					 * so its old-value view survives the coercion. */` |
|   335360 |  5482 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5483 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5484 | `					}` |
|        - |  5485 | `					/* Force a numeric cast on the variable */` |
|   335360 |  5486 | `					PH7_MemObjToNumeric(pObj);` |
|   335360 |  5487 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5488 | `						pObj->rVal++;` |
|        - |  5489 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5490 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5491 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5492 | `						 * integer-valued real. */` |
|        9 |  5493 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5494 | `					}else{` |
|   335352 |  5495 | `						pObj->x.iVal++;` |
|        - |  5496 | `					}` |
|   335360 |  5497 | `					if( pInstr->iP1 ){` |
|        - |  5498 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5499 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5500 | `					}` |
|        - |  5501 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5502 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5503 | `				}` |
|   167725 |  5504 | `			}` |
|   167727 |  5505 | `		}else{` |
|      ! 0 |  5506 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5507 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5508 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5509 | `				}else{` |
|        - |  5510 | `					/* Force a numeric cast */` |
|      ! 0 |  5511 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5512 | `					/* Pre-increment */` |
|      ! 0 |  5513 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5514 | `						pTos->rVal++;` |
|        - |  5515 | `						/* Try to get an integer representation */` |
|      ! 0 |  5516 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5517 | `					}else{` |
|      ! 0 |  5518 | `						pTos->x.iVal++;` |
|      ! 0 |  5519 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5520 | `					}` |
|        - |  5521 | `				}` |
|      ! 0 |  5522 | `			}` |
|        - |  5523 | `		}` |
|   167725 |  5524 | `	}` |
|   335408 |  5525 | `	break;` |
|        - |  5526 | `/*` |
|        - |  5527 | ` * DECR: P1 * *` |
|        - |  5528 | ` *` |
|        - |  5529 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5530 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5531 | ` * and decrement after that.` |
|        - |  5532 | ` */` |
|       14 |  5533 | `case PH7_OP_DECR:` |
|        - |  5534 | `#ifdef UNTRUST` |
|        - |  5535 | `	if( pTos < pStack ){` |
|        - |  5536 | `		goto Abort;` |
|        - |  5537 | `	}` |
|        - |  5538 | `#endif` |
|        - |  5539 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5540 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5541 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5542 | `			ph7_value *pObj;` |
|       27 |  5543 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5544 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5545 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5546 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5547 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5548 | `					if( pInstr->iP1 ){` |
|        - |  5549 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5550 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5551 | `					}` |
|        - |  5552 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5553 | `				}else{` |
|        - |  5554 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5555 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5556 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5557 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5558 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5559 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5560 | `					}` |
|       21 |  5561 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5562 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5563 | `						pObj->rVal--;` |
|        - |  5564 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5565 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5566 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5567 | `						 * integer-valued real. */` |
|        9 |  5568 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5569 | `					}else{` |
|       13 |  5570 | `						pObj->x.iVal--;` |
|        - |  5571 | `					}` |
|       21 |  5572 | `					if( pInstr->iP1 ){` |
|        - |  5573 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5574 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5575 | `					}` |
|        - |  5576 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5577 | `				}` |
|       13 |  5578 | `			}` |
|       14 |  5579 | `		}else{` |
|      ! 0 |  5580 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5581 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5582 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5583 | `				}else{` |
|        - |  5584 | `					/* Force a numeric cast */` |
|      ! 0 |  5585 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5586 | `					/* Pre-decrement */` |
|      ! 0 |  5587 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5588 | `						pTos->rVal--;` |
|        - |  5589 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5590 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5591 | `					}else{` |
|      ! 0 |  5592 | `						pTos->x.iVal--;` |
|      ! 0 |  5593 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5594 | `					}` |
|        - |  5595 | `				}` |
|      ! 0 |  5596 | `			}` |
|        - |  5597 | `		}` |
|       13 |  5598 | `	}` |
|       29 |  5599 | `	break;` |
|        - |  5600 | `/*` |
|        - |  5601 | ` * UMINUS: * * *` |
|        - |  5602 | ` *` |
|        - |  5603 | ` * Perform a unary minus operation.` |
|        - |  5604 | ` */` |
|    29683 |  5605 | `case PH7_OP_UMINUS:` |
|        - |  5606 | `#ifdef UNTRUST` |
|        - |  5607 | `	if( pTos < pStack ){` |
|        - |  5608 | `		goto Abort;` |
|        - |  5609 | `	}` |
|        - |  5610 | `#endif` |
|        - |  5611 | `	/* Force a numeric (integer,real or both) cast */` |
|    59368 |  5612 | `	PH7_MemObjToNumeric(pTos);` |
|    59368 |  5613 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5614 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5615 | `	}` |
|    59368 |  5616 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59338 |  5617 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29668 |  5618 | `	}` |
|    59368 |  5619 | `	break;` |
|        - |  5620 | `/*` |
|        - |  5621 | ` * UPLUS: * * *` |
|        - |  5622 | ` *` |
|        - |  5623 | ` * Perform a unary plus operation.` |
|        - |  5624 | ` */` |
|       18 |  5625 | `case PH7_OP_UPLUS:` |
|        - |  5626 | `#ifdef UNTRUST` |
|        - |  5627 | `	if( pTos < pStack ){` |
|        - |  5628 | `		goto Abort;` |
|        - |  5629 | `	}` |
|        - |  5630 | `#endif` |
|        - |  5631 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5632 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5633 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5634 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5635 | `	}` |
|       37 |  5636 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5637 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5638 | `	}` |
|       37 |  5639 | `	break;` |
|        - |  5640 | `/*` |
|        - |  5641 | ` * OP_LNOT: * * *` |
|        - |  5642 | ` *` |
|        - |  5643 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5644 | ` * with its complement.` |
|        - |  5645 | ` */` |
|    44826 |  5646 | `case PH7_OP_LNOT:` |
|        - |  5647 | `#ifdef UNTRUST` |
|        - |  5648 | `	if( pTos < pStack ){` |
|        - |  5649 | `		goto Abort;` |
|        - |  5650 | `	}` |
|        - |  5651 | `#endif` |
|        - |  5652 | `	/* Force a boolean cast */` |
|    89698 |  5653 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5654 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5655 | `	}` |
|    89698 |  5656 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89698 |  5657 | `	break;` |
|        - |  5658 | `/*` |
|        - |  5659 | ` * OP_BITNOT: * * *` |
|        - |  5660 | ` *` |
|        - |  5661 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5662 | ` * with its ones-complement.` |
|        - |  5663 | ` */` |
|       14 |  5664 | `case PH7_OP_BITNOT:` |
|        - |  5665 | `#ifdef UNTRUST` |
|        - |  5666 | `	if( pTos < pStack ){` |
|        - |  5667 | `		goto Abort;` |
|        - |  5668 | `	}` |
|        - |  5669 | `#endif` |
|        - |  5670 | `	/* Force an integer cast */` |
|       30 |  5671 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5672 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5673 | `	}` |
|       30 |  5674 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  5675 | `	break;` |
|        - |  5676 | `/* OP_MUL * * *` |
|        - |  5677 | ` * OP_MUL_STORE * * *` |
|        - |  5678 | ` *` |
|        - |  5679 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5680 | ` * and push the result back onto the stack.` |
|        - |  5681 | ` */` |
|     1290 |  5682 | `case PH7_OP_MUL:` |
|        - |  5683 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  5684 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5685 | `	/* Force the operand to be numeric */` |
|        - |  5686 | `#ifdef UNTRUST` |
|        - |  5687 | `	if( pNos < pStack ){` |
|        - |  5688 | `		goto Abort;` |
|        - |  5689 | `	}` |
|        - |  5690 | `#endif` |
|     2582 |  5691 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  5692 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5693 | `	/* Perform the requested operation */` |
|     2582 |  5694 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5695 | `		/* Floating point arithemic */` |
|        - |  5696 | `		ph7_real a,b,r;` |
|       21 |  5697 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5698 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5699 | `		}` |
|       21 |  5700 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5701 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5702 | `		}` |
|       21 |  5703 | `		a = pNos->rVal;` |
|       21 |  5704 | `		b = pTos->rVal;` |
|       21 |  5705 | `		r = a * b;` |
|        - |  5706 | `		/* Push the result */` |
|       21 |  5707 | `		pNos->rVal = r;` |
|       21 |  5708 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5709 | `		/* Try to get an integer representation */` |
|       21 |  5710 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  5711 | `	}else{` |
|        - |  5712 | `		/* Integer arithmetic */` |
|        - |  5713 | `		sxi64 a,b,r;` |
|     2562 |  5714 | `		a = pNos->x.iVal;` |
|     2562 |  5715 | `		b = pTos->x.iVal;` |
|     2562 |  5716 | `		r = a * b;` |
|        - |  5717 | `		/* Push the result */` |
|     2562 |  5718 | `		pNos->x.iVal = r;` |
|     2562 |  5719 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5720 | `	}` |
|     2582 |  5721 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5722 | `		ph7_value *pObj;` |
|       32 |  5723 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5724 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5725 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5726 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5727 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5728 | `		}` |
|       15 |  5729 | `	}` |
|     2582 |  5730 | `	VmPopOperand(&pTos,1);` |
|     2582 |  5731 | `	break;` |
|        - |  5732 | `				 }` |
|        - |  5733 | `/* OP_POW * * *` |
|        - |  5734 | ` * OP_POW_STORE * * *` |
|        - |  5735 | ` *` |
|        - |  5736 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5737 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5738 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5739 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5740 | ` */` |
|       67 |  5741 | `case PH7_OP_POW:` |
|        - |  5742 | `case PH7_OP_POW_STORE: {` |
|      135 |  5743 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  5744 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5745 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5746 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5747 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5748 | `	 */` |
|      135 |  5749 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  5750 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5751 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5752 | `	int bBothInt;` |
|      135 |  5753 | `	int usedInt = 0;` |
|        - |  5754 | `	ph7_real a, b, r;` |
|        - |  5755 | `#endif` |
|      135 |  5756 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5757 | `#ifdef UNTRUST` |
|        - |  5758 | `	if( pNos < pStack ){` |
|        - |  5759 | `		goto Abort;` |
|        - |  5760 | `	}` |
|        - |  5761 | `#endif` |
|      135 |  5762 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  5763 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5764 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  5765 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  5766 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  5767 | `	if( bBothInt ){` |
|      123 |  5768 | `		base_i = pBase->x.iVal;` |
|      123 |  5769 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5770 | `	}` |
|      135 |  5771 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5772 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5773 | `	}` |
|      135 |  5774 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  5775 | `		PH7_MemObjToReal(pExp);` |
|       66 |  5776 | `	}` |
|      135 |  5777 | `	a = pBase->rVal;` |
|      135 |  5778 | `	b = pExp->rVal;` |
|      135 |  5779 | `	r = pow(a, b);` |
|        - |  5780 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5781 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5782 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5783 | `	 * representable as double but not as signed int64. */` |
|      135 |  5784 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5785 | `		sxi64 result_i = 1;` |
|      117 |  5786 | `		sxi64 cur_base = base_i;` |
|      117 |  5787 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5788 | `		int overflow = 0;` |
|      401 |  5789 | `		while( cur_exp > 0 ){` |
|      289 |  5790 | `			if( cur_exp & 1 ){` |
|      189 |  5791 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5792 | `					overflow = 1;` |
|        3 |  5793 | `					break;` |
|        - |  5794 | `				}` |
|       93 |  5795 | `			}` |
|      287 |  5796 | `			cur_exp >>= 1;` |
|      287 |  5797 | `			if( cur_exp > 0 ){` |
|      181 |  5798 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5799 | `					overflow = 1;` |
|        3 |  5800 | `					break;` |
|        - |  5801 | `				}` |
|       89 |  5802 | `			}` |
|        1 |  5803 | `		}` |
|      117 |  5804 | `		if( !overflow ){` |
|      113 |  5805 | `			pNos->x.iVal = result_i;` |
|      113 |  5806 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5807 | `			usedInt = 1;` |
|       56 |  5808 | `		}` |
|       58 |  5809 | `	}` |
|      135 |  5810 | `	if( !usedInt ){` |
|       23 |  5811 | `		pNos->rVal = r;` |
|       23 |  5812 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  5813 | `	}` |
|        - |  5814 | `#else` |
|        - |  5815 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5816 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5817 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5818 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5819 | `	 * represented. */` |
|        - |  5820 | `	base_i = pBase->x.iVal;` |
|        - |  5821 | `	exp_i  = pExp->x.iVal;` |
|        - |  5822 | `	{` |
|        - |  5823 | `		sxi64 result_i = 1;` |
|        - |  5824 | `		sxi64 cur_base = base_i;` |
|        - |  5825 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5826 | `		if( cur_exp < 0 ){` |
|        - |  5827 | `			result_i = 0;` |
|        - |  5828 | `		}else{` |
|        - |  5829 | `			while( cur_exp > 0 ){` |
|        - |  5830 | `				if( cur_exp & 1 ){` |
|        - |  5831 | `					result_i *= cur_base;` |
|        - |  5832 | `				}` |
|        - |  5833 | `				cur_exp >>= 1;` |
|        - |  5834 | `				if( cur_exp > 0 ){` |
|        - |  5835 | `					cur_base *= cur_base;` |
|        - |  5836 | `				}` |
|        - |  5837 | `			}` |
|        - |  5838 | `		}` |
|        - |  5839 | `		pNos->x.iVal = result_i;` |
|        - |  5840 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5841 | `	}` |
|        - |  5842 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  5843 | `	if( bStore ){` |
|        - |  5844 | `		ph7_value *pObj;` |
|       23 |  5845 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5846 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5847 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5848 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5849 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5850 | `		}` |
|       11 |  5851 | `	}` |
|      135 |  5852 | `	VmPopOperand(&pTos,1);` |
|      135 |  5853 | `	break;` |
|        - |  5854 | `				 }` |
|        - |  5855 | `/* OP_ADD * * *` |
|        - |  5856 | ` *` |
|        - |  5857 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5858 | ` * and push the result back onto the stack.` |
|        - |  5859 | ` */` |
|      528 |  5860 | `case PH7_OP_ADD:{` |
|     1058 |  5861 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5862 | `#ifdef UNTRUST` |
|        - |  5863 | `	if( pNos < pStack ){` |
|        - |  5864 | `		goto Abort;` |
|        - |  5865 | `	}` |
|        - |  5866 | `#endif` |
|        - |  5867 | `	/* Perform the addition */` |
|     1058 |  5868 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1058 |  5869 | `	VmPopOperand(&pTos,1);` |
|     1058 |  5870 | `	break;` |
|        - |  5871 | `				}` |
|        - |  5872 | `/*` |
|        - |  5873 | ` * OP_ADD_STORE * * *` |
|        - |  5874 | ` *` |
|        - |  5875 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5876 | ` * and push the result back onto the stack.` |
|        - |  5877 | ` */` |
|      502 |  5878 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5879 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5880 | `	ph7_value *pObj;` |
|        - |  5881 | `	sxu32 nIdx;` |
|        - |  5882 | `#ifdef UNTRUST` |
|        - |  5883 | `	if( pNos < pStack ){` |
|        - |  5884 | `		goto Abort;` |
|        - |  5885 | `	}` |
|        - |  5886 | `#endif` |
|        - |  5887 | `	/* Perform the addition */` |
|     1006 |  5888 | `	nIdx = pTos->nIdx;` |
|     1006 |  5889 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5890 | `	/* Peform the store operation */` |
|     1006 |  5891 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5892 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5893 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5894 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5895 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5896 | `	}` |
|        - |  5897 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5898 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5899 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5900 | `	break;` |
|        - |  5901 | `				}` |
|        - |  5902 | `/* OP_SUB * * *` |
|        - |  5903 | ` *` |
|        - |  5904 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5905 | ` * first (what was next on the stack) from the second (the` |
|        - |  5906 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5907 | ` */` |
|      349 |  5908 | `case PH7_OP_SUB: {` |
|      700 |  5909 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5910 | `#ifdef UNTRUST` |
|        - |  5911 | `	if( pNos < pStack ){` |
|        - |  5912 | `		goto Abort;` |
|        - |  5913 | `	}` |
|        - |  5914 | `#endif` |
|      700 |  5915 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5916 | `		/* Floating point arithemic */` |
|        - |  5917 | `		ph7_real a,b,r;` |
|       97 |  5918 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5919 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5920 | `		}` |
|       97 |  5921 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5922 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5923 | `		}` |
|       97 |  5924 | `		a = pNos->rVal;` |
|       97 |  5925 | `		b = pTos->rVal;` |
|       97 |  5926 | `		r = a - b;` |
|        - |  5927 | `		/* Push the result */` |
|       97 |  5928 | `		pNos->rVal = r;` |
|       97 |  5929 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5930 | `		/* Try to get an integer representation */` |
|       97 |  5931 | `		PH7_MemObjTryInteger(pNos);` |
|       49 |  5932 | `	}else{` |
|        - |  5933 | `		/* Integer arithmetic */` |
|        - |  5934 | `		sxi64 a,b,r;` |
|      604 |  5935 | `		a = pNos->x.iVal;` |
|      604 |  5936 | `		b = pTos->x.iVal;` |
|      604 |  5937 | `		r = a - b;` |
|        - |  5938 | `		/* Push the result */` |
|      604 |  5939 | `		pNos->x.iVal = r;` |
|      604 |  5940 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5941 | `	}` |
|      700 |  5942 | `	VmPopOperand(&pTos,1);` |
|      700 |  5943 | `	break;` |
|        - |  5944 | `				 }` |
|        - |  5945 | `/* OP_SUB_STORE * * *` |
|        - |  5946 | ` *` |
|        - |  5947 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5948 | ` * first (what was next on the stack) from the second (the` |
|        - |  5949 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5950 | ` */` |
|        4 |  5951 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5952 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5953 | `	ph7_value *pObj;` |
|        - |  5954 | `#ifdef UNTRUST` |
|        - |  5955 | `	if( pNos < pStack ){` |
|        - |  5956 | `		goto Abort;` |
|        - |  5957 | `	}` |
|        - |  5958 | `#endif` |
|       10 |  5959 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5960 | `		/* Floating point arithemic */` |
|        - |  5961 | `		ph7_real a,b,r;` |
|      ! 0 |  5962 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5963 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5964 | `		}` |
|      ! 0 |  5965 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5966 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5967 | `		}` |
|      ! 0 |  5968 | `		a = pTos->rVal;` |
|      ! 0 |  5969 | `		b = pNos->rVal;` |
|      ! 0 |  5970 | `		r = a - b;` |
|        - |  5971 | `		/* Push the result */` |
|      ! 0 |  5972 | `		pNos->rVal = r;` |
|      ! 0 |  5973 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5974 | `		/* Try to get an integer representation */` |
|      ! 0 |  5975 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5976 | `	}else{` |
|        - |  5977 | `		/* Integer arithmetic */` |
|        - |  5978 | `		sxi64 a,b,r;` |
|       10 |  5979 | `		a = pTos->x.iVal;` |
|       10 |  5980 | `		b = pNos->x.iVal;` |
|       10 |  5981 | `		r = a - b;` |
|        - |  5982 | `		/* Push the result */` |
|       10 |  5983 | `		pNos->x.iVal = r;` |
|       10 |  5984 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5985 | `	}` |
|       10 |  5986 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5987 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5988 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5989 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5990 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5991 | `	}` |
|       10 |  5992 | `	VmPopOperand(&pTos,1);` |
|       10 |  5993 | `	break;` |
|        - |  5994 | `				 }` |
|        - |  5995 |  |
|        - |  5996 | `/*` |
|        - |  5997 | ` * OP_MOD * * *` |
|        - |  5998 | ` *` |
|        - |  5999 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6000 | ` * first (what was next on the stack) from the second (the` |
|        - |  6001 | ` * top of the stack) and push the remainder after division` |
|        - |  6002 | ` * onto the stack.` |
|        - |  6003 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6004 | ` */` |
|      308 |  6005 | `case PH7_OP_MOD:{` |
|      618 |  6006 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6007 | `	sxi64 a,b,r;` |
|        - |  6008 | `#ifdef UNTRUST` |
|        - |  6009 | `	if( pNos < pStack ){` |
|        - |  6010 | `		goto Abort;` |
|        - |  6011 | `	}` |
|        - |  6012 | `#endif` |
|        - |  6013 | `	/* Force the operands to be integer */` |
|      618 |  6014 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6015 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6016 | `	}` |
|      618 |  6017 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6018 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6019 | `	}` |
|        - |  6020 | `	/* Perform the requested operation */` |
|      618 |  6021 | `	a = pNos->x.iVal;` |
|      618 |  6022 | `	b = pTos->x.iVal;` |
|      618 |  6023 | `	if( b == 0 ){` |
|        3 |  6024 | `		r = 0;` |
|        3 |  6025 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6026 | `		/* goto Abort; */` |
|        2 |  6027 | `	}else{` |
|      615 |  6028 | `		r = a%b;` |
|        - |  6029 | `	}` |
|        - |  6030 | `	/* Push the result */` |
|      618 |  6031 | `	pNos->x.iVal = r;` |
|      618 |  6032 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  6033 | `	VmPopOperand(&pTos,1);` |
|      618 |  6034 | `	break;` |
|        - |  6035 | `				}` |
|        - |  6036 | `/*` |
|        - |  6037 | ` * OP_MOD_STORE * * *` |
|        - |  6038 | ` *` |
|        - |  6039 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6040 | ` * first (what was next on the stack) from the second (the` |
|        - |  6041 | ` * top of the stack) and push the remainder after division` |
|        - |  6042 | ` * onto the stack.` |
|        - |  6043 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6044 | ` */` |
|        1 |  6045 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6046 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6047 | `	ph7_value *pObj;` |
|        - |  6048 | `	sxi64 a,b,r;` |
|        - |  6049 | `#ifdef UNTRUST` |
|        - |  6050 | `	if( pNos < pStack ){` |
|        - |  6051 | `		goto Abort;` |
|        - |  6052 | `	}` |
|        - |  6053 | `#endif` |
|        - |  6054 | `	/* Force the operands to be integer */` |
|        3 |  6055 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6056 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6057 | `	}` |
|        3 |  6058 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6059 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6060 | `	}` |
|        - |  6061 | `	/* Perform the requested operation */` |
|        3 |  6062 | `	a = pTos->x.iVal;` |
|        3 |  6063 | `	b = pNos->x.iVal;` |
|        3 |  6064 | `	if( b == 0 ){` |
|      ! 0 |  6065 | `		r = 0;` |
|      ! 0 |  6066 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6067 | `		/* goto Abort; */` |
|      ! 0 |  6068 | `	}else{` |
|        3 |  6069 | `		r = a%b;` |
|        - |  6070 | `	}` |
|        - |  6071 | `	/* Push the result */` |
|        3 |  6072 | `	pNos->x.iVal = r;` |
|        3 |  6073 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6074 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6075 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6076 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6077 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6078 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6079 | `	}` |
|        3 |  6080 | `	VmPopOperand(&pTos,1);` |
|        3 |  6081 | `	break;` |
|        - |  6082 | `				}` |
|        - |  6083 | `/*` |
|        - |  6084 | ` * OP_DIV * * *` |
|        - |  6085 | ` *` |
|        - |  6086 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6087 | ` * first (what was next on the stack) from the second (the` |
|        - |  6088 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6089 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6090 | ` */` |
|       33 |  6091 | `case PH7_OP_DIV:{` |
|       68 |  6092 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6093 | `	ph7_real a,b,r;` |
|        - |  6094 | `#ifdef UNTRUST` |
|        - |  6095 | `	if( pNos < pStack ){` |
|        - |  6096 | `		goto Abort;` |
|        - |  6097 | `	}` |
|        - |  6098 | `#endif` |
|        - |  6099 | `	/* Force the operands to be real */` |
|       68 |  6100 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6101 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6102 | `	}` |
|       68 |  6103 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6104 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6105 | `	}` |
|        - |  6106 | `	/* Perform the requested operation */` |
|       68 |  6107 | `	a = pNos->rVal;` |
|       68 |  6108 | `	b = pTos->rVal;` |
|       68 |  6109 | `	if( b == 0 ){` |
|        - |  6110 | `		/* Division by zero */` |
|        3 |  6111 | `		pNos->rVal = 0;` |
|        3 |  6112 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6113 | `		/* goto Abort; */` |
|        2 |  6114 | `	}else{` |
|       65 |  6115 | `		r = a/b;` |
|        - |  6116 | `		/* Push the result */` |
|       65 |  6117 | `		pNos->rVal = r;` |
|       65 |  6118 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6119 | `		/* Try to get an integer representation */` |
|       65 |  6120 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6121 | `	}` |
|       68 |  6122 | `	VmPopOperand(&pTos,1);` |
|       68 |  6123 | `	break;` |
|        - |  6124 | `				}` |
|        - |  6125 | `/*` |
|        - |  6126 | ` * OP_DIV_STORE * * *` |
|        - |  6127 | ` *` |
|        - |  6128 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6129 | ` * first (what was next on the stack) from the second (the` |
|        - |  6130 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6131 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6132 | ` */` |
|        2 |  6133 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6134 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6135 | `	ph7_value *pObj;` |
|        - |  6136 | `	ph7_real a,b,r;` |
|        - |  6137 | `#ifdef UNTRUST` |
|        - |  6138 | `	if( pNos < pStack ){` |
|        - |  6139 | `		goto Abort;` |
|        - |  6140 | `	}` |
|        - |  6141 | `#endif` |
|        - |  6142 | `	/* Force the operands to be real */` |
|        5 |  6143 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6144 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6145 | `	}` |
|        5 |  6146 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6147 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6148 | `	}` |
|        - |  6149 | `	/* Perform the requested operation */` |
|        5 |  6150 | `	a = pTos->rVal;` |
|        5 |  6151 | `	b = pNos->rVal;` |
|        5 |  6152 | `	if( b == 0 ){` |
|        - |  6153 | `		/* Division by zero */` |
|      ! 0 |  6154 | `		r = 0;` |
|      ! 0 |  6155 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6156 | `		/* goto Abort; */` |
|      ! 0 |  6157 | `	}else{` |
|        5 |  6158 | `		r = a/b;` |
|        - |  6159 | `		/* Push the result */` |
|        5 |  6160 | `		pNos->rVal = r;` |
|        5 |  6161 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6162 | `		/* Try to get an integer representation */` |
|        5 |  6163 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6164 | `	}` |
|        5 |  6165 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6166 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6167 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6168 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6169 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6170 | `	}` |
|        5 |  6171 | `	VmPopOperand(&pTos,1);` |
|        5 |  6172 | `	break;` |
|        - |  6173 | `				}` |
|        - |  6174 | `/* OP_BAND * * *` |
|        - |  6175 | ` *` |
|        - |  6176 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6177 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6178 | ` * two elements.` |
|        - |  6179 | `*/` |
|        - |  6180 | `/* OP_BOR * * *` |
|        - |  6181 | ` *` |
|        - |  6182 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6183 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6184 | ` * two elements.` |
|        - |  6185 | ` */` |
|        - |  6186 | `/* OP_BXOR * * *` |
|        - |  6187 | ` *` |
|        - |  6188 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6189 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6190 | ` * two elements.` |
|        - |  6191 | ` */` |
|       43 |  6192 | `case PH7_OP_BAND:` |
|        - |  6193 | `case PH7_OP_BOR:` |
|        - |  6194 | `case PH7_OP_BXOR:{` |
|       88 |  6195 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6196 | `	sxi64 a,b,r;` |
|        - |  6197 | `#ifdef UNTRUST` |
|        - |  6198 | `	if( pNos < pStack ){` |
|        - |  6199 | `		goto Abort;` |
|        - |  6200 | `	}` |
|        - |  6201 | `#endif` |
|        - |  6202 | `	/* Force the operands to be integer */` |
|       88 |  6203 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6204 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6205 | `	}` |
|       88 |  6206 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6207 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6208 | `	}` |
|        - |  6209 | `	/* Perform the requested operation */` |
|       88 |  6210 | `	a = pNos->x.iVal;` |
|       88 |  6211 | `	b = pTos->x.iVal;` |
|       88 |  6212 | `	switch(pInstr->iOp){` |
|        7 |  6213 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6214 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6215 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6216 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6217 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6218 | `	case PH7_OP_BAND:` |
|       60 |  6219 | `	default:          r = a&b; break;` |
|        - |  6220 | `	}` |
|        - |  6221 | `	/* Push the result */` |
|       88 |  6222 | `	pNos->x.iVal = r;` |
|       88 |  6223 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6224 | `	VmPopOperand(&pTos,1);` |
|       88 |  6225 | `	break;` |
|        - |  6226 | `				 }` |
|        - |  6227 | `/* OP_BAND_STORE * * *` |
|        - |  6228 | ` *` |
|        - |  6229 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6230 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6231 | ` * two elements.` |
|        - |  6232 | `*/` |
|        - |  6233 | `/* OP_BOR_STORE * * *` |
|        - |  6234 | ` *` |
|        - |  6235 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6236 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6237 | ` * two elements.` |
|        - |  6238 | ` */` |
|        - |  6239 | `/* OP_BXOR_STORE * * *` |
|        - |  6240 | ` *` |
|        - |  6241 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6242 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6243 | ` * two elements.` |
|        - |  6244 | ` */` |
|       10 |  6245 | `case PH7_OP_BAND_STORE:` |
|        - |  6246 | `case PH7_OP_BOR_STORE:` |
|        - |  6247 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6248 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6249 | `	ph7_value *pObj;` |
|        - |  6250 | `	sxi64 a,b,r;` |
|        - |  6251 | `#ifdef UNTRUST` |
|        - |  6252 | `	if( pNos < pStack ){` |
|        - |  6253 | `		goto Abort;` |
|        - |  6254 | `	}` |
|        - |  6255 | `#endif` |
|        - |  6256 | `	/* Force the operands to be integer */` |
|       21 |  6257 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6258 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6259 | `	}` |
|       21 |  6260 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6261 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6262 | `	}` |
|        - |  6263 | `	/* Perform the requested operation */` |
|       21 |  6264 | `	a = pTos->x.iVal;` |
|       21 |  6265 | `	b = pNos->x.iVal;` |
|       21 |  6266 | `	switch(pInstr->iOp){` |
|        3 |  6267 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6268 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6269 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6270 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6271 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6272 | `	case PH7_OP_BAND:` |
|        7 |  6273 | `	default:          r = a&b; break;` |
|        - |  6274 | `	}` |
|        - |  6275 | `	/* Push the result */` |
|       21 |  6276 | `	pNos->x.iVal = r;` |
|       21 |  6277 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6278 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6279 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6280 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6281 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6282 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6283 | `	}` |
|       21 |  6284 | `	VmPopOperand(&pTos,1);` |
|       21 |  6285 | `	break;` |
|        - |  6286 | `				 }` |
|        - |  6287 | `/* OP_SHL * * *` |
|        - |  6288 | ` *` |
|        - |  6289 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6290 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6291 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6292 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6293 | ` */` |
|        - |  6294 | `/* OP_SHR * * *` |
|        - |  6295 | ` *` |
|        - |  6296 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6297 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6298 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6299 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6300 | ` */` |
|       12 |  6301 | `case PH7_OP_SHL:` |
|        - |  6302 | `case PH7_OP_SHR: {` |
|       25 |  6303 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6304 | `	sxi64 a,r;` |
|        - |  6305 | `	sxi32 b;` |
|        - |  6306 | `#ifdef UNTRUST` |
|        - |  6307 | `	if( pNos < pStack ){` |
|        - |  6308 | `		goto Abort;` |
|        - |  6309 | `	}` |
|        - |  6310 | `#endif` |
|        - |  6311 | `	/* Force the operands to be integer */` |
|       25 |  6312 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6313 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6314 | `	}` |
|       25 |  6315 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6316 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6317 | `	}` |
|        - |  6318 | `	/* Perform the requested operation */` |
|       25 |  6319 | `	a = pNos->x.iVal;` |
|       25 |  6320 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6321 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6322 | `		r = a << b;` |
|        8 |  6323 | `	}else{` |
|       11 |  6324 | `		r = a >> b;` |
|        - |  6325 | `	}` |
|        - |  6326 | `	/* Push the result */` |
|       25 |  6327 | `	pNos->x.iVal = r;` |
|       25 |  6328 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6329 | `	VmPopOperand(&pTos,1);` |
|       25 |  6330 | `	break;` |
|        - |  6331 | `				 }` |
|        - |  6332 | `/*  OP_SHL_STORE * * *` |
|        - |  6333 | ` *` |
|        - |  6334 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6335 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6336 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6337 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6338 | ` */` |
|        - |  6339 | `/* OP_SHR_STORE * * *` |
|        - |  6340 | ` *` |
|        - |  6341 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6342 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6343 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6344 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6345 | ` */` |
|        9 |  6346 | `case PH7_OP_SHL_STORE:` |
|        - |  6347 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6348 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6349 | `	ph7_value *pObj;` |
|        - |  6350 | `	sxi64 a,r;` |
|        - |  6351 | `	sxi32 b;` |
|        - |  6352 | `#ifdef UNTRUST` |
|        - |  6353 | `	if( pNos < pStack ){` |
|        - |  6354 | `		goto Abort;` |
|        - |  6355 | `	}` |
|        - |  6356 | `#endif` |
|        - |  6357 | `	/* Force the operands to be integer */` |
|       19 |  6358 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6359 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6360 | `	}` |
|       19 |  6361 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6362 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6363 | `	}` |
|        - |  6364 | `	/* Perform the requested operation */` |
|       19 |  6365 | `	a = pTos->x.iVal;` |
|       19 |  6366 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6367 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6368 | `		r = a << b;` |
|        5 |  6369 | `	}else{` |
|       11 |  6370 | `		r = a >> b;` |
|        - |  6371 | `	}` |
|        - |  6372 | `	/* Push the result */` |
|       19 |  6373 | `	pNos->x.iVal = r;` |
|       19 |  6374 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6375 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6376 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6377 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6378 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6379 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6380 | `	}` |
|       19 |  6381 | `	VmPopOperand(&pTos,1);` |
|       19 |  6382 | `	break;` |
|        - |  6383 | `				 }` |
|        - |  6384 | `/* CAT:  P1 * *` |
|        - |  6385 | ` *` |
|        - |  6386 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6387 | ` * back.` |
|        - |  6388 | ` */` |
|    71743 |  6389 | `case PH7_OP_CAT:{` |
|        - |  6390 | `	ph7_value *pNos,*pCur;` |
|   143488 |  6391 | `	if( pInstr->iP1 < 1 ){` |
|   116008 |  6392 | `		pNos = &pTos[-1];` |
|    58005 |  6393 | `	}else{` |
|    27482 |  6394 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6395 | `	}` |
|        - |  6396 | `#ifdef UNTRUST` |
|        - |  6397 | `	if( pNos < pStack ){` |
|        - |  6398 | `		goto Abort;` |
|        - |  6399 | `	}` |
|        - |  6400 | `#endif` |
|        - |  6401 | `	/* Force a string cast */` |
|   143488 |  6402 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6403 | `		PH7_MemObjToString(pNos);` |
|      835 |  6404 | `	}` |
|   143488 |  6405 | `	pCur = &pNos[1];` |
|   289698 |  6406 | `	while( pCur <= pTos ){` |
|   146212 |  6407 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50918 |  6408 | `			PH7_MemObjToString(pCur);` |
|    25458 |  6409 | `		}` |
|        - |  6410 | `		/* Perform the concatenation */` |
|   146212 |  6411 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146168 |  6412 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    73083 |  6413 | `		}` |
|   146212 |  6414 | `		SyBlobRelease(&pCur->sBlob);` |
|   146212 |  6415 | `		pCur++;` |
|        2 |  6416 | `	}` |
|   143488 |  6417 | `	pTos = pNos;` |
|   143488 |  6418 | `	break;` |
|        - |  6419 | `				}` |
|        - |  6420 | `/*  CAT_STORE: * * *` |
|        - |  6421 | ` *` |
|        - |  6422 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6423 | ` * back.` |
|        - |  6424 | ` */` |
|     4112 |  6425 | `case PH7_OP_CAT_STORE:{` |
|     8226 |  6426 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6427 | `	ph7_value *pObj;` |
|        - |  6428 | `#ifdef UNTRUST` |
|        - |  6429 | `	if( pNos < pStack ){` |
|        - |  6430 | `		goto Abort;` |
|        - |  6431 | `	}` |
|        - |  6432 | `#endif` |
|     8226 |  6433 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6434 | `		/* Force a string cast */` |
|        3 |  6435 | `		PH7_MemObjToString(pTos);` |
|        1 |  6436 | `	}` |
|     8226 |  6437 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6438 | `		/* Force a string cast */` |
|      ! 0 |  6439 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6440 | `	}` |
|        - |  6441 | `	/* Perform the concatenation (Reverse order) */` |
|     8226 |  6442 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8226 |  6443 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     4112 |  6444 | `	}` |
|        - |  6445 | `	/* Perform the store operation */` |
|     8226 |  6446 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6447 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8226 |  6448 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8226 |  6449 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8224 |  6450 | `		PH7_MemObjStore(pTos,pObj);` |
|     4111 |  6451 | `	}` |
|     8224 |  6452 | `	PH7_MemObjStore(pTos,pNos);` |
|     8224 |  6453 | `	VmPopOperand(&pTos,1);` |
|     8224 |  6454 | `	break;` |
|        - |  6455 | `				}` |
|        - |  6456 | `/* OP_AND: * * *` |
|        - |  6457 | ` *` |
|        - |  6458 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6459 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6460 | ` * stack.` |
|        - |  6461 | ` */` |
|        - |  6462 | `/* OP_OR: * * *` |
|        - |  6463 | ` *` |
|        - |  6464 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6465 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6466 | ` * stack.` |
|        - |  6467 | ` */` |
|   108208 |  6468 | `case PH7_OP_LAND:` |
|        - |  6469 | `case PH7_OP_LOR: {` |
|   216462 |  6470 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6471 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6472 | `#ifdef UNTRUST` |
|        - |  6473 | `	if( pNos < pStack ){` |
|        - |  6474 | `		goto Abort;` |
|        - |  6475 | `	}` |
|        - |  6476 | `#endif` |
|        - |  6477 | `	/* Force a boolean cast */` |
|   216462 |  6478 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6479 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6480 | `	}` |
|   216462 |  6481 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6482 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6483 | `	}` |
|   216462 |  6484 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216462 |  6485 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216462 |  6486 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6487 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99336 |  6488 | `		v1 = and_logic[v1*3+v2];` |
|    49691 |  6489 | `	}else{` |
|        - |  6490 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117128 |  6491 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6492 | `	}` |
|   216462 |  6493 | `	if( v1 == 2 ){` |
|      ! 0 |  6494 | `		v1 = 1;` |
|      ! 0 |  6495 | `	}` |
|   216462 |  6496 | `	VmPopOperand(&pTos,1);` |
|   216462 |  6497 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216462 |  6498 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216462 |  6499 | `	break;` |
|        - |  6500 | `				 }` |
|        - |  6501 | `/*` |
|        - |  6502 | ` * OP_NULLC: * * *` |
|        - |  6503 | ` * Null coalescing operator '??'.` |
|        - |  6504 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6505 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6506 | ` */` |
|        - |  6507 | `/*` |
|        - |  6508 | ` * OP_NULLC: * P2 *` |
|        - |  6509 | ` * Short-circuit null coalescing '??'.` |
|        - |  6510 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6511 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6512 | ` */` |
|       93 |  6513 | `case PH7_OP_NULLC: {` |
|        - |  6514 | `#ifdef UNTRUST` |
|        - |  6515 | `	if( pTos < pStack ){` |
|        - |  6516 | `		goto Abort;` |
|        - |  6517 | `	}` |
|        - |  6518 | `#endif` |
|      188 |  6519 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6520 | `		/* Left is not null — keep it and skip the RHS */` |
|      114 |  6521 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       58 |  6522 | `	}else{` |
|        - |  6523 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       76 |  6524 | `		VmPopOperand(&pTos, 1);` |
|        - |  6525 | `	}` |
|      188 |  6526 | `	break;` |
|        - |  6527 |  |
|        - |  6528 | `/*` |
|        - |  6529 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6530 | ` * Null coalescing assignment short-circuit.` |
|        - |  6531 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6532 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6533 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6534 | ` */` |
|       28 |  6535 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6536 | `#ifdef UNTRUST` |
|        - |  6537 | `	if( pTos < pStack ){` |
|        - |  6538 | `		goto Abort;` |
|        - |  6539 | `	}` |
|        - |  6540 | `#endif` |
|       58 |  6541 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6542 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6543 | `	}` |
|       58 |  6544 | `	break;` |
|        - |  6545 |  |
|        - |  6546 | `/*` |
|        - |  6547 | ` * OP_NULLC_STORE: * * *` |
|        - |  6548 | ` * Null coalescing assignment store.` |
|        - |  6549 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6550 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6551 | ` * expression result.` |
|        - |  6552 | ` */` |
|        - |  6553 | `/*` |
|        - |  6554 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6555 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6556 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6557 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6558 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6559 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6560 | ` */` |
|       51 |  6561 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6562 | `#ifdef UNTRUST` |
|        - |  6563 | `	if( pTos < pStack ){` |
|        - |  6564 | `		goto Abort;` |
|        - |  6565 | `	}` |
|        - |  6566 | `#endif` |
|      104 |  6567 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6568 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6569 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6570 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6571 | `	}` |
|      104 |  6572 | `	break;` |
|        - |  6573 |  |
|       17 |  6574 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6575 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6576 | `	ph7_value *pObj;` |
|        - |  6577 | `	sxu32 nIdx;` |
|        - |  6578 | `#ifdef UNTRUST` |
|        - |  6579 | `	if( pNos < pStack ){` |
|        - |  6580 | `		goto Abort;` |
|        - |  6581 | `	}` |
|        - |  6582 | `#endif` |
|        - |  6583 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6584 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6585 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6586 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6587 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6588 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6589 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6590 | `		ph7_value *apArg[2];` |
|        5 |  6591 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6592 | `		apArg[1] = pTos;` |
|        5 |  6593 | `		if( pSet ){` |
|        5 |  6594 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6595 | `		}` |
|        - |  6596 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6597 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6598 | `		VmPopOperand(&pTos,1);` |
|        - |  6599 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6600 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6601 | `		break;` |
|        - |  6602 | `	}` |
|       32 |  6603 | `	nIdx = pNos->nIdx;` |
|       32 |  6604 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6605 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6606 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6607 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6608 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6609 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6610 | `	}` |
|       32 |  6611 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6612 | `	VmPopOperand(&pTos,1);` |
|       32 |  6613 | `	break;` |
|        - |  6614 |  |
|        - |  6615 | `/*` |
|        - |  6616 | ` * OP_SPREAD: * * *` |
|        - |  6617 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6618 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6619 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6620 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6621 | ` */` |
|        9 |  6622 | `case PH7_OP_SPREAD: {` |
|        - |  6623 | `#ifdef UNTRUST` |
|        - |  6624 | `	if( pTos < pStack ){` |
|        - |  6625 | `		goto Abort;` |
|        - |  6626 | `	}` |
|        - |  6627 | `#endif` |
|       20 |  6628 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6629 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6630 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6631 | `		if( nEntry == 0 ){` |
|        - |  6632 | `			/* Empty array — remove from stack */` |
|        3 |  6633 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6634 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6635 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6636 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6637 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6638 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6639 | `				VM_STACK_GUARD);` |
|      ! 0 |  6640 | `		}else{` |
|        - |  6641 | `			ph7_hashmap_node *pNode2;` |
|        - |  6642 | `			ph7_value *pElem;` |
|        - |  6643 | `			sxu32 i;` |
|        - |  6644 | `			/* Overwrite TOS with first element */` |
|       18 |  6645 | `			pNode2 = pMap->pFirst;` |
|       18 |  6646 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6647 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6648 | `			if( pElem ){` |
|       18 |  6649 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6650 | `			}` |
|       18 |  6651 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6652 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6653 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6654 | `			pNode2 = pNode2->pPrev;` |
|        - |  6655 | `			/* Push remaining elements */` |
|       44 |  6656 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6657 | `				pTos++;` |
|       28 |  6658 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6659 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6660 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6661 | `				if( pElem ){` |
|       28 |  6662 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6663 | `				}` |
|       28 |  6664 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6665 | `			}` |
|       18 |  6666 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6667 | `		}` |
|        9 |  6668 | `	}` |
|        - |  6669 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6670 | `	break;` |
|        - |  6671 |  |
|        - |  6672 | `/*` |
|        - |  6673 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6674 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6675 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6676 | ` */` |
|       34 |  6677 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6678 | `#ifdef UNTRUST` |
|        - |  6679 | `	if( pTos < pStack ){` |
|        - |  6680 | `		goto Abort;` |
|        - |  6681 | `	}` |
|        - |  6682 | `#endif` |
|       70 |  6683 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  6684 | `	break;` |
|        - |  6685 |  |
|        - |  6686 | `/* OP_LXOR: * * *` |
|        - |  6687 | ` *` |
|        - |  6688 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6689 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6690 | ` * stack.` |
|        - |  6691 | ` * According to the PHP language reference manual:` |
|        - |  6692 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6693 | ` *  TRUE,but not both.` |
|        - |  6694 | ` */` |
|        5 |  6695 | `case PH7_OP_LXOR:{` |
|       11 |  6696 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6697 | `	sxi32 v = 0;` |
|        - |  6698 | `#ifdef UNTRUST` |
|        - |  6699 | `	if( pNos < pStack ){` |
|        - |  6700 | `		goto Abort;` |
|        - |  6701 | `	}` |
|        - |  6702 | `#endif` |
|        - |  6703 | `	/* Force a boolean cast */` |
|       11 |  6704 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6705 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6706 | `	}` |
|       11 |  6707 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6708 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6709 | `	}` |
|       11 |  6710 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6711 | `		v = 1;` |
|        3 |  6712 | `	}` |
|       11 |  6713 | `	VmPopOperand(&pTos,1);` |
|       11 |  6714 | `	pTos->x.iVal = v;` |
|       11 |  6715 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6716 | `	break;` |
|        - |  6717 | `				 }` |
|        - |  6718 | `/* OP_EQ P1 P2 P3` |
|        - |  6719 | ` *` |
|        - |  6720 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6721 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6722 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6723 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6724 | ` */` |
|        - |  6725 | `/* OP_NEQ P1 P2 P3` |
|        - |  6726 | ` *` |
|        - |  6727 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6728 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6729 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6730 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6731 | ` */` |
|     4577 |  6732 | `case PH7_OP_EQ:` |
|        - |  6733 | `case PH7_OP_NEQ: {` |
|     9156 |  6734 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6735 | `	/* Perform the comparison and act accordingly */` |
|        - |  6736 | `#ifdef UNTRUST` |
|        - |  6737 | `	if( pNos < pStack ){` |
|        - |  6738 | `		goto Abort;` |
|        - |  6739 | `	}` |
|        - |  6740 | `#endif` |
|     9156 |  6741 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9156 |  6742 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6743 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9147 |  6744 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9112 |  6745 | `		rc = rc == 0;` |
|     4557 |  6746 | `	}else{` |
|       28 |  6747 | `		rc = rc != 0;` |
|        - |  6748 | `	}` |
|     9156 |  6749 | `	VmPopOperand(&pTos,1);` |
|     9156 |  6750 | `	if( !pInstr->iP2 ){` |
|        - |  6751 | `		/* Push comparison result without taking the jump */` |
|     9156 |  6752 | `		PH7_MemObjRelease(pTos);` |
|     9156 |  6753 | `		pTos->x.iVal = rc;` |
|        - |  6754 | `		/* Invalidate any prior representation */` |
|     9156 |  6755 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4579 |  6756 | `	}else{` |
|      ! 0 |  6757 | `		if( rc ){` |
|        - |  6758 | `			/* Jump to the desired location */` |
|      ! 0 |  6759 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6760 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6761 | `		}` |
|        - |  6762 | `	}` |
|     9156 |  6763 | `	break;` |
|        - |  6764 | `				 }` |
|        - |  6765 | `/* OP_TEQ P1 P2 *` |
|        - |  6766 | ` *` |
|        - |  6767 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6768 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6769 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6770 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6771 | ` */` |
|   161309 |  6772 | `case PH7_OP_TEQ: {` |
|   322620 |  6773 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6774 | `	/* Perform the comparison and act accordingly */` |
|        - |  6775 | `#ifdef UNTRUST` |
|        - |  6776 | `	if( pNos < pStack ){` |
|        - |  6777 | `		goto Abort;` |
|        - |  6778 | `	}` |
|        - |  6779 | `#endif` |
|   322620 |  6780 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   322620 |  6781 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6782 | `		rc = 0;` |
|        2 |  6783 | `	}else{` |
|   322618 |  6784 | `		rc = rc == 0;` |
|        - |  6785 | `	}` |
|   322620 |  6786 | `	VmPopOperand(&pTos,1);` |
|   322620 |  6787 | `	if( !pInstr->iP2 ){` |
|        - |  6788 | `		/* Push comparison result without taking the jump */` |
|   322620 |  6789 | `		PH7_MemObjRelease(pTos);` |
|   322620 |  6790 | `		pTos->x.iVal = rc;` |
|        - |  6791 | `		/* Invalidate any prior representation */` |
|   322620 |  6792 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   161311 |  6793 | `	}else{` |
|      ! 0 |  6794 | `		if( rc ){` |
|        - |  6795 | `			/* Jump to the desired location */` |
|      ! 0 |  6796 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6797 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6798 | `		}` |
|        - |  6799 | `	}` |
|   322620 |  6800 | `	break;` |
|        - |  6801 | `				 }` |
|        - |  6802 | `/* OP_TNE P1 P2 *` |
|        - |  6803 | ` *` |
|        - |  6804 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6805 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6806 | ` * instruction.` |
|        - |  6807 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6808 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6809 | ` *` |
|        - |  6810 | ` */` |
|   124094 |  6811 | `case PH7_OP_TNE: {` |
|   248190 |  6812 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6813 | `	/* Perform the comparison and act accordingly */` |
|        - |  6814 | `#ifdef UNTRUST` |
|        - |  6815 | `	if( pNos < pStack ){` |
|        - |  6816 | `		goto Abort;` |
|        - |  6817 | `	}` |
|        - |  6818 | `#endif` |
|   248190 |  6819 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   248190 |  6820 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6821 | `		rc = 1;` |
|        2 |  6822 | `	}else{` |
|   248188 |  6823 | `		rc = rc != 0;` |
|        - |  6824 | `	}` |
|   248190 |  6825 | `	VmPopOperand(&pTos,1);` |
|   248190 |  6826 | `	if( !pInstr->iP2 ){` |
|        - |  6827 | `		/* Push comparison result without taking the jump */` |
|   248190 |  6828 | `		PH7_MemObjRelease(pTos);` |
|   248190 |  6829 | `		pTos->x.iVal = rc;` |
|        - |  6830 | `		/* Invalidate any prior representation */` |
|   248190 |  6831 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124096 |  6832 | `	}else{` |
|      ! 0 |  6833 | `		if( rc ){` |
|        - |  6834 | `			/* Jump to the desired location */` |
|      ! 0 |  6835 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6836 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6837 | `		}` |
|        - |  6838 | `	}` |
|   248190 |  6839 | `	break;` |
|        - |  6840 | `				 }` |
|        - |  6841 | `/* OP_LT P1 P2 P3` |
|        - |  6842 | ` *` |
|        - |  6843 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6844 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6845 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6846 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6847 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6848 | ` *` |
|        - |  6849 | ` */` |
|        - |  6850 | `/* OP_LE P1 P2 P3` |
|        - |  6851 | ` *` |
|        - |  6852 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6853 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6854 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6855 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6856 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6857 | ` *` |
|        - |  6858 | ` */` |
|   112423 |  6859 | `case PH7_OP_LT:` |
|        - |  6860 | `case PH7_OP_LE: {` |
|   224892 |  6861 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6862 | `	/* Perform the comparison and act accordingly */` |
|        - |  6863 | `#ifdef UNTRUST` |
|        - |  6864 | `	if( pNos < pStack ){` |
|        - |  6865 | `		goto Abort;` |
|        - |  6866 | `	}` |
|        - |  6867 | `#endif` |
|   224892 |  6868 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224892 |  6869 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6870 | `		rc = 0;` |
|   224888 |  6871 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  6872 | `		rc = rc < 1;` |
|      805 |  6873 | `	}else{` |
|   223278 |  6874 | `		rc = rc < 0;` |
|        - |  6875 | `	}` |
|   224892 |  6876 | `	VmPopOperand(&pTos,1);` |
|   224892 |  6877 | `	if( !pInstr->iP2 ){` |
|        - |  6878 | `		/* Push comparison result without taking the jump */` |
|   224892 |  6879 | `		PH7_MemObjRelease(pTos);` |
|   224892 |  6880 | `		pTos->x.iVal = rc;` |
|        - |  6881 | `		/* Invalidate any prior representation */` |
|   224892 |  6882 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112469 |  6883 | `	}else{` |
|      ! 0 |  6884 | `		if( rc ){` |
|        - |  6885 | `			/* Jump to the desired location */` |
|      ! 0 |  6886 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6887 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6888 | `		}` |
|        - |  6889 | `	}` |
|   224892 |  6890 | `	break;` |
|        - |  6891 | `				}` |
|        - |  6892 | `/* OP_GT P1 P2 P3` |
|        - |  6893 | ` *` |
|        - |  6894 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6895 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6896 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6897 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6898 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6899 | ` *` |
|        - |  6900 | ` */` |
|        - |  6901 | `/* OP_GE P1 P2 P3` |
|        - |  6902 | ` *` |
|        - |  6903 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6904 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6905 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6906 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6907 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6908 | ` *` |
|        - |  6909 | ` */` |
|    55654 |  6910 | `case PH7_OP_GT:` |
|        - |  6911 | `case PH7_OP_GE: {` |
|   111310 |  6912 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6913 | `	/* Perform the comparison and act accordingly */` |
|        - |  6914 | `#ifdef UNTRUST` |
|        - |  6915 | `	if( pNos < pStack ){` |
|        - |  6916 | `		goto Abort;` |
|        - |  6917 | `	}` |
|        - |  6918 | `#endif` |
|   111310 |  6919 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111310 |  6920 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6921 | `		rc = 0;` |
|   111306 |  6922 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110878 |  6923 | `		rc = rc >= 0;` |
|    55440 |  6924 | `	}else{` |
|      426 |  6925 | `		rc = rc > 0;` |
|        - |  6926 | `	}` |
|   111310 |  6927 | `	VmPopOperand(&pTos,1);` |
|   111310 |  6928 | `	if( !pInstr->iP2 ){` |
|        - |  6929 | `		/* Push comparison result without taking the jump */` |
|   111310 |  6930 | `		PH7_MemObjRelease(pTos);` |
|   111310 |  6931 | `		pTos->x.iVal = rc;` |
|        - |  6932 | `		/* Invalidate any prior representation */` |
|   111310 |  6933 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55656 |  6934 | `	}else{` |
|      ! 0 |  6935 | `		if( rc ){` |
|        - |  6936 | `			/* Jump to the desired location */` |
|      ! 0 |  6937 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6938 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6939 | `		}` |
|        - |  6940 | `	}` |
|   111310 |  6941 | `	break;` |
|        - |  6942 | `				}` |
|        - |  6943 | `/* OP_SPACESHIP * * *` |
|        - |  6944 | ` *` |
|        - |  6945 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6946 | ` *   -1 if left < right` |
|        - |  6947 | ` *    0 if left == right` |
|        - |  6948 | ` *    1 if left > right` |
|        - |  6949 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6950 | ` */` |
|       25 |  6951 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6952 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6953 | `#ifdef UNTRUST` |
|        - |  6954 | `	if( pNos < pStack ){` |
|        - |  6955 | `		goto Abort;` |
|        - |  6956 | `	}` |
|        - |  6957 | `#endif` |
|       51 |  6958 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6959 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6960 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6961 | `		rc = 1;` |
|        4 |  6962 | `	}else{` |
|        - |  6963 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6964 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6965 | `	}` |
|       51 |  6966 | `	VmPopOperand(&pTos,1);` |
|       51 |  6967 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6968 | `	pTos->x.iVal = rc;` |
|       51 |  6969 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6970 | `	break;` |
|        - |  6971 | `				}` |
|        - |  6972 | `/* OP_SEQ P1 P2 *` |
|        - |  6973 | ` * Strict string comparison.` |
|        - |  6974 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6975 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6976 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6977 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6978 | ` * use PH7_OP_EQ.` |
|        - |  6979 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6980 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6981 | ` */` |
|        - |  6982 | `/* OP_SNE P1 P2 *` |
|        - |  6983 | ` * Strict string comparison.` |
|        - |  6984 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6985 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6986 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6987 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6988 | ` * use PH7_OP_EQ.` |
|        - |  6989 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6990 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6991 | ` */` |
|       18 |  6992 | `case PH7_OP_SEQ:` |
|        - |  6993 | `case PH7_OP_SNE: {` |
|       38 |  6994 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6995 | `	SyString s1,s2;` |
|        - |  6996 | `	/* Perform the comparison and act accordingly */` |
|        - |  6997 | `#ifdef UNTRUST` |
|        - |  6998 | `	if( pNos < pStack ){` |
|        - |  6999 | `		goto Abort;` |
|        - |  7000 | `	}` |
|        - |  7001 | `#endif` |
|        - |  7002 | `	/* Force a string cast */` |
|       38 |  7003 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7004 | `		PH7_MemObjToString(pTos);` |
|        2 |  7005 | `	}` |
|       38 |  7006 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7007 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7008 | `	}` |
|       38 |  7009 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7010 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7011 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7012 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7013 | `		rc = rc != 0;` |
|      ! 0 |  7014 | `	}else{` |
|       38 |  7015 | `		rc = rc == 0;` |
|        - |  7016 | `	}` |
|       38 |  7017 | `	VmPopOperand(&pTos,1);` |
|       38 |  7018 | `	if( !pInstr->iP2 ){` |
|        - |  7019 | `		/* Push comparison result without taking the jump */` |
|       38 |  7020 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7021 | `		pTos->x.iVal = rc;` |
|        - |  7022 | `		/* Invalidate any prior representation */` |
|       38 |  7023 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7024 | `	}else{` |
|      ! 0 |  7025 | `		if( rc ){` |
|        - |  7026 | `			/* Jump to the desired location */` |
|      ! 0 |  7027 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7028 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7029 | `		}` |
|        - |  7030 | `	}` |
|       38 |  7031 | `	break;` |
|        - |  7032 | `				 }` |
|        - |  7033 | `/*` |
|        - |  7034 | ` * OP_LOAD_REF * * *` |
|        - |  7035 | ` * Push the index of a referenced object on the stack.` |
|        - |  7036 | ` */` |
|       60 |  7037 | `case PH7_OP_LOAD_REF: {` |
|        - |  7038 | `	sxu32 nIdx;` |
|        - |  7039 | `#ifdef UNTRUST` |
|        - |  7040 | `	if( pTos < pStack ){` |
|        - |  7041 | `		goto Abort;` |
|        - |  7042 | `	}` |
|        - |  7043 | `#endif` |
|        - |  7044 | `	/* Extract memory object index */` |
|      121 |  7045 | `	nIdx = pTos->nIdx;` |
|      121 |  7046 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7047 | `		/* Nullify the object */` |
|      101 |  7048 | `		PH7_MemObjRelease(pTos);` |
|        - |  7049 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7050 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7051 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7052 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7053 | `	}` |
|      121 |  7054 | `	break;` |
|        - |  7055 | `					  }` |
|        - |  7056 | `/*` |
|        - |  7057 | ` * OP_STORE_REF * * P3` |
|        - |  7058 | ` * Perform an assignment operation by reference.` |
|        - |  7059 | ` */` |
|       16 |  7060 | ` case PH7_OP_STORE_REF: {` |
|       34 |  7061 | `	 SyString sName = { 0 , 0 };` |
|        - |  7062 | `	 VmFrame *pFrameLocal;` |
|        - |  7063 | `	SyHashEntry *pEntry;` |
|        - |  7064 | `	sxu32 nIdx;` |
|        - |  7065 | `#ifdef UNTRUST` |
|        - |  7066 | `	if( pTos < pStack ){` |
|        - |  7067 | `		goto Abort;` |
|        - |  7068 | `	}` |
|        - |  7069 | `#endif` |
|       34 |  7070 | `	if( pInstr->p3 == 0 ){` |
|        - |  7071 | `		char *zName;` |
|        - |  7072 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7073 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7074 | `			/* Force a string cast */` |
|      ! 0 |  7075 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7076 | `		}` |
|      ! 0 |  7077 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7078 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7079 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7080 | `			if( zName ){` |
|      ! 0 |  7081 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7082 | `			}` |
|      ! 0 |  7083 | `		}` |
|      ! 0 |  7084 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7085 | `		pTos--;` |
|      ! 0 |  7086 | `	}else{` |
|       34 |  7087 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7088 | `	}` |
|       34 |  7089 | `	nIdx = pTos->nIdx;` |
|       34 |  7090 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7091 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7092 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7093 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7094 | `		}else{` |
|        - |  7095 | `			ph7_value *pObj;` |
|        - |  7096 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7097 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7098 | `			if( pObj == 0 ){` |
|      ! 0 |  7099 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7100 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7101 | `				goto Abort;` |
|        - |  7102 | `			}` |
|        - |  7103 | `			/* Perform the store operation */` |
|      ! 0 |  7104 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7105 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7106 | `		}` |
|       34 |  7107 | `	}else if( sName.nByte > 0){` |
|       34 |  7108 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7109 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7110 | `		}else{` |
|       34 |  7111 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7112 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7113 | `			/* Query the local frame */` |
|       34 |  7114 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7115 | `			if( pEntry ){` |
|      ! 0 |  7116 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7117 | `			}else{` |
|       34 |  7118 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7119 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7120 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7121 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7122 | `				}` |
|       34 |  7123 | `				if( rc == SXRET_OK ){` |
|       34 |  7124 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7125 | `				}` |
|        - |  7126 | `			}` |
|        - |  7127 | `		}` |
|       16 |  7128 | `	}` |
|       34 |  7129 | `	break;` |
|        - |  7130 | `				 }` |
|        - |  7131 | `/*` |
|        - |  7132 | ` * OP_UPLINK P1 * *` |
|        - |  7133 | ` * Link a variable to the top active VM frame.` |
|        - |  7134 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7135 | ` */` |
|       30 |  7136 | `case PH7_OP_UPLINK: {` |
|       62 |  7137 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7138 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7139 | `		SyString sName;` |
|        - |  7140 | `		/* Perform the link */` |
|      132 |  7141 | `		while( pLink <= pTos ){` |
|       72 |  7142 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7143 | `				/* Force a string cast */` |
|      ! 0 |  7144 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7145 | `			}` |
|       72 |  7146 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7147 | `			if( sName.nByte > 0 ){` |
|       72 |  7148 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7149 | `			}` |
|       72 |  7150 | `			pLink++;` |
|        2 |  7151 | `		}` |
|       30 |  7152 | `	}` |
|       62 |  7153 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7154 | `	break;` |
|        - |  7155 | `					}` |
|        - |  7156 | `/*` |
|        - |  7157 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7158 | ` * Push an exception in the corresponding container so that` |
|        - |  7159 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7160 | ` */` |
|      180 |  7161 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      362 |  7162 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7163 | `	VmFrame *pFrameLocal;` |
|        - |  7164 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      362 |  7165 | `	pException->iFinallyDone = 0;` |
|      362 |  7166 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7167 | `	/* Create the exception frame */` |
|      362 |  7168 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      362 |  7169 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7170 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7171 | `		goto Abort;` |
|        - |  7172 | `	}` |
|        - |  7173 | `	/* Mark the special frame */` |
|      362 |  7174 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      362 |  7175 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7176 | `	/* Point to the frame that trigger the exception */` |
|      362 |  7177 | `	pFrameLocal = pFrameLocal->pParent;` |
|      362 |  7178 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      362 |  7179 | `	pException->pFrame = pFrameLocal;` |
|      362 |  7180 | `	break;` |
|        - |  7181 | `							}` |
|        - |  7182 | `/*` |
|        - |  7183 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7184 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7185 | ` */` |
|      179 |  7186 | `case PH7_OP_POP_EXCEPTION: {` |
|      360 |  7187 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      360 |  7188 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7189 | `		ph7_exception **apException;` |
|        - |  7190 | `		/* Pop the loaded exception */` |
|       32 |  7191 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7192 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7193 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7194 | `		}` |
|       15 |  7195 | `	}` |
|      360 |  7196 | `	pException->pFrame = 0;` |
|        - |  7197 | `	/* Leave the exception frame */` |
|      360 |  7198 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7199 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      360 |  7200 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7201 | `		sxi32 rcFinally;` |
|       20 |  7202 | `		pException->iFinallyDone = 1;` |
|       20 |  7203 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7204 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7205 | `			goto Abort;` |
|        - |  7206 | `		}` |
|        9 |  7207 | `	}` |
|      360 |  7208 | `	break;` |
|        - |  7209 | `							}` |
|        - |  7210 |  |
|        - |  7211 | `/*` |
|        - |  7212 | ` * OP_THROW * P2 *` |
|        - |  7213 | ` * Throw an user exception.` |
|        - |  7214 | ` */` |
|       75 |  7215 | `case PH7_OP_THROW: {` |
|      152 |  7216 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      152 |  7217 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7218 | `#ifdef UNTRUST` |
|        - |  7219 | `	if( pTos < pStack ){` |
|        - |  7220 | `		goto Abort;` |
|        - |  7221 | `	}` |
|        - |  7222 | `#endif` |
|      152 |  7223 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7224 | `	/* Tell the upper layer that an exception was thrown */` |
|      152 |  7225 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      152 |  7226 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      152 |  7227 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7228 | `		ph7_class *pThrowable;` |
|        - |  7229 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      152 |  7230 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      153 |  7231 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7232 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7233 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7234 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7235 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7236 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7237 | `			if( pErrorClass ){` |
|        3 |  7238 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7239 | `			}` |
|        3 |  7240 | `			if( pErrInst ){` |
|        - |  7241 | `				ph7_class_method *pCons;` |
|        3 |  7242 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7243 | `				if( pCons ){` |
|        - |  7244 | `					ph7_value sArg;` |
|        - |  7245 | `					ph7_value *apArg[1];` |
|        - |  7246 | `					SyString sMsgStr;` |
|        - |  7247 | `					static const char zErrMsg[] =` |
|        - |  7248 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7249 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7250 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7251 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7252 | `					apArg[0] = &sArg;` |
|        3 |  7253 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7254 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7255 | `				}` |
|        3 |  7256 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7257 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7258 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7259 | `					goto Abort;` |
|        - |  7260 | `				}` |
|        2 |  7261 | `			}else{` |
|        - |  7262 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7263 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7264 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7265 | `					goto Abort;` |
|        - |  7266 | `				}` |
|        - |  7267 | `			}` |
|        2 |  7268 | `		}else{` |
|        - |  7269 | `			/* Throw the exception */` |
|      150 |  7270 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      150 |  7271 | `			if( rc == SXERR_ABORT ){` |
|        - |  7272 | `				/* Abort processing immediately */` |
|       11 |  7273 | `				goto Abort;` |
|        - |  7274 | `			}` |
|        - |  7275 | `		}` |
|       72 |  7276 | `	}else{` |
|        - |  7277 | `		/* Expecting a class instance */` |
|      ! 0 |  7278 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7279 | `		if( rc == SXERR_ABORT ){` |
|        - |  7280 | `			/* Abort processing immediately */` |
|      ! 0 |  7281 | `			goto Abort;` |
|        - |  7282 | `		}` |
|        - |  7283 | `	}` |
|        - |  7284 | `	/* Pop the top entry */` |
|      142 |  7285 | `	VmPopOperand(&pTos,1);` |
|        - |  7286 | `	/* Perform an unconditional jump */` |
|      142 |  7287 | `	pc = nJump - 1;` |
|      142 |  7288 | `	break;` |
|        - |  7289 | `				   }` |
|        - |  7290 | `/*` |
|        - |  7291 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7292 | ` * Prepare a foreach step.` |
|        - |  7293 | ` */` |
|     6166 |  7294 | `case PH7_OP_FOREACH_INIT: {` |
|    12334 |  7295 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7296 | `	void *pName;` |
|        - |  7297 | `#ifdef UNTRUST` |
|        - |  7298 | `	if( pTos < pStack ){` |
|        - |  7299 | `		goto Abort;` |
|        - |  7300 | `	}` |
|        - |  7301 | `#endif` |
|    12334 |  7302 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7303 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7304 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7305 | `			/* Force a string cast */` |
|      ! 0 |  7306 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7307 | `		}` |
|        - |  7308 | `		/* Duplicate name */` |
|      ! 0 |  7309 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7310 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7311 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7312 | `		}` |
|      ! 0 |  7313 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7314 | `	}` |
|    12334 |  7315 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7316 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7317 | `			/* Force a string cast */` |
|      ! 0 |  7318 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7319 | `		}` |
|        - |  7320 | `		/* Duplicate name */` |
|      ! 0 |  7321 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7322 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7323 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7324 | `		}` |
|      ! 0 |  7325 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7326 | `	}` |
|        - |  7327 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12334 |  7328 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7329 | `		/* Jump out of the loop */` |
|      ! 0 |  7330 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7331 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7332 | `		}` |
|      ! 0 |  7333 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7334 | `	}else{` |
|        - |  7335 | `		ph7_foreach_step *pStep;` |
|    12334 |  7336 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12334 |  7337 | `		if( pStep == 0 ){` |
|      ! 0 |  7338 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7339 | `			/* Jump out of the loop */` |
|      ! 0 |  7340 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7341 | `		}else{` |
|        - |  7342 | `			/* Zero the structure */` |
|    12334 |  7343 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7344 | `			/* Prepare the step */` |
|    12334 |  7345 | `			pStep->iFlags = pInfo->iFlags;` |
|    12334 |  7346 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7347 | `				ph7_hashmap *pMap;` |
|        - |  7348 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7349 | `				 * source array so mutations don't affect other sharers. */` |
|    12300 |  7350 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7351 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7352 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7353 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7354 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7355 | `						 * variable still points at the same hashmap as` |
|        - |  7356 | `						 * the stack value. */` |
|        9 |  7357 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7358 | `							pCur->iRef--;` |
|        9 |  7359 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7360 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7361 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7362 | `						}` |
|        4 |  7363 | `					}` |
|        4 |  7364 | `				}` |
|    12300 |  7365 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7366 | `				/* Reset the internal loop cursor */` |
|    12300 |  7367 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7368 | `				/* Mark the step */` |
|    12300 |  7369 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12300 |  7370 | `				pStep->xIter.pMap = pMap;` |
|    12300 |  7371 | `				pMap->iRef++;` |
|     6151 |  7372 | `			}else{` |
|       36 |  7373 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7374 | `				ph7_class *pIteratorClass;` |
|        - |  7375 | `				/* Check if the object implements Iterator */` |
|       36 |  7376 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7377 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7378 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7379 | `					ph7_class_method *pRewind;` |
|       24 |  7380 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7381 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7382 | `					pThis->iRef++;` |
|       24 |  7383 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7384 | `					if( pRewind ){` |
|       24 |  7385 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7386 | `					}` |
|       13 |  7387 | `				}else{` |
|        - |  7388 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7389 | `					ph7_class *pIterAggClass;` |
|       14 |  7390 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7391 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7392 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7393 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7394 | `						ph7_class_method *pGetIter;` |
|        3 |  7395 | `						int iterAggOk = 0;` |
|        3 |  7396 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7397 | `						if( pGetIter ){` |
|        - |  7398 | `							ph7_value sResult;` |
|        3 |  7399 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7400 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7401 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7402 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7403 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7404 | `									ph7_class_method *pRewind;` |
|        3 |  7405 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7406 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7407 | `									pIterObj->iRef++;` |
|        - |  7408 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7409 | `									pStep->pOwner = pThis;` |
|        3 |  7410 | `									pThis->iRef++;` |
|        3 |  7411 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7412 | `									if( pRewind ){` |
|        3 |  7413 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7414 | `									}` |
|        3 |  7415 | `									iterAggOk = 1;` |
|        1 |  7416 | `								}` |
|        1 |  7417 | `							}` |
|        3 |  7418 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7419 | `						}` |
|        3 |  7420 | `						if( !iterAggOk ){` |
|        - |  7421 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7422 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7423 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7424 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7425 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7426 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7427 | `						}` |
|        2 |  7428 | `					}else{` |
|        - |  7429 | `						/* Plain object iteration via hAttr */` |
|       12 |  7430 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7431 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7432 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7433 | `						pThis->iRef++;` |
|        - |  7434 | `					}` |
|        - |  7435 | `				}` |
|        - |  7436 | `			}` |
|        - |  7437 | `		}` |
|    12334 |  7438 | `		if( pStep ){` |
|    12334 |  7439 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7440 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7441 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7442 | `				/* Jump out of the loop */` |
|      ! 0 |  7443 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7444 | `			}` |
|     6166 |  7445 | `		}` |
|        - |  7446 | `	}` |
|    12334 |  7447 | `	VmPopOperand(&pTos,1);` |
|    12334 |  7448 | `	break;` |
|        - |  7449 | `						  }` |
|        - |  7450 | `/*` |
|        - |  7451 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7452 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7453 | ` */` |
|   101183 |  7454 | `case PH7_OP_FOREACH_STEP: {` |
|   202368 |  7455 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7456 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7457 | `	ph7_value *pValue;` |
|        - |  7458 | `	VmFrame *pFrameLocal;` |
|        - |  7459 | `	/* Peek the last step */` |
|   202368 |  7460 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   202368 |  7461 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   202368 |  7462 | `	pFrameLocal = pVm->pFrame;` |
|   202368 |  7463 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   202368 |  7464 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   202234 |  7465 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7466 | `		ph7_hashmap_node *pNode;` |
|        - |  7467 | `		/* Extract the current node value */` |
|   202234 |  7468 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   202234 |  7469 | `		if( pNode == 0 ){` |
|        - |  7470 | `			/* No more entry to process */` |
|    12298 |  7471 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12298 |  7472 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7473 | `				/* Break the reference with the last element */` |
|        7 |  7474 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7475 | `			}` |
|        - |  7476 | `			/* Automatically reset the loop cursor */` |
|    12298 |  7477 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7478 | `			/* Cleanup the mess left behind */` |
|    12298 |  7479 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12298 |  7480 | `			SySetPop(&pInfo->aStep);` |
|    12298 |  7481 | `			PH7_HashmapUnref(pMap);` |
|     6150 |  7482 | `		}else{` |
|   189938 |  7483 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7484 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7485 | `				if( pKey ){` |
|      528 |  7486 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7487 | `				}` |
|      263 |  7488 | `			}` |
|   189938 |  7489 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7490 | `				SyHashEntry *pEntry;` |
|        - |  7491 | `				/* Pass by reference */` |
|       23 |  7492 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7493 | `				if( pEntry ){` |
|       21 |  7494 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7495 | `				}else{` |
|        4 |  7496 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7497 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7498 | `				}` |
|       12 |  7499 | `			}else{` |
|        - |  7500 | `				/* Make a copy of the entry value */` |
|   189916 |  7501 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   189916 |  7502 | `				if( pValue ){` |
|   189916 |  7503 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    94957 |  7504 | `				}` |
|        - |  7505 | `			}` |
|        2 |  7506 | `		}` |
|   101252 |  7507 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7508 | `		/* Iterator-based iteration.` |
|        - |  7509 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7510 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7511 | `		 */` |
|      106 |  7512 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7513 | `		ph7_class_method *pMethod;` |
|        - |  7514 | `		ph7_value sResult;` |
|      106 |  7515 | `		int isValid = 0;` |
|        - |  7516 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7517 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7518 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7519 | `		}else{` |
|       82 |  7520 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7521 | `			if( pMethod ){` |
|       82 |  7522 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7523 | `			}` |
|        - |  7524 | `		}` |
|        - |  7525 | `		/* Call valid() */` |
|      106 |  7526 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7527 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7528 | `		if( pMethod ){` |
|      106 |  7529 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7530 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7531 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7532 | `		}` |
|      106 |  7533 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7534 | `		if( !isValid ){` |
|        - |  7535 | `			/* Iterator exhausted */` |
|       24 |  7536 | `			pc = pInstr->iP2 - 1;` |
|        - |  7537 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7538 | `			if( pStep->pOwner ){` |
|        3 |  7539 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7540 | `			}` |
|       24 |  7541 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7542 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7543 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7544 | `		}else{` |
|        - |  7545 | `			/* Call current() to get value */` |
|       84 |  7546 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7547 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7548 | `			if( pMethod ){` |
|       84 |  7549 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7550 | `			}` |
|       84 |  7551 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7552 | `			if( pValue ){` |
|       84 |  7553 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7554 | `			}` |
|       84 |  7555 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7556 | `			/* Call key() if needed */` |
|       84 |  7557 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7558 | `				ph7_value sKey;` |
|       35 |  7559 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7560 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7561 | `				if( pMethod ){` |
|       35 |  7562 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7563 | `				}` |
|       35 |  7564 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7565 | `				if( pValue ){` |
|       35 |  7566 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7567 | `				}` |
|       35 |  7568 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7569 | `			}` |
|        - |  7570 | `		}` |
|       54 |  7571 | `	}else{` |
|       32 |  7572 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7573 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7574 | `		SyHashEntry *pEntry;` |
|        - |  7575 | `		/* Point to the next attribute */` |
|       36 |  7576 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7577 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7578 | `			/* Check access permission */` |
|       38 |  7579 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7580 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7581 | `					break; /* Access is granted */` |
|        - |  7582 | `			}` |
|        1 |  7583 | `		}` |
|       32 |  7584 | `		if( pEntry == 0 ){` |
|        - |  7585 | `			/* Clean up the mess left behind */` |
|       12 |  7586 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7587 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7588 | `				/* Break the reference with the last element */` |
|        3 |  7589 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7590 | `			}` |
|       12 |  7591 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7592 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7593 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7594 | `		}else{` |
|       22 |  7595 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7596 | `			ph7_value *pAttrValue;` |
|       22 |  7597 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7598 | `				/* Fill with the current attribute name */` |
|       22 |  7599 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7600 | `				if( pKey ){` |
|       22 |  7601 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7602 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7603 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7604 | `				}` |
|       10 |  7605 | `			}` |
|        - |  7606 | `			/* Extract attribute value */` |
|       22 |  7607 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7608 | `			if( pAttrValue ){` |
|       22 |  7609 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7610 | `					/* Pass by reference */` |
|        3 |  7611 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7612 | `					if( pEntry ){` |
|        3 |  7613 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7614 | `					}else{` |
|      ! 0 |  7615 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7616 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7617 | `					}` |
|        2 |  7618 | `				}else{` |
|        - |  7619 | `					/* Make a copy of the attribute value */` |
|       20 |  7620 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7621 | `					if( pValue ){` |
|       20 |  7622 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7623 | `					}` |
|        - |  7624 | `				}` |
|       10 |  7625 | `			}` |
|        - |  7626 | `		}` |
|        - |  7627 | `	}` |
|   202368 |  7628 | `	break;` |
|        - |  7629 | `						  }` |
|        - |  7630 | `/*` |
|        - |  7631 | ` * OP_MEMBER P1 P2` |
|        - |  7632 | ` * Load class attribute/method on the stack.` |
|        - |  7633 | ` */` |
|     3980 |  7634 | `case PH7_OP_MEMBER: {` |
|        - |  7635 | `	ph7_class_instance *pThis;` |
|        - |  7636 | `	ph7_value *pNos;` |
|        - |  7637 | `	SyString sName;` |
|     7962 |  7638 | `	if( !pInstr->iP1 ){` |
|     7734 |  7639 | `		pNos = &pTos[-1];` |
|        - |  7640 | `#ifdef UNTRUST` |
|        - |  7641 | `		if( pNos < pStack ){` |
|        - |  7642 | `			goto Abort;` |
|        - |  7643 | `		}` |
|        - |  7644 | `#endif` |
|     7734 |  7645 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7646 | `			ph7_class *pClass;` |
|        - |  7647 | `			/* Class already instantiated */` |
|     7732 |  7648 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7649 | `			/* Point to the instantiated class */` |
|     7732 |  7650 | `			pClass = pThis->pClass;` |
|        - |  7651 | `			/* Extract attribute name first */` |
|     7732 |  7652 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7732 |  7653 | `			if( pInstr->iP2 ){` |
|        - |  7654 | `				/* Method call */` |
|      782 |  7655 | `				ph7_class_method *pMeth = 0;` |
|      782 |  7656 | `				if( sName.nByte > 0 ){` |
|        - |  7657 | `					/* Extract the target method */` |
|      782 |  7658 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      390 |  7659 | `				}` |
|      782 |  7660 | `				if( pMeth == 0 ){` |
|      ! 0 |  7661 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7662 | `						&pClass->sName,&sName` |
|        - |  7663 | `						);` |
|        - |  7664 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7665 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7666 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7667 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7668 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7669 | `				}else{` |
|        - |  7670 | `					/* Push method name on the stack */` |
|      782 |  7671 | `					PH7_MemObjRelease(pTos);` |
|      782 |  7672 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      782 |  7673 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7674 | `				}` |
|      782 |  7675 | `				pTos->nIdx = SXU32_HIGH;` |
|      392 |  7676 | `			}else{` |
|        - |  7677 | `				/* Attribute access */` |
|     6952 |  7678 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7679 | `				SyHashEntry *pEntry;` |
|        - |  7680 | `				/* Extract the target attribute */` |
|     6952 |  7681 | `				if( sName.nByte > 0 ){` |
|     6952 |  7682 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6952 |  7683 | `					if( pEntry ){` |
|        - |  7684 | `						/* Point to the attribute value */` |
|     6950 |  7685 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3474 |  7686 | `					}` |
|     3475 |  7687 | `				}` |
|     6952 |  7688 | `				if( pObjAttr == 0 ){` |
|        - |  7689 | `					/* No such attribute,load null */` |
|        4 |  7690 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7691 | `						&pClass->sName,&sName);` |
|        - |  7692 | `					/* Call the __get magic method if available */` |
|        3 |  7693 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7694 | `				}` |
|     6952 |  7695 | `				VmPopOperand(&pTos,1);` |
|        - |  7696 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7697 | `				 * This is due to the following case:` |
|        - |  7698 | `				 *     (new TestClass())->foo;` |
|        - |  7699 | `				 */` |
|     6952 |  7700 | `				pThis->iRef++;` |
|     6952 |  7701 | `				PH7_MemObjRelease(pTos);` |
|     6952 |  7702 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6952 |  7703 | `				if( pObjAttr ){` |
|     6950 |  7704 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7705 | `					/* Check attribute access */` |
|     6950 |  7706 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7707 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7708 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7709 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7710 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7711 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6948 |  7712 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3516 |  7713 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  7714 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  7715 | `							int bIsLhs = 0;` |
|       82 |  7716 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  7717 | `								bIsLhs = 1;` |
|       39 |  7718 | `							}` |
|       82 |  7719 | `							if( !bIsLhs ){` |
|        3 |  7720 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7721 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7722 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7723 | `									goto Abort;` |
|        - |  7724 | `								}` |
|        - |  7725 | `								{` |
|        3 |  7726 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7727 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7728 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3980 |  7729 | `										break;` |
|        - |  7730 | `									}` |
|        - |  7731 | `								}` |
|      ! 0 |  7732 | `								goto Exception;` |
|        - |  7733 | `							}` |
|       39 |  7734 | `						}` |
|        - |  7735 | `						/* Load attribute */` |
|     6948 |  7736 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6948 |  7737 | `						if( pValue ){` |
|     6948 |  7738 | `							if( pThis->iRef < 2 ){` |
|        - |  7739 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7740 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7741 | `								 */` |
|        7 |  7742 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7743 | `							}else{` |
|        - |  7744 | `								/* Simple load */` |
|     6942 |  7745 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7746 | `							}` |
|     6948 |  7747 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6946 |  7748 | `								if( pThis->iRef > 1 ){` |
|        - |  7749 | `									/* Load attribute index */` |
|     6940 |  7750 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3469 |  7751 | `								}` |
|     3472 |  7752 | `							}` |
|     3473 |  7753 | `						}` |
|     3475 |  7754 | `					}else{` |
|        - |  7755 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7756 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7757 | `						char zMsg[256];` |
|      ! 0 |  7758 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7759 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7760 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7761 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7762 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7763 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7764 | `						goto Abort;` |
|        - |  7765 | `					}` |
|     3473 |  7766 | `				}` |
|        - |  7767 | `				/* Safely unreference the object */` |
|     6950 |  7768 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7769 | `			}` |
|     3866 |  7770 | `		}else{` |
|        3 |  7771 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7772 | `			VmPopOperand(&pTos,1);` |
|        3 |  7773 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7774 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7775 | `		}` |
|     3867 |  7776 | `	}else{` |
|        - |  7777 | `		/* Static member access using class name */` |
|      230 |  7778 | `		pNos = pTos;` |
|      230 |  7779 | `		pThis = 0;` |
|      230 |  7780 | `		if( !pInstr->p3 ){` |
|      192 |  7781 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  7782 | `			pNos--;` |
|        - |  7783 | `#ifdef UNTRUST` |
|        - |  7784 | `			if( pNos < pStack ){` |
|        - |  7785 | `				goto Abort;` |
|        - |  7786 | `			}` |
|        - |  7787 | `#endif` |
|       97 |  7788 | `		}else{` |
|        - |  7789 | `			/* Attribute name already computed */` |
|       40 |  7790 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7791 | `		}` |
|      230 |  7792 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      230 |  7793 | `			ph7_class *pClass = 0;` |
|      230 |  7794 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7795 | `				/* Class already instantiated */` |
|        5 |  7796 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7797 | `				pClass = pThis->pClass;` |
|        5 |  7798 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7799 | `			}else{` |
|        - |  7800 | `				/* Try to extract the target class */` |
|      226 |  7801 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      226 |  7802 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      226 |  7803 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7804 | `					/* Handle self/static/parent keywords */` |
|      226 |  7805 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7806 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7807 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7808 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7809 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7810 | `						}` |
|      196 |  7811 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7812 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      166 |  7813 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7814 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7815 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7816 | `							pClass = pSelf->pBase;` |
|       13 |  7817 | `						}` |
|       15 |  7818 | `					}else{` |
|      114 |  7819 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7820 | `					}` |
|      112 |  7821 | `				}` |
|        - |  7822 | `			}` |
|      230 |  7823 | `			if( pClass == 0 ){` |
|        - |  7824 | `				/* Undefined class */` |
|      ! 0 |  7825 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7826 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7827 | `					);` |
|      ! 0 |  7828 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7829 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7830 | `				}` |
|      ! 0 |  7831 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7832 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7833 | `			}else{` |
|      230 |  7834 | `				if( pInstr->iP2 ){` |
|        - |  7835 | `					/* Method call */` |
|       86 |  7836 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7837 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7838 | `						/* Extract the target method */` |
|       86 |  7839 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7840 | `					}` |
|       86 |  7841 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7842 | `						if( pMeth ){` |
|      ! 0 |  7843 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7844 | `								&pClass->sName,&sName` |
|        - |  7845 | `								);` |
|      ! 0 |  7846 | `						}else{` |
|      ! 0 |  7847 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7848 | `								&pClass->sName,&sName` |
|        - |  7849 | `								);` |
|        - |  7850 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7851 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7852 | `						}` |
|        - |  7853 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7854 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7855 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7856 | `						}` |
|      ! 0 |  7857 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7858 | `					}else{` |
|        - |  7859 | `						/* Push method name on the stack */` |
|       86 |  7860 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7861 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7862 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7863 | `					}` |
|       86 |  7864 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7865 | `				}else{` |
|        - |  7866 | `					/* Attribute access */` |
|      146 |  7867 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7868 | `					/* Check for special ::class pseudo-constant */` |
|      192 |  7869 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7870 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7871 | `						/* ::class returns the fully qualified class name */` |
|        - |  7872 | `						/* Pop the attribute name from the stack */` |
|       60 |  7873 | `						if( !pInstr->p3 ){` |
|       60 |  7874 | `							VmPopOperand(&pTos,1);` |
|       29 |  7875 | `						}` |
|       60 |  7876 | `						PH7_MemObjRelease(pTos);` |
|        - |  7877 | `						/* Load the class name */` |
|       60 |  7878 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7879 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7880 | `					}else{` |
|        - |  7881 | `						/* Extract the target attribute */` |
|       88 |  7882 | `						if( sName.nByte > 0 ){` |
|       88 |  7883 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       43 |  7884 | `						}` |
|       88 |  7885 | `						if( pAttr == 0 ){` |
|        - |  7886 | `							/* No such attribute,load null */` |
|      ! 0 |  7887 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7888 | `								&pClass->sName,&sName);` |
|        - |  7889 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7890 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7891 | `						}` |
|        - |  7892 | `						/* Pop the attribute name from the stack */` |
|       88 |  7893 | `						if( !pInstr->p3 ){` |
|       50 |  7894 | `							VmPopOperand(&pTos,1);` |
|       24 |  7895 | `						}` |
|       88 |  7896 | `						PH7_MemObjRelease(pTos);` |
|       88 |  7897 | `						pTos->nIdx = SXU32_HIGH;` |
|       88 |  7898 | `						if( pAttr ){` |
|       88 |  7899 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7900 | `								/* Access to a non static attribute */` |
|      ! 0 |  7901 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7902 | `									&pClass->sName,&pAttr->sName` |
|        - |  7903 | `									);` |
|      ! 0 |  7904 | `							}else{` |
|        - |  7905 | `								ph7_value *pValue;` |
|        - |  7906 | `								/* Check if the access to the attribute is allowed */` |
|       88 |  7907 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7908 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7909 | `									 * Same LHS-of-store peek as the instance path. */` |
|       82 |  7910 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       56 |  7911 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7912 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7913 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7914 | `										if( pS ){` |
|       28 |  7915 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7916 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7917 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7918 | `												int bIsLhs = 0;` |
|        8 |  7919 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7920 | `													bIsLhs = 1;` |
|        2 |  7921 | `												}` |
|        8 |  7922 | `												if( !bIsLhs ){` |
|        3 |  7923 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7924 | `													if( pThis ){` |
|      ! 0 |  7925 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7926 | `													}` |
|        3 |  7927 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7928 | `														goto Abort;` |
|        - |  7929 | `													}` |
|        - |  7930 | `													{` |
|        3 |  7931 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7932 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7933 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7934 | `															break;` |
|        - |  7935 | `														}` |
|        - |  7936 | `													}` |
|      ! 0 |  7937 | `													goto Exception;` |
|        - |  7938 | `												}` |
|        2 |  7939 | `											}` |
|       12 |  7940 | `										}` |
|       12 |  7941 | `									}` |
|        - |  7942 | `									/* Load the desired attribute */` |
|       82 |  7943 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       82 |  7944 | `									if( pValue ){` |
|       82 |  7945 | `										PH7_MemObjLoad(pValue,pTos);` |
|       82 |  7946 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7947 | `											/* Load index number */` |
|       38 |  7948 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7949 | `										}` |
|       40 |  7950 | `									}` |
|       42 |  7951 | `								}else{` |
|        - |  7952 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7953 | `									char zMsg[256];` |
|        5 |  7954 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7955 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7956 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7957 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7958 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7959 | `									}else{` |
|      ! 0 |  7960 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7961 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7962 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7963 | `									}` |
|        5 |  7964 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7965 | `									goto Abort;` |
|        - |  7966 | `								}` |
|        - |  7967 | `							}` |
|       40 |  7968 | `						}` |
|        - |  7969 | `					}` |
|        - |  7970 | `				}` |
|      224 |  7971 | `				if( pThis ){` |
|        - |  7972 | `					/* Safely unreference the object */` |
|        5 |  7973 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7974 | `				}` |
|        - |  7975 | `			}` |
|      113 |  7976 | `		}else{` |
|        - |  7977 | `			/* Pop operands */` |
|      ! 0 |  7978 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7979 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7980 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7981 | `			}` |
|      ! 0 |  7982 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7983 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7984 | `		}` |
|        - |  7985 | `	}` |
|     7954 |  7986 | `	break;` |
|        - |  7987 | `					}` |
|        - |  7988 | `/*` |
|        - |  7989 | ` * OP_NEW P1 * * *` |
|        - |  7990 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7991 | ` */` |
|      649 |  7992 | `case PH7_OP_NEW: {` |
|     1300 |  7993 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1300 |  7994 | `	ph7_class *pClass = 0;` |
|        - |  7995 | `	ph7_class_instance *pNew;` |
|     1300 |  7996 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7997 | `		/* Try to extract the desired class */` |
|     1949 |  7998 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1298 |  7999 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      649 |  8000 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8001 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8002 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8003 | `	}` |
|     1300 |  8004 | `	if( pClass == 0 ){` |
|        - |  8005 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8006 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8007 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8008 | `			);` |
|        - |  8009 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8010 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8011 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8012 | `			/* Pop given arguments */` |
|      ! 0 |  8013 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8014 | `		}` |
|      ! 0 |  8015 | `		goto Abort;` |
|      ! 0 |  8016 | `	}else{` |
|        - |  8017 | `		ph7_class_method *pCons;` |
|        - |  8018 | `		/* Create a new class instance */` |
|     1300 |  8019 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1300 |  8020 | `		if( pNew == 0 ){` |
|      ! 0 |  8021 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8022 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8023 | `				&pClass->sName` |
|        - |  8024 | `			);` |
|      ! 0 |  8025 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8026 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8027 | `				/* Pop given arguments */` |
|      ! 0 |  8028 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8029 | `			}` |
|      ! 0 |  8030 | `			break;` |
|        - |  8031 | `		}` |
|        - |  8032 | `		/* Check if a constructor is available */` |
|     1300 |  8033 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1300 |  8034 | `		if( pCons == 0 ){` |
|      928 |  8035 | `			SyString *pName = &pClass->sName;` |
|        - |  8036 | `			/* Check for a constructor with the same base class name */` |
|      928 |  8037 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      463 |  8038 | `		}` |
|     1300 |  8039 | `		if( pCons ){` |
|        - |  8040 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8041 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8042 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8043 | `			 * (including variadic string-key packing). */` |
|      374 |  8044 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8045 | `			sxi32 rcCons;` |
|      374 |  8046 | `			SySetReset(&aArg);` |
|      746 |  8047 | `			while( pArg < pTos ){` |
|      374 |  8048 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      374 |  8049 | `				pArg++;` |
|        2 |  8050 | `			}` |
|      374 |  8051 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8052 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8053 | `				sxu32 n;` |
|      114 |  8054 | `				n = SySetUsed(&aArg);` |
|        - |  8055 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8056 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8057 | `				 * after resolution). */` |
|      222 |  8058 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8059 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8060 | `					if( pFuncArg ){` |
|      110 |  8061 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8062 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8063 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8064 | `						}` |
|       54 |  8065 | `					}` |
|      110 |  8066 | `					n++;` |
|        2 |  8067 | `				}` |
|       56 |  8068 | `			}` |
|      374 |  8069 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8070 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      374 |  8071 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8072 | `				pNew->iRef = 1;` |
|      ! 0 |  8073 | `			}` |
|      374 |  8074 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8075 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8076 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8077 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8078 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8079 | `				sxi32 iResumePc;` |
|        5 |  8080 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8081 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8082 | `					goto Abort;` |
|        - |  8083 | `				}` |
|        5 |  8084 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8085 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8086 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8087 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8088 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8089 | `					}` |
|        5 |  8090 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8091 | `					pc = iResumePc;` |
|        5 |  8092 | `					break;` |
|        - |  8093 | `				}` |
|      ! 0 |  8094 | `				goto Exception;` |
|        - |  8095 | `			}` |
|      184 |  8096 | `		}` |
|     1296 |  8097 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8098 | `			/* Pop given arguments */` |
|      306 |  8099 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      152 |  8100 | `		}` |
|     1296 |  8101 | `		PH7_MemObjRelease(pTos);` |
|     1296 |  8102 | `		pTos->x.pOther = pNew;` |
|     1296 |  8103 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8104 | `	}` |
|     1296 |  8105 | `	break;` |
|        - |  8106 | `				 }` |
|        - |  8107 | `/*` |
|        - |  8108 | ` * OP_CLONE * * *` |
|        - |  8109 | ` * Perfome a clone operation.` |
|        - |  8110 | ` */` |
|       24 |  8111 | `case PH7_OP_CLONE: {` |
|        - |  8112 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8113 | `#ifdef UNTRUST` |
|        - |  8114 | `	if( pTos < pStack ){` |
|        - |  8115 | `		goto Abort;` |
|        - |  8116 | `	}` |
|        - |  8117 | `#endif` |
|        - |  8118 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8119 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8120 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8121 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8122 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8123 | `		break;` |
|        - |  8124 | `	}` |
|        - |  8125 | `	/* Point to the source */` |
|       46 |  8126 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8127 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8128 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8129 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8130 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8131 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8132 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8133 | `		break;` |
|        - |  8134 | `	}` |
|        - |  8135 | `	/* Perform the clone operation */` |
|       46 |  8136 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8137 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8138 | `	if( pClone == 0 ){` |
|      ! 0 |  8139 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8140 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8141 | `	}else{` |
|        - |  8142 | `		/* Load the cloned object */` |
|       46 |  8143 | `		pTos->x.pOther = pClone;` |
|       46 |  8144 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8145 | `	}` |
|       46 |  8146 | `	break;` |
|        - |  8147 | `				   }` |
|        - |  8148 | `/*` |
|        - |  8149 | ` * OP_SWITCH * * P3` |
|        - |  8150 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8151 | ` */` |
|       26 |  8152 | `case PH7_OP_SWITCH: {` |
|       54 |  8153 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8154 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8155 | `	ph7_value sValue,sCaseValue;` |
|        - |  8156 | `	sxu32 n,nEntry;` |
|        - |  8157 | `#ifdef UNTRUST` |
|        - |  8158 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8159 | `		goto Abort;` |
|        - |  8160 | `	}` |
|        - |  8161 | `#endif` |
|        - |  8162 | `	/* Point to the case table  */` |
|       54 |  8163 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8164 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8165 | `	/* Select the appropriate case block to execute */` |
|       54 |  8166 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8167 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8168 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8169 | `		pCase = &aCase[n];` |
|      130 |  8170 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8171 | `		/* Execute the case expression first */` |
|      130 |  8172 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8173 | `		/* Compare the two expression */` |
|      130 |  8174 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8175 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8176 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8177 | `		if( rc == 0 ){` |
|        - |  8178 | `			/* Value match,jump to this block */` |
|       52 |  8179 | `			pc = pCase->nStart - 1;` |
|       52 |  8180 | `			break;` |
|        - |  8181 | `		}` |
|       41 |  8182 | `	}` |
|       54 |  8183 | `	VmPopOperand(&pTos,1);` |
|       54 |  8184 | `	if( n >= nEntry ){` |
|        - |  8185 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8186 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8187 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8188 | `		}else{` |
|        - |  8189 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8190 | `			pc = pSwitch->nOut - 1;` |
|        - |  8191 | `		}` |
|        1 |  8192 | `	}` |
|       54 |  8193 | `	break;` |
|        - |  8194 | `					}` |
|        - |  8195 | `/*` |
|        - |  8196 | ` * OP_MATCH * * P3` |
|        - |  8197 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8198 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8199 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8200 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8201 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8202 | ` */` |
|       54 |  8203 | `case PH7_OP_MATCH: {` |
|      110 |  8204 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8205 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8206 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8207 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8208 | `	int matched = 0;` |
|        - |  8209 | `#ifdef UNTRUST` |
|        - |  8210 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8211 | `		goto Abort;` |
|        - |  8212 | `	}` |
|        - |  8213 | `#endif` |
|      110 |  8214 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8215 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8216 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8217 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8218 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8219 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8220 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8221 | `		pArm = &aArm[i];` |
|      240 |  8222 | `		if( pArm->bDefault ){` |
|       13 |  8223 | `			pDefault = pArm;` |
|       13 |  8224 | `			continue;` |
|        - |  8225 | `		}` |
|      228 |  8226 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8227 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8228 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8229 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8230 | `				continue;` |
|        - |  8231 | `			}` |
|      260 |  8232 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8233 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8234 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8235 | `			if( rc == 0 ){` |
|       93 |  8236 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8237 | `				matched = 1;` |
|       93 |  8238 | `				break;` |
|        - |  8239 | `			}` |
|       85 |  8240 | `		}` |
|      115 |  8241 | `	}` |
|      110 |  8242 | `	if( !matched && pDefault ){` |
|       13 |  8243 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8244 | `		matched = 1;` |
|        6 |  8245 | `	}` |
|      110 |  8246 | `	if( !matched ){` |
|        5 |  8247 | `		const char *zType = "unknown";` |
|        - |  8248 | `		char zMsg[128];` |
|        - |  8249 | `		sxu32 nMsg;` |
|        5 |  8250 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8251 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8252 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8253 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8254 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8255 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8256 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8257 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8258 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8259 | `		default: break;` |
|        - |  8260 | `		}` |
|        7 |  8261 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8262 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8263 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8264 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8265 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8266 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8267 | `		goto Abort;` |
|        - |  8268 | `	}` |
|      105 |  8269 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8270 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8271 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8272 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8273 | `	break;` |
|        - |  8274 | `					}` |
|        - |  8275 | `/*` |
|        - |  8276 | ` * OP_YIELD P1 P2 *` |
|        - |  8277 | ` *  Yield a value from a generator function.` |
|        - |  8278 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8279 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8280 | ` */` |
|       34 |  8281 | `case PH7_OP_YIELD: {` |
|        - |  8282 | `	ph7_generator *pGen;` |
|       70 |  8283 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8284 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8285 | `		goto Abort;` |
|        - |  8286 | `	}` |
|       70 |  8287 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8288 | `	if( pInstr->iP2 ){` |
|        - |  8289 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8290 | `#ifdef UNTRUST` |
|        - |  8291 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8292 | `#endif` |
|        7 |  8293 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8294 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8295 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8296 | `		VmPopOperand(&pTos, 1);` |
|        - |  8297 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8298 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8299 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8300 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8301 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8302 | `			}` |
|        1 |  8303 | `		}` |
|       67 |  8304 | `	}else if( pInstr->iP1 ){` |
|        - |  8305 | `		/* yield $value */` |
|        - |  8306 | `#ifdef UNTRUST` |
|        - |  8307 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8308 | `#endif` |
|       64 |  8309 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8310 | `		VmPopOperand(&pTos, 1);` |
|        - |  8311 | `		/* Auto-increment key */` |
|       64 |  8312 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8313 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8314 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8315 | `	}else{` |
|        - |  8316 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8317 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8318 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8319 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8320 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8321 | `	}` |
|        - |  8322 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8323 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8324 | `	goto Suspend;` |
|        - |  8325 |  |
|        - |  8326 | `/*` |
|        - |  8327 | ` * OP_CALL P1 * *` |
|        - |  8328 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8329 | ` *  function on the stack.` |
|        - |  8330 | ` */` |
|   356871 |  8331 | `case PH7_OP_CALL: {` |
|   713788 |  8332 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8333 | `	ph7_value *pArg;` |
|   713788 |  8334 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   713788 |  8335 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8336 | `	SyHashEntry *pEntry;` |
|        - |  8337 | `	SyString sName;` |
|        - |  8338 | `	/* Extract function name */` |
|   713788 |  8339 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8340 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8341 | `			ph7_value sResult;` |
|        - |  8342 | `			sxi32 rcArr;` |
|        3 |  8343 | `			SySetReset(&aArg);` |
|        3 |  8344 | `			while( pArg < pTos ){` |
|      ! 0 |  8345 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8346 | `				pArg++;` |
|      ! 0 |  8347 | `			}` |
|        3 |  8348 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8349 | `			/* May be a class instance and it's static method */` |
|        3 |  8350 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8351 | `			SySetReset(&aArg);` |
|        - |  8352 | `			/* Pop given arguments */` |
|        3 |  8353 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8354 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8355 | `			}` |
|        3 |  8356 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8357 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8358 | `				goto Abort;` |
|        - |  8359 | `			}` |
|        3 |  8360 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8361 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8362 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8363 | `				sxi32 iResumePc;` |
|        3 |  8364 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8365 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8366 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8367 | `					pc = iResumePc;` |
|        3 |  8368 | `					break;` |
|        - |  8369 | `				}` |
|      ! 0 |  8370 | `				goto Exception;` |
|        - |  8371 | `			}` |
|        - |  8372 | `			/* Copy result */` |
|      ! 0 |  8373 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8374 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8375 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8376 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8377 | `			ph7_value sResult;` |
|        - |  8378 | `			sxi32 rcInv;` |
|       84 |  8379 | `			SySetReset(&aArg);` |
|      200 |  8380 | `			while( pArg < pTos ){` |
|      118 |  8381 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8382 | `				pArg++;` |
|        2 |  8383 | `			}` |
|       84 |  8384 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8385 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8386 | `				(int)SySetUsed(&aArg),` |
|       82 |  8387 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8388 | `				&sResult,` |
|       82 |  8389 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8390 | `			SySetReset(&aArg);` |
|       84 |  8391 | `			if( nCallArgs > 0 ){` |
|       76 |  8392 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8393 | `			}` |
|       84 |  8394 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8395 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8396 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8397 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8398 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8399 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8400 | `				pThis->iRef++;` |
|       13 |  8401 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8402 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8403 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8404 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8405 | `					goto Abort;` |
|        - |  8406 | `				}` |
|        - |  8407 | `				{` |
|       13 |  8408 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8409 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8410 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8411 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8412 | `						break;` |
|        - |  8413 | `					}` |
|        - |  8414 | `				}` |
|      ! 0 |  8415 | `				goto Exception;` |
|        - |  8416 | `			}` |
|       72 |  8417 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8418 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8419 | `				goto Abort;` |
|        - |  8420 | `			}` |
|       72 |  8421 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8422 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8423 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8424 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8425 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8426 | `				sxi32 iResumePc;` |
|        7 |  8427 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8428 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8429 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8430 | `					pc = iResumePc;` |
|        5 |  8431 | `					break;` |
|        - |  8432 | `				}` |
|        3 |  8433 | `				goto Exception;` |
|        - |  8434 | `			}` |
|       66 |  8435 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8436 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8437 | `		}else{` |
|        - |  8438 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8439 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8440 | `			/* Pop given arguments */` |
|      ! 0 |  8441 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8442 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8443 | `			}` |
|        - |  8444 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8445 | `			PH7_MemObjRelease(pTos);` |
|        - |  8446 | `		}` |
|       66 |  8447 | `		break;` |
|        - |  8448 | `	}` |
|   713704 |  8449 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8450 | `	/* Check for a compiled function first.` |
|        - |  8451 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8452 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   713704 |  8453 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8454 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8455 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8456 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8457 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8458 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8459 | `	{` |
|   713704 |  8460 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   713704 |  8461 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8462 | `		const char *zFunc;` |
|        - |  8463 | `		const char *zEnd;` |
|        - |  8464 | `		const char *z;` |
|        - |  8465 | `		SyString sGlobal;` |
|       22 |  8466 | `		zFunc = sName.zString;` |
|       22 |  8467 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8468 | `		z = zEnd;` |
|        - |  8469 | `		/* Find last namespace separator */` |
|      194 |  8470 | `		while( z > zFunc ){` |
|      194 |  8471 | `			if( z[-1] == '\\' ){` |
|       22 |  8472 | `				break;` |
|        - |  8473 | `			}` |
|      174 |  8474 | `			z--;` |
|        2 |  8475 | `		}` |
|       22 |  8476 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8477 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8478 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8479 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8480 | `		}` |
|       10 |  8481 | `	}` |
|        - |  8482 | `	} /* end VmCallArgMap namespace scope */` |
|   713704 |  8483 | `	if( pEntry ){` |
|        - |  8484 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8485 | `		ph7_class_instance *pThis;` |
|        - |  8486 | `		ph7_value *pFrameStack;` |
|        - |  8487 | `		ph7_vm_func *pVmFunc;` |
|        - |  8488 | `		ph7_class *pSelf;` |
|        - |  8489 | `		VmFrame *pFrame;` |
|        - |  8490 | `		ph7_value *pObj;` |
|        - |  8491 | `		VmSlot sArg;` |
|        - |  8492 | `		sxu32 n;` |
|        - |  8493 | `		/* initialize fields */` |
|    18458 |  8494 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18458 |  8495 | `		pThis = 0;` |
|    18458 |  8496 | `		pSelf = 0;` |
|    18458 |  8497 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8498 | `			ph7_class_method *pMeth;` |
|        - |  8499 | `			/* Class method call */` |
|     3322 |  8500 | `			ph7_value *pTarget = &pTos[-1];` |
|     3322 |  8501 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8502 | `				/* Extract the 'this' pointer */` |
|     3322 |  8503 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8504 | `					/* Instance already loaded */` |
|     3232 |  8505 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3232 |  8506 | `					pThis->iRef++;` |
|     3232 |  8507 | `					pSelf = pThis->pClass;` |
|     1615 |  8508 | `				}` |
|     3322 |  8509 | `				if( pSelf == 0 ){` |
|       92 |  8510 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8511 | `						/* "Late Static Binding" class name */` |
|      128 |  8512 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8513 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8514 | `					}` |
|       92 |  8515 | `					if( pSelf == 0 ){` |
|       21 |  8516 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8517 | `					}` |
|       45 |  8518 | `				}` |
|     3322 |  8519 | `				if( pThis == 0  ){` |
|       92 |  8520 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8521 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8522 | `					if( pFrameLocal->pParent ){` |
|        - |  8523 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8524 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8525 | `						if( pThis ){` |
|       21 |  8526 | `							pThis->iRef++;` |
|       10 |  8527 | `						}` |
|       32 |  8528 | `					}` |
|       45 |  8529 | `				}` |
|     3322 |  8530 | `				VmPopOperand(&pTos,1);` |
|     3322 |  8531 | `				PH7_MemObjRelease(pTos);` |
|        - |  8532 | `				/* Synchronize pointers */` |
|     3322 |  8533 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8534 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8535 | `				 * user have already computed the random generated unique class method name` |
|        - |  8536 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8537 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8538 | `				 */` |
|     3322 |  8539 | `				while( pArg < pStack ){` |
|      ! 0 |  8540 | `					pArg++;` |
|      ! 0 |  8541 | `				}` |
|     3322 |  8542 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8543 | `					/* Check if the call is allowed */` |
|     3322 |  8544 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3322 |  8545 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8546 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8547 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8548 | `							char zMsg[256];` |
|      ! 0 |  8549 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8550 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8551 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8552 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8553 | `							/* Pop given arguments */` |
|      ! 0 |  8554 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8555 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8556 | `							}` |
|      ! 0 |  8557 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8558 | `							goto Abort;` |
|        - |  8559 | `						}` |
|        6 |  8560 | `					}` |
|     1660 |  8561 | `				}` |
|     1660 |  8562 | `			}` |
|     1660 |  8563 | `		}` |
|        - |  8564 | `		/* Check The recursion limit */` |
|    18458 |  8565 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8566 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8567 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8568 | `				&pVmFunc->sName);` |
|        - |  8569 | `			/* Pop given arguments */` |
|        3 |  8570 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8571 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8572 | `			}` |
|        - |  8573 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8574 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8575 | `			break;` |
|        - |  8576 | `		}` |
|    18456 |  8577 | `		if( pVmFunc->pNextName ){` |
|        - |  8578 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8579 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8580 | `		}` |
|    18456 |  8581 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8582 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8583 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8584 | `			ph7_generator *pGenerator;` |
|        - |  8585 | `			ph7_class_instance *pGenObj;` |
|        - |  8586 | `			ph7_value *pCtxAttr;` |
|        - |  8587 | `			SyString sAttrName;` |
|        - |  8588 | `			ph7_value **apCallArgs;` |
|        - |  8589 | `			int nGenArgs, iArg;` |
|        - |  8590 | `			/* Collect arguments from the operand stack */` |
|       24 |  8591 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8592 | `			apCallArgs = 0;` |
|       24 |  8593 | `			if( nGenArgs > 0 ){` |
|       14 |  8594 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8595 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8596 | `				if( apCallArgs == 0 ){` |
|        - |  8597 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8598 | `					nGenArgs = 0;` |
|      ! 0 |  8599 | `				}else{` |
|       10 |  8600 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8601 | `					int didReorder = 0;` |
|       10 |  8602 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8603 | `						/* Named-argument reordering for generator */` |
|        5 |  8604 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8605 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8606 | `						sxu32 nNV = nF;` |
|        5 |  8607 | `						sxi32 iVIdx = -1;` |
|        - |  8608 | `						sxi32 *aGSlot;` |
|        - |  8609 | `						sxu8 *aGUsed;` |
|        - |  8610 | `						sxu32 gi;` |
|       13 |  8611 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8612 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8613 | `						}` |
|        7 |  8614 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8615 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8616 | `						if( aGSlot ){` |
|        5 |  8617 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8618 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8619 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8620 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8621 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8622 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8623 | `								goto Abort;` |
|        - |  8624 | `							}` |
|        - |  8625 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8626 | `							 * append overflow (variadic / positional beyond` |
|        - |  8627 | `							 * formals) so downstream sees every argument. */` |
|        - |  8628 | `							{` |
|        5 |  8629 | `								int nOut = 0;` |
|       13 |  8630 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8631 | `									sxu32 gj;` |
|       13 |  8632 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8633 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8634 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8635 | `											break;` |
|        - |  8636 | `										}` |
|        3 |  8637 | `									}` |
|        5 |  8638 | `								}` |
|       13 |  8639 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8640 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8641 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8642 | `									}` |
|        5 |  8643 | `								}` |
|        5 |  8644 | `								nGenArgs = nOut;` |
|        - |  8645 | `							}` |
|        5 |  8646 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8647 | `							didReorder = 1;` |
|        2 |  8648 | `						}` |
|        - |  8649 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8650 | `						 * positional fill below — preserves arg order rather` |
|        - |  8651 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8652 | `					}` |
|       10 |  8653 | `					if( !didReorder ){` |
|       12 |  8654 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8655 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8656 | `						}` |
|        2 |  8657 | `					}` |
|        - |  8658 | `				}` |
|        4 |  8659 | `			}` |
|        - |  8660 | `			/* Create execution context and generator wrapper */` |
|       24 |  8661 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8662 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8663 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8664 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8665 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8666 | `				break;` |
|        - |  8667 | `			}` |
|       24 |  8668 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8669 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8670 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8671 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8672 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8673 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8674 | `				break;` |
|        - |  8675 | `			}` |
|        - |  8676 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8677 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8678 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8679 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8680 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8681 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8682 | `			if( apCallArgs ){` |
|       10 |  8683 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8684 | `			}` |
|       24 |  8685 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8686 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8687 | `				if( pThis ){` |
|      ! 0 |  8688 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8689 | `				}` |
|      ! 0 |  8690 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8691 | `					goto Abort;` |
|        - |  8692 | `				}` |
|      ! 0 |  8693 | `				break;` |
|        - |  8694 | `			}` |
|        - |  8695 | `			/* Create Generator class instance */` |
|       24 |  8696 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8697 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8698 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8699 | `				break;` |
|        - |  8700 | `			}` |
|        - |  8701 | `			/* Store generator in __ctx attribute */` |
|       24 |  8702 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8703 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8704 | `			if( pCtxAttr ){` |
|       24 |  8705 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8706 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8707 | `			}` |
|        - |  8708 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8709 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8710 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8711 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8712 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8713 | `			pGenObj->iRef++;` |
|       24 |  8714 | `			if( pThis ){` |
|      ! 0 |  8715 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8716 | `			}` |
|       24 |  8717 | `			break;` |
|        - |  8718 | `		}` |
|        - |  8719 | `		/* Extract the formal argument set */` |
|    18434 |  8720 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8721 | `		/* Create a new VM frame  */` |
|    18434 |  8722 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18434 |  8723 | `		if( rc != SXRET_OK ){` |
|        - |  8724 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8725 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8726 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8727 | `				&pVmFunc->sName);` |
|        - |  8728 | `			/* Pop given arguments */` |
|      ! 0 |  8729 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8730 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8731 | `			}` |
|        - |  8732 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8733 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8734 | `			break;` |
|        - |  8735 | `		}` |
|    18434 |  8736 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8737 | `			/* Install the '$this' variable */` |
|        - |  8738 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3250 |  8739 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3250 |  8740 | `			if( pObj ){` |
|        - |  8741 | `				/* Reflect the change */` |
|     3250 |  8742 | `				pObj->x.pOther = pThis;` |
|     3250 |  8743 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1624 |  8744 | `			}` |
|     1624 |  8745 | `		}` |
|    18434 |  8746 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8747 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8748 | `			/* Install static variables */` |
|      ! 0 |  8749 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8750 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8751 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8752 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8753 | `					/* Initialize the static variables */` |
|      ! 0 |  8754 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8755 | `					if( pObj ){` |
|        - |  8756 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8757 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8758 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8759 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8760 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8761 | `						}` |
|      ! 0 |  8762 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8763 | `					}else{` |
|      ! 0 |  8764 | `						continue;` |
|        - |  8765 | `					}` |
|      ! 0 |  8766 | `				}` |
|        - |  8767 | `				/* Install in the current frame */` |
|      ! 0 |  8768 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8769 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8770 | `			}` |
|      ! 0 |  8771 | `		}` |
|        - |  8772 | `		/* Push arguments in the local frame */` |
|        - |  8773 | `		{` |
|    18434 |  8774 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8775 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8776 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18434 |  8777 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18434 |  8778 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8779 | `			/* ============================================================` |
|        - |  8780 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8781 | `			 *` |
|        - |  8782 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8783 | `			 * or position, then install them in the frame.` |
|        - |  8784 | `			 * ============================================================ */` |
|       96 |  8785 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8786 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8787 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8788 | `			sxu32 nNonVariadic;` |
|        - |  8789 | `			sxi32 *aSlot;` |
|        - |  8790 | `			sxu8  *aUsed;` |
|        - |  8791 | `			sxu32 i;` |
|        - |  8792 | `			/* Find variadic parameter index */` |
|      292 |  8793 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8794 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8795 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8796 | `					break;` |
|        - |  8797 | `				}` |
|      100 |  8798 | `			}` |
|       96 |  8799 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8800 | `			/* Allocate mapping arrays */` |
|      143 |  8801 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8802 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8803 | `			if( aSlot == 0 ){` |
|      ! 0 |  8804 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8805 | `				goto Abort;` |
|        - |  8806 | `			}` |
|       96 |  8807 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8808 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8809 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8810 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8811 | `			if( rc == PH7_ABORT ){` |
|        7 |  8812 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8813 | `				goto Abort;` |
|        - |  8814 | `			}` |
|        - |  8815 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8816 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8817 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8818 | `				sxi32 iSrc = -1;` |
|      309 |  8819 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8820 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8821 | `						iSrc = (sxi32)i;` |
|      169 |  8822 | `						break;` |
|        - |  8823 | `					}` |
|       62 |  8824 | `				}` |
|      187 |  8825 | `				if( iSrc >= 0 ){` |
|        - |  8826 | `					/* Argument was provided — install with type checking */` |
|      169 |  8827 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8828 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8829 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8830 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8831 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8832 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8833 | `					}` |
|        - |  8834 | `					/* Type checking: union types */` |
|      169 |  8835 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8836 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8837 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8838 | `							bCallIsStrict);` |
|       13 |  8839 | `						if( rcU != SXRET_OK ){` |
|        - |  8840 | `							const char *zGiven;` |
|      ! 0 |  8841 | `							const char *zExpected = "union";` |
|        - |  8842 | `							char zBuf[128];` |
|        - |  8843 | `							char zTypeBuf[128];` |
|      ! 0 |  8844 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8845 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8846 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8847 | `								zGiven = "null";` |
|      ! 0 |  8848 | `							}else{` |
|      ! 0 |  8849 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8850 | `							}` |
|      ! 0 |  8851 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8852 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8853 | `							}` |
|      ! 0 |  8854 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8855 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8856 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8857 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8858 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8859 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8860 | `							pFrameStack = 0;` |
|      ! 0 |  8861 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8862 | `							goto SkipFuncBody;` |
|        - |  8863 | `						}` |
|      171 |  8864 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8865 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8866 | `						/* Scalar/class type checking */` |
|       17 |  8867 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8868 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8869 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8870 | `							if( pClass ){` |
|      ! 0 |  8871 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8872 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8873 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8874 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8875 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8876 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8877 | `									}` |
|      ! 0 |  8878 | `								}else{` |
|      ! 0 |  8879 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8880 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8881 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8882 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8883 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8884 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8885 | `									}` |
|        - |  8886 | `								}` |
|      ! 0 |  8887 | `							}` |
|       17 |  8888 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8889 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8890 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8891 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8892 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8893 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8894 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8895 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8896 | `								pFrameStack = 0;` |
|      ! 0 |  8897 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8898 | `								goto SkipFuncBody;` |
|        7 |  8899 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8900 | `								char zTypeBuf[128];` |
|      ! 0 |  8901 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8902 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8903 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8904 | `									ph7_type_name(pVal));` |
|      ! 0 |  8905 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8906 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8907 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8908 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8909 | `								pFrameStack = 0;` |
|      ! 0 |  8910 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8911 | `								goto SkipFuncBody;` |
|        - |  8912 | `							}` |
|        3 |  8913 | `						}` |
|        8 |  8914 | `					}` |
|        - |  8915 | `					/* Install: by reference or by value */` |
|      169 |  8916 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8917 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8918 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8919 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8920 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8921 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8922 | `							}` |
|      ! 0 |  8923 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8924 | `						}else{` |
|        7 |  8925 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8926 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8927 | `							if( pRefEntry == 0 ){` |
|        7 |  8928 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8929 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8930 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8931 | `								sArg.pUserData = 0;` |
|        5 |  8932 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8933 | `							}` |
|        5 |  8934 | `							pObj = 0;` |
|        - |  8935 | `						}` |
|        3 |  8936 | `					}else{` |
|      165 |  8937 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8938 | `					}` |
|      169 |  8939 | `					if( pObj ){` |
|      165 |  8940 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8941 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8942 | `						sArg.pUserData = 0;` |
|      165 |  8943 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8944 | `					}` |
|       85 |  8945 | `				}else{` |
|        - |  8946 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8947 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8948 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8949 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8950 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8951 | `						if( pObj ){` |
|       19 |  8952 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8953 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8954 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8955 | `							sArg.pUserData = 0;` |
|       19 |  8956 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8957 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8958 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8959 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8960 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8961 | `							}` |
|        9 |  8962 | `						}` |
|        9 |  8963 | `					}` |
|        - |  8964 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8965 | `				}` |
|       94 |  8966 | `			}` |
|        - |  8967 | `			/* Handle variadic parameter */` |
|       89 |  8968 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8969 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8970 | `				if( pObj ){` |
|        9 |  8971 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8972 | `					{` |
|        9 |  8973 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8974 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8975 | `							if( aSlot[i] == -1 ){` |
|       16 |  8976 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8977 | `									/* Named variadic entry: insert with string key */` |
|        - |  8978 | `									ph7_value sKey;` |
|       11 |  8979 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8980 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8981 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8982 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8983 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8984 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8985 | `								}else{` |
|        - |  8986 | `									/* Positional variadic entry */` |
|      ! 0 |  8987 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8988 | `								}` |
|        5 |  8989 | `							}` |
|       12 |  8990 | `						}` |
|        - |  8991 | `					}` |
|        9 |  8992 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8993 | `					sArg.pUserData = 0;` |
|        9 |  8994 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8995 | `				}` |
|        5 |  8996 | `			}else{` |
|        - |  8997 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8998 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8999 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9000 | `				 * the positional-only path's behavior. */` |
|       81 |  9001 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9002 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9003 | `					if( aSlot[i] == -2 ){` |
|        - |  9004 | `						char zAnonBuf[32];` |
|        - |  9005 | `						SyString sAnonName;` |
|      ! 0 |  9006 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9007 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9008 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9009 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9010 | `						if( pObj ){` |
|      ! 0 |  9011 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9012 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9013 | `							sArg.pUserData = 0;` |
|      ! 0 |  9014 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9015 | `						}` |
|      ! 0 |  9016 | `						nAnon++;` |
|      ! 0 |  9017 | `					}` |
|       79 |  9018 | `				}` |
|        - |  9019 | `			}` |
|        - |  9020 | `			/* Release all stack arguments */` |
|      267 |  9021 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9022 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9023 | `			}` |
|       89 |  9024 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9025 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9026 | `			n = nFormal;` |
|       45 |  9027 | `		}else{` |
|        - |  9028 | `		/* ============================================================` |
|        - |  9029 | `		 * Positional-only matching path (original)` |
|        - |  9030 | `		 * ============================================================ */` |
|    18340 |  9031 | `		n = 0;` |
|    48864 |  9032 | `		while( pArg < pTos ){` |
|    30598 |  9033 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9034 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9035 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9036 | `				if( pObj ){` |
|        - |  9037 | `					/* Initialize as empty array */` |
|       40 |  9038 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9039 | `					{` |
|       40 |  9040 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9041 | `						while( pArg < pTos ){` |
|        - |  9042 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9043 | `							 *` |
|        - |  9044 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9045 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9046 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9047 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9048 | `							 * fixing both wants a separate counter for elements` |
|        - |  9049 | `							 * already packed into the variadic array. */` |
|      114 |  9050 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9051 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9052 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9053 | `									bCallIsStrict);` |
|       16 |  9054 | `								if( rcU != SXRET_OK ){` |
|        - |  9055 | `									const char *zGiven;` |
|        3 |  9056 | `									const char *zExpected = "union";` |
|        - |  9057 | `									char zBuf[128];` |
|        - |  9058 | `									char zTypeBuf[128];` |
|        3 |  9059 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9060 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9061 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9062 | `										zGiven = "null";` |
|      ! 0 |  9063 | `									}else{` |
|        3 |  9064 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9065 | `									}` |
|        3 |  9066 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9067 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9068 | `									}` |
|        4 |  9069 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9070 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9071 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9072 | `										goto Abort;` |
|        - |  9073 | `									}` |
|        3 |  9074 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9075 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9076 | `									pFrameStack = 0;` |
|        3 |  9077 | `									rc = PH7_EXCEPTION;` |
|        3 |  9078 | `									goto SkipFuncBody;` |
|        - |  9079 | `								}` |
|       14 |  9080 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9081 | `								pArg++;` |
|       14 |  9082 | `								continue;` |
|        - |  9083 | `							}` |
|        - |  9084 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9085 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9086 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9087 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9088 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9089 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9090 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9091 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9092 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9093 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9094 | `										goto Abort;` |
|        - |  9095 | `									}` |
|        - |  9096 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9097 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9098 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9099 | `									pFrameStack = 0;` |
|      ! 0 |  9100 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9101 | `									goto SkipFuncBody;` |
|       13 |  9102 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9103 | `									char zTypeBuf[128];` |
|      ! 0 |  9104 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9105 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9106 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9107 | `										ph7_type_name(pArg));` |
|      ! 0 |  9108 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9109 | `										goto Abort;` |
|        - |  9110 | `									}` |
|      ! 0 |  9111 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9112 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9113 | `									pFrameStack = 0;` |
|      ! 0 |  9114 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9115 | `									goto SkipFuncBody;` |
|        - |  9116 | `								}` |
|        6 |  9117 | `							}` |
|      100 |  9118 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9119 | `							pArg++;` |
|        2 |  9120 | `						}` |
|        - |  9121 | `					}` |
|       38 |  9122 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9123 | `					sArg.pUserData = 0;` |
|       38 |  9124 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9125 | `				}` |
|       38 |  9126 | `				break; /* All remaining args consumed */` |
|        - |  9127 | `			}` |
|    30560 |  9128 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30342 |  9129 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9130 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9131 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9132 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9133 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9134 | `						goto Abort;` |
|        - |  9135 | `					}` |
|      ! 0 |  9136 | `				}` |
|        - |  9137 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30344 |  9138 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9139 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9140 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9141 | `						bCallIsStrict);` |
|       60 |  9142 | `					if( rcU != SXRET_OK ){` |
|        - |  9143 | `						const char *zGiven;` |
|       19 |  9144 | `						const char *zExpected = "union";` |
|        - |  9145 | `						char zBuf[128];` |
|        - |  9146 | `						char zTypeBuf[128];` |
|       19 |  9147 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9148 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9149 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9150 | `							zGiven = "null";` |
|        5 |  9151 | `						}else{` |
|        5 |  9152 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9153 | `						}` |
|       19 |  9154 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9155 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9156 | `						}` |
|       28 |  9157 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9158 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9159 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9160 | `							goto Abort;` |
|        - |  9161 | `						}` |
|       19 |  9162 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9163 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9164 | `						pFrameStack = 0;` |
|       19 |  9165 | `						rc = PH7_EXCEPTION;` |
|       19 |  9166 | `						goto SkipFuncBody;` |
|        - |  9167 | `					}` |
|       21 |  9168 | `				}else` |
|        - |  9169 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9170 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30310 |  9171 | `				if( aFormalArg[n].nType > 0` |
|    15859 |  9172 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9173 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9174 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9175 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9176 | `						ph7_class *pClass;` |
|        - |  9177 | `						/* Try to extract the desired class */` |
|       26 |  9178 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9179 | `						if( pClass ){` |
|       22 |  9180 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9181 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9182 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9183 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9184 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9185 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9186 | `								}` |
|      ! 0 |  9187 | `							}else{` |
|        - |  9188 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9189 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9190 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9191 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9192 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9193 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9194 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9195 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9196 | `								}` |
|        - |  9197 | `							}` |
|       12 |  9198 | `						}` |
|     1394 |  9199 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9200 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9201 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9202 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9203 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9204 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9205 | `								goto Abort;` |
|        - |  9206 | `							}` |
|        - |  9207 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9208 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9209 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9210 | `							pFrameStack = 0;` |
|       11 |  9211 | `							rc = PH7_EXCEPTION;` |
|       11 |  9212 | `							goto SkipFuncBody;` |
|       16 |  9213 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9214 | `							char zTypeBuf[128];` |
|       11 |  9215 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9216 | `								&aFormalArg[n].sName,` |
|        6 |  9217 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9218 | `								ph7_type_name(pArg));` |
|        8 |  9219 | `							if( rc == PH7_ABORT ){` |
|        5 |  9220 | `								goto Abort;` |
|        - |  9221 | `							}` |
|        3 |  9222 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9223 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9224 | `							pFrameStack = 0;` |
|        3 |  9225 | `							rc = PH7_EXCEPTION;` |
|        3 |  9226 | `							goto SkipFuncBody;` |
|        - |  9227 | `						}` |
|        4 |  9228 | `					}` |
|      694 |  9229 | `				}` |
|    30310 |  9230 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9231 | `					/* Pass by reference */` |
|       58 |  9232 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9233 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9234 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9235 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9236 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9237 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9238 | `						}` |
|        - |  9239 | `						/* Switch to pass by value */` |
|      ! 0 |  9240 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9241 | `					}else{` |
|        - |  9242 | `						SyHashEntry *pRefEntry;` |
|        - |  9243 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9244 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9245 | `						if( pRefEntry == 0 ){` |
|       86 |  9246 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9247 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9248 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9249 | `							sArg.pUserData = 0;` |
|       58 |  9250 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9251 | `						}` |
|       58 |  9252 | `						pObj = 0;` |
|        - |  9253 | `					}` |
|       30 |  9254 | `				}else{` |
|        - |  9255 | `					/* Pass by value,make a copy of the given argument */` |
|    30254 |  9256 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9257 | `				}` |
|    15156 |  9258 | `			}else{` |
|        - |  9259 | `				char zName[32];` |
|        - |  9260 | `				SyString sArgName;` |
|        - |  9261 | `				/* Set a dummy name */` |
|      218 |  9262 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9263 | `				sArgName.zString = zName;` |
|        - |  9264 | `				/* Annonymous argument */` |
|      218 |  9265 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9266 | `			}` |
|    30526 |  9267 | `			if( pObj ){` |
|    30470 |  9268 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9269 | `				/* Insert argument index  */` |
|    30470 |  9270 | `				sArg.nIdx = pObj->nIdx;` |
|    30470 |  9271 | `				sArg.pUserData = 0;` |
|    30470 |  9272 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15234 |  9273 | `			}` |
|    30526 |  9274 | `			PH7_MemObjRelease(pArg);` |
|    30526 |  9275 | `			pArg++;` |
|    30526 |  9276 | `			++n;` |
|        2 |  9277 | `		}` |
|        - |  9278 | `		} /* end named vs positional branch */` |
|        - |  9279 | `		/* Set up closure environment */` |
|    18392 |  9280 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9281 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9282 | `			ph7_value *pValue;` |
|        - |  9283 | `			sxu32 iEnv;` |
|      178 |  9284 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      422 |  9285 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      246 |  9286 | `				pEnv = &aEnv[iEnv];` |
|      246 |  9287 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9288 | `					/* Do not install null value */` |
|      172 |  9289 | `					continue;` |
|        - |  9290 | `				}` |
|       76 |  9291 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9292 | `				if( pValue == 0 ){` |
|      ! 0 |  9293 | `					continue;` |
|        - |  9294 | `				}` |
|        - |  9295 | `				/* Invalidate any prior representation */` |
|       76 |  9296 | `				PH7_MemObjRelease(pValue);` |
|        - |  9297 | `				/* Duplicate bound variable value */` |
|       76 |  9298 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9299 | `			}` |
|       88 |  9300 | `		}` |
|        - |  9301 | `		/* Process default values for remaining formal parameters */` |
|    21216 |  9302 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2872 |  9303 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9304 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9305 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9306 | `				if( pObj ){` |
|       48 |  9307 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9308 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9309 | `					sArg.pUserData = 0;` |
|       48 |  9310 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9311 | `				}` |
|       48 |  9312 | `				n++;` |
|       48 |  9313 | `				break; /* Variadic is always last */` |
|        - |  9314 | `			}` |
|     2826 |  9315 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2820 |  9316 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2820 |  9317 | `				if( pObj ){` |
|        - |  9318 | `					/* Evaluate the default value and extract it's result */` |
|     2820 |  9319 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2820 |  9320 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9321 | `						goto Abort;` |
|        - |  9322 | `					}` |
|        - |  9323 | `					/* Insert argument index */` |
|     2820 |  9324 | `					sArg.nIdx = pObj->nIdx;` |
|     2820 |  9325 | `					sArg.pUserData = 0;` |
|     2820 |  9326 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9327 | `					/* Make sure the default argument is of the correct type */` |
|     2818 |  9328 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1840 |  9329 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9330 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9331 | `						/* Cast to the desired type */` |
|        3 |  9332 | `						xCast(pObj);` |
|        1 |  9333 | `					}` |
|     1409 |  9334 | `				}` |
|     1409 |  9335 | `			}` |
|     2826 |  9336 | `			++n;` |
|        2 |  9337 | `		}` |
|        - |  9338 | `		} /* end VmCallArgMap scope */` |
|        - |  9339 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9340 | `		 * does not return anything.` |
|        - |  9341 | `		 */` |
|    18392 |  9342 | `		PH7_MemObjRelease(pTos);` |
|    18392 |  9343 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9344 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18392 |  9345 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18392 |  9346 | `		if( pFrameStack == 0 ){` |
|        - |  9347 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9348 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9349 | `				&pVmFunc->sName);` |
|      ! 0 |  9350 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9351 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9352 | `			}` |
|      ! 0 |  9353 | `			break;` |
|        - |  9354 | `		}` |
|     9195 |  9355 | `SkipFuncBody:` |
|    18424 |  9356 | `		if( pSelf ){` |
|        - |  9357 | `			/* Push class name */` |
|     3320 |  9358 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1659 |  9359 | `		}` |
|        - |  9360 | `		/* Increment nesting level */` |
|    18424 |  9361 | `		pVm->nRecursionDepth++;` |
|    18424 |  9362 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9363 | `			/* Execute function body */` |
|    27587 |  9364 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18390 |  9365 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9195 |  9366 | `		}` |
|        - |  9367 | `		/* Decrement nesting level */` |
|    18424 |  9368 | `		pVm->nRecursionDepth--;` |
|    18424 |  9369 | `		if( pSelf ){` |
|        - |  9370 | `			/* Pop class name */` |
|     3320 |  9371 | `			(void)SySetPop(&pVm->aSelf);` |
|     1659 |  9372 | `		}` |
|        - |  9373 | `		/* Cleanup the mess left behind */` |
|    18424 |  9374 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9375 | `			/* Return by reference,reflect that */` |
|        9 |  9376 | `			if( n != SXU32_HIGH ){` |
|        9 |  9377 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9378 | `				sxu32 i;` |
|        - |  9379 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9380 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9381 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9382 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9383 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9384 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9385 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9386 | `								&pVmFunc->sName);` |
|      ! 0 |  9387 | `						}` |
|      ! 0 |  9388 | `						n = SXU32_HIGH;` |
|      ! 0 |  9389 | `						break;` |
|        - |  9390 | `					}` |
|        3 |  9391 | `				}` |
|        5 |  9392 | `			}else{` |
|      ! 0 |  9393 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9394 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9395 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9396 | `						&pVmFunc->sName);` |
|      ! 0 |  9397 | `				}` |
|        - |  9398 | `			}` |
|        9 |  9399 | `			pTos->nIdx = n;` |
|        4 |  9400 | `		}` |
|        - |  9401 | `		/* Cleanup the mess left behind */` |
|    18424 |  9402 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9403 | `			/* An exception was throw in this frame */` |
|      100 |  9404 | `			pFrame = pFrame->pParent;` |
|      100 |  9405 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9406 | `				/* Pop the resutlt */` |
|       62 |  9407 | `				VmPopOperand(&pTos,1);` |
|        - |  9408 | `				/* Jump to this destination */` |
|       62 |  9409 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9410 | `				rc = PH7_OK;` |
|       32 |  9411 | `			}else{` |
|       39 |  9412 | `				if( pFrame->pParent ){` |
|       39 |  9413 | `					rc = PH7_EXCEPTION;` |
|       20 |  9414 | `				}else{` |
|        - |  9415 | `					/* Continue normal execution */` |
|      ! 0 |  9416 | `					rc = PH7_OK;` |
|        - |  9417 | `				}` |
|        - |  9418 | `			}` |
|       49 |  9419 | `		}` |
|        - |  9420 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18424 |  9421 | `		if( pFrameStack ){` |
|    18392 |  9422 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9195 |  9423 | `		}` |
|        - |  9424 | `		/* Leave the frame */` |
|    18424 |  9425 | `		VmLeaveFrame(&(*pVm));` |
|    18424 |  9426 | `		if( rc == PH7_ABORT ){` |
|        - |  9427 | `			/* Abort processing immeditaley */` |
|       17 |  9428 | `			goto Abort;` |
|    18408 |  9429 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9430 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9431 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9432 | `			 * overwriting the state saved by the inner level.` |
|        - |  9433 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9434 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9435 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9436 | `			goto Suspend;` |
|    18370 |  9437 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9438 | `			goto Exception;` |
|        - |  9439 | `		}` |
|     9167 |  9440 | `	}else{` |
|        - |  9441 | `		ph7_user_func *pFunc;` |
|        - |  9442 | `		ph7_context sCtx;` |
|        - |  9443 | `		ph7_value sRet;` |
|        - |  9444 | `		/* Look for an installed foreign function.` |
|        - |  9445 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9446 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9447 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9448 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   695248 |  9449 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9450 | `		{` |
|   695248 |  9451 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   695248 |  9452 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9453 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9454 | `			const char *zShort = sName.zString;` |
|        - |  9455 | `			sxu32 i;` |
|      334 |  9456 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9457 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9458 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9459 | `				}` |
|      158 |  9460 | `			}` |
|       22 |  9461 | `			if( zShort != sName.zString ){` |
|       22 |  9462 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9463 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9464 | `			}` |
|       10 |  9465 | `		}` |
|        - |  9466 | `		} /* end VmCallArgMap namespace scope */` |
|   695248 |  9467 | `		if( pEntry == 0 ){` |
|        - |  9468 | `			/* Call to undefined function */` |
|        5 |  9469 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9470 | `			/* Pop given arguments */` |
|        5 |  9471 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9472 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9473 | `			}` |
|        - |  9474 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9475 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9476 | `			break;` |
|        - |  9477 | `		}` |
|   695244 |  9478 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9479 | `		/* Start collecting function arguments */` |
|   695244 |  9480 | `		SySetReset(&aArg);` |
|  1874328 |  9481 | `		while( pArg < pTos ){` |
|  1179086 |  9482 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1179086 |  9483 | `			pArg++;` |
|        2 |  9484 | `		}` |
|        - |  9485 | `		/* Assume a null return value */` |
|   695244 |  9486 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9487 | `		/* Init the call context */` |
|   695244 |  9488 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9489 | `		/* Call the foreign function */` |
|   695244 |  9490 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9491 | `		/* Release the call context */` |
|   695244 |  9492 | `		VmReleaseCallContext(&sCtx);` |
|   695244 |  9493 | `		if( rc == PH7_ABORT ){` |
|      497 |  9494 | `			goto Abort;` |
|   694748 |  9495 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9496 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9497 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9498 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9499 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9500 | `				goto Exception;` |
|        - |  9501 | `			}` |
|        - |  9502 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9503 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9504 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9505 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9506 | `			}` |
|        - |  9507 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9508 | `			VmPopOperand(&pTos,1);` |
|        - |  9509 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9510 | `			pFrm = pVm->pFrame;` |
|      108 |  9511 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9512 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9513 | `			}` |
|      108 |  9514 | `			break;` |
|        - |  9515 | `		}` |
|   694638 |  9516 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9517 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9518 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9519 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9520 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9521 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9522 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9523 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9524 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9525 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9526 | `			}` |
|        - |  9527 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9528 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9529 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9530 | `			goto Suspend;` |
|        - |  9531 | `		}` |
|   694600 |  9532 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9533 | `			/* Pop function name and arguments */` |
|   672676 |  9534 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   336359 |  9535 | `		}` |
|        - |  9536 | `		/* Save foreign function return value */` |
|   694600 |  9537 | `		PH7_MemObjStore(&sRet,pTos);` |
|   694600 |  9538 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9539 | `	}` |
|   712930 |  9540 | `	break;` |
|        - |  9541 | `				  }` |
|        - |  9542 | `/*` |
|        - |  9543 | ` * OP_CONSUME: P1 * *` |
|        - |  9544 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9545 | ` */` |
|    15786 |  9546 | `case PH7_OP_CONSUME: {` |
|    31574 |  9547 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    31574 |  9548 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9549 |  |
|    31574 |  9550 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    31574 |  9551 | `	pCur = pOut;` |
|        - |  9552 | `	/* Start the consume process  */` |
|    63188 |  9553 | `	while( pOut <= pTos ){` |
|        - |  9554 | `		/* Force a string cast */` |
|    31616 |  9555 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1050 |  9556 | `			PH7_MemObjToString(pOut);` |
|      524 |  9557 | `		}` |
|    31616 |  9558 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9559 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9560 | `			/* Invoke the output consumer callback */` |
|    19232 |  9561 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19232 |  9562 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19232 |  9563 | `			SyBlobRelease(&pOut->sBlob);` |
|    19232 |  9564 | `			if( rc == SXERR_ABORT ){` |
|        - |  9565 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9566 | `				goto Abort;` |
|        - |  9567 | `			}` |
|     9615 |  9568 | `		}` |
|    31616 |  9569 | `		pOut++;` |
|        2 |  9570 | `	}` |
|    31574 |  9571 | `	pTos = &pCur[-1];` |
|    31572 |  9572 | `	break;` |
|        - |  9573 | `					 }` |
|        - |  9574 |  |
|        - |  9575 | `		} /* Switch() */` |
| 11746814 |  9576 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9577 | `	} /* For(;;) */` |
|    22036 |  9578 | `Done:` |
|    44074 |  9579 | `	SySetRelease(&aArg);` |
|    44074 |  9580 | `	return SXRET_OK;` |
|       72 |  9581 | `Suspend:` |
|      146 |  9582 | `	SySetRelease(&aArg);` |
|      146 |  9583 | `	return PH7_SUSPEND;` |
|      280 |  9584 | `Abort:` |
|      561 |  9585 | `	SySetRelease(&aArg);` |
|     1875 |  9586 | `	while( pTos >= pStack ){` |
|     1315 |  9587 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9588 | `		pTos--;` |
|        1 |  9589 | `	}` |
|      561 |  9590 | `	return PH7_ABORT;` |
|       29 |  9591 | `Exception:` |
|       60 |  9592 | `	SySetRelease(&aArg);` |
|      112 |  9593 | `	while( pTos >= pStack ){` |
|       54 |  9594 | `		PH7_MemObjRelease(pTos);` |
|       54 |  9595 | `		pTos--;` |
|        2 |  9596 | `	}` |
|       60 |  9597 | `	return PH7_EXCEPTION;` |
|    22419 |  9598 |  |
|        - |  9599 | `/*` |
|        - |  9600 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9601 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9602 | ` * See block-comment on that function for additional information.` |
|        - |  9603 | ` */` |
|    20428 |  9604 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9605 |  |
|        - |  9606 | `	ph7_value *pStack;` |
|        - |  9607 | `	sxi32 rc;` |
|        - |  9608 | `	/* Allocate a new operand stack */` |
|    20430 |  9609 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20430 |  9610 | `	if( pStack == 0 ){` |
|      ! 0 |  9611 | `		return SXERR_MEM;` |
|        - |  9612 | `	}` |
|        - |  9613 | `	/* Execute the program */` |
|    20430 |  9614 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9615 | `	/* Free the operand stack */` |
|    20430 |  9616 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9617 | `	/* Execution result */` |
|    20430 |  9618 | `	return rc;` |
|    10216 |  9619 |  |
|        - |  9620 | `/*` |
|        - |  9621 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9622 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9623 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9624 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9625 | ` * execution ends.` |
|        - |  9626 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9627 | ` * additional information.` |
|        - |  9628 | ` */` |
|     2820 |  9629 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9630 |  |
|        - |  9631 | `	VmShutdownCB *pEntry;` |
|        - |  9632 | `	ph7_value *apArg[10];` |
|        - |  9633 | `	sxu32 n,nEntry;` |
|        - |  9634 | `	int i;` |
|        - |  9635 | `	/* Point to the stack of registered callbacks */` |
|     2822 |  9636 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31022 |  9637 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28202 |  9638 | `		apArg[i] = 0;` |
|    14102 |  9639 | `	}` |
|        - |  9640 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - |  9641 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - |  9642 | `	 * callbacks, mirroring PHP.` |
|        - |  9643 | `	 */` |
|     2822 |  9644 | `	pVm->bHaltRequested = 0;` |
|     2832 |  9645 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 |  9646 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9647 | `		if( pEntry ){` |
|        - |  9648 | `			/* Prepare callback arguments if any */` |
|       12 |  9649 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9650 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9651 | `					break;` |
|        - |  9652 | `				}` |
|      ! 0 |  9653 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9654 | `			}` |
|        - |  9655 | `			/* Invoke the callback */` |
|       12 |  9656 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9657 | `			/*` |
|        - |  9658 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9659 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9660 | `			 */` |
|       12 |  9661 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9662 | `			if( pEntry ){` |
|       12 |  9663 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 |  9664 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9665 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9666 | `				}` |
|        5 |  9667 | `			}` |
|       12 |  9668 | `			if( pVm->bHaltRequested ){` |
|        - |  9669 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 |  9670 | `				break;` |
|        - |  9671 | `			}` |
|        5 |  9672 | `		}` |
|        7 |  9673 | `	}` |
|     2822 |  9674 | `	SySetReset(&pVm->aShutdown);` |
|     2822 |  9675 |  |
|        - |  9676 | `/*` |
|        - |  9677 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9678 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9679 | ` * See block-comment on that function for additional information.` |
|        - |  9680 | ` */` |
|     2820 |  9681 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9682 |  |
|        - |  9683 | `	/* Make sure we are ready to execute this program */` |
|     2822 |  9684 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9685 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9686 | `	}` |
|        - |  9687 | `	/* Set the execution magic number  */` |
|     2822 |  9688 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9689 | `	/* Execute the program */` |
|     2822 |  9690 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9691 | `	/* Invoke any shutdown callbacks */` |
|     2822 |  9692 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9693 | `	/*` |
|        - |  9694 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9695 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9696 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9697 | `	 */` |
|     2822 |  9698 | `	return SXRET_OK;` |
|     1412 |  9699 |  |
|        - |  9700 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9701 | `/*` |
|        - |  9702 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9703 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9704 | ` */` |
|       46 |  9705 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9706 |  |
|        - |  9707 | `	ph7_exec_ctx *pCtx;` |
|        - |  9708 | `	ph7_value *pStack;` |
|        - |  9709 | `	VmFrame *pFrame;` |
|       48 |  9710 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9711 | `	if( pCtx == 0 ){` |
|      ! 0 |  9712 | `		return 0;` |
|        - |  9713 | `	}` |
|       48 |  9714 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9715 | `	pCtx->pVm = pVm;` |
|       48 |  9716 | `	pCtx->pFunc = pFunc;` |
|       48 |  9717 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9718 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9719 | `	pCtx->pc = 0;` |
|       48 |  9720 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9721 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9722 | `	/* Allocate a private operand stack */` |
|       48 |  9723 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9724 | `	if( pStack == 0 ){` |
|      ! 0 |  9725 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9726 | `		return 0;` |
|        - |  9727 | `	}` |
|       48 |  9728 | `	pCtx->pStack = pStack;` |
|        - |  9729 | `	/* Create a detached frame for the fiber */` |
|       48 |  9730 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9731 | `	if( pFrame == 0 ){` |
|      ! 0 |  9732 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9733 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9734 | `		return 0;` |
|        - |  9735 | `	}` |
|       48 |  9736 | `	pCtx->pFrame = pFrame;` |
|       48 |  9737 | `	return pCtx;` |
|       25 |  9738 |  |
|        - |  9739 | `/*` |
|        - |  9740 | ` * Start executing a fiber context for the first time.` |
|        - |  9741 | ` */` |
|       46 |  9742 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9743 |  |
|        - |  9744 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9745 | `	sxi32 rc;` |
|       48 |  9746 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9747 | `		return SXERR_INVALID;` |
|        - |  9748 | `	}` |
|        - |  9749 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9750 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9751 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9752 | `	/* Save and set the active context */` |
|       48 |  9753 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9754 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9755 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9756 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9757 | `	pVm->nRecursionDepth++;` |
|        - |  9758 | `	/* Execute from the beginning */` |
|       48 |  9759 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9760 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9761 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9762 | `	pVm->nRecursionDepth--;` |
|        - |  9763 | `	/* Restore the previous context */` |
|       48 |  9764 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9765 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9766 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9767 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9768 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9769 | `		if( pResult ){` |
|       24 |  9770 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9771 | `		}` |
|       46 |  9772 | `		return SXRET_OK;` |
|        - |  9773 | `	}` |
|        - |  9774 | `	/* Detach frame */` |
|        3 |  9775 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9776 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9777 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9778 | `	}` |
|        3 |  9779 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9780 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9781 | `		return PH7_ABORT;` |
|        - |  9782 | `	}` |
|        3 |  9783 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9784 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9785 | `		return PH7_EXCEPTION;` |
|        - |  9786 | `	}` |
|        - |  9787 | `	/* Normal completion */` |
|        3 |  9788 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9789 | `	if( pResult ){` |
|        3 |  9790 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9791 | `	}` |
|        3 |  9792 | `	return SXRET_OK;` |
|       25 |  9793 |  |
|        - |  9794 | `/*` |
|        - |  9795 | ` * Resume a suspended fiber context.` |
|        - |  9796 | ` */` |
|       98 |  9797 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9798 |  |
|        - |  9799 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9800 | `	sxi32 rc;` |
|      100 |  9801 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9802 | `		return SXERR_INVALID;` |
|        - |  9803 | `	}` |
|        - |  9804 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9805 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9806 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9807 | `	if( pResumeValue ){` |
|       40 |  9808 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9809 | `	}else{` |
|       62 |  9810 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9811 | `	}` |
|      100 |  9812 | `	pCtx->nTos++;` |
|        - |  9813 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9814 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9815 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9816 | `	/* Save and set the active context */` |
|      100 |  9817 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9818 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9819 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9820 | `	pVm->nRecursionDepth++;` |
|        - |  9821 | `	/* Resume execution from saved PC */` |
|      100 |  9822 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9823 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9824 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9825 | `	pVm->nRecursionDepth--;` |
|        - |  9826 | `	/* Restore the previous context */` |
|      100 |  9827 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9828 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9829 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9830 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9831 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9832 | `		if( pResult ){` |
|       18 |  9833 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9834 | `		}` |
|       64 |  9835 | `		return SXRET_OK;` |
|        - |  9836 | `	}` |
|        - |  9837 | `	/* Detach frame */` |
|       38 |  9838 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9839 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9840 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9841 | `	}` |
|       38 |  9842 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9843 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9844 | `		return PH7_ABORT;` |
|        - |  9845 | `	}` |
|       38 |  9846 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9847 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9848 | `		return PH7_EXCEPTION;` |
|        - |  9849 | `	}` |
|        - |  9850 | `	/* Normal completion */` |
|       38 |  9851 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9852 | `	if( pResult ){` |
|       20 |  9853 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9854 | `	}` |
|       38 |  9855 | `	return SXRET_OK;` |
|       51 |  9856 |  |
|        - |  9857 | `/*` |
|        - |  9858 | ` * Release an execution context and all its resources.` |
|        - |  9859 | ` */` |
|        4 |  9860 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9861 |  |
|        5 |  9862 | `	if( pCtx == 0 ){` |
|      ! 0 |  9863 | `		return;` |
|        - |  9864 | `	}` |
|        5 |  9865 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9866 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9867 | `		return;` |
|        - |  9868 | `	}` |
|        5 |  9869 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9870 | `	/* Release values */` |
|        5 |  9871 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9872 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9873 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9874 | `	if( pCtx->pFrame ){` |
|        - |  9875 | `		VmSlot *aSlot;` |
|        - |  9876 | `		sxu32 n;` |
|        - |  9877 | `		/* Free local variables */` |
|        5 |  9878 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9879 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9880 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9881 | `		}` |
|        - |  9882 | `		/* Remove local references */` |
|        5 |  9883 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9884 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9885 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9886 | `		}` |
|        5 |  9887 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9888 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9889 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9890 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9891 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9892 | `		pCtx->pFrame = 0;` |
|        2 |  9893 | `	}` |
|        - |  9894 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9895 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9896 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9897 | `	if( pCtx->pStack ){` |
|        5 |  9898 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9899 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9900 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9901 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9902 | `				pTos--;` |
|        1 |  9903 | `			}` |
|        2 |  9904 | `		}` |
|        5 |  9905 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9906 | `		pCtx->pStack = 0;` |
|        2 |  9907 | `	}` |
|        - |  9908 | `	/* Free the context itself */` |
|        5 |  9909 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9910 |  |
|        - |  9911 | `/*` |
|        - |  9912 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9913 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9914 | ` */` |
|       90 |  9915 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9916 |  |
|        - |  9917 | `	ph7_class_instance *pThis;` |
|        - |  9918 | `	SyString sAttr;` |
|        - |  9919 | `	ph7_value *pAttr;` |
|       92 |  9920 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9921 | `		return 0;` |
|        - |  9922 | `	}` |
|       92 |  9923 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9924 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9925 | `		return 0;` |
|        - |  9926 | `	}` |
|       92 |  9927 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9928 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9929 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9930 | `		return 0;` |
|        - |  9931 | `	}` |
|       62 |  9932 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9933 |  |
|        - |  9934 | `/*` |
|        - |  9935 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9936 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9937 | ` */` |
|       38 |  9938 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9939 |  |
|       40 |  9940 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9941 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9942 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9943 | `			"Cannot suspend outside of a fiber");` |
|        - |  9944 | `	}` |
|       40 |  9945 | `	if( nArg > 0 ){` |
|       40 |  9946 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9947 | `	}else{` |
|      ! 0 |  9948 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9949 | `	}` |
|       40 |  9950 | `	return PH7_SUSPEND;` |
|       21 |  9951 |  |
|        - |  9952 | `/*` |
|        - |  9953 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9954 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9955 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9956 | ` */` |
|       24 |  9957 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9958 |  |
|        - |  9959 | `	ph7_class_instance *pThis;` |
|        - |  9960 | `	ph7_value *pAttr;` |
|        - |  9961 | `	SyString sAttrName;` |
|       26 |  9962 | `	if( nArg < 2 ){` |
|      ! 0 |  9963 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9964 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9965 | `	}` |
|       26 |  9966 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9967 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9968 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9969 | `	}` |
|       26 |  9970 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9971 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9972 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9973 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9974 | `	}` |
|        - |  9975 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9976 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9977 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9978 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9979 | `	}` |
|        - |  9980 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9981 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9982 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9983 | `	if( pAttr ){` |
|       26 |  9984 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9985 | `	}` |
|       26 |  9986 | `	return PH7_OK;` |
|       14 |  9987 |  |
|        - |  9988 | `/*` |
|        - |  9989 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9990 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9991 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9992 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9993 | ` */` |
|       24 |  9994 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9995 | `	ph7_class_instance **ppThis)` |
|        2 |  9996 |  |
|       26 |  9997 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9998 | `	ph7_value *pCallable;` |
|        - |  9999 | `	SyString sAttrName;` |
|       26 | 10000 | `	*ppThis = 0;` |
|       26 | 10001 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10002 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10003 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10004 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10005 | `		return 0;` |
|        - | 10006 | `	}` |
|       26 | 10007 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10008 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10009 | `		SyString sName;` |
|        - | 10010 | `		SyHashEntry *pEntry;` |
|        - | 10011 | `		ph7_vm_func *pFunc;` |
|       26 | 10012 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10013 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10014 | `		if( pEntry == 0 ){` |
|      ! 0 | 10015 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10016 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10017 | `			return 0;` |
|        - | 10018 | `		}` |
|       26 | 10019 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10020 | `		return pFunc;` |
|      ! 0 | 10021 | `	}else{` |
|        - | 10022 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10023 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10024 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10025 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10026 | `		if( pMethod == 0 ){` |
|      ! 0 | 10027 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10028 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10029 | `			return 0;` |
|        - | 10030 | `		}` |
|      ! 0 | 10031 | `		*ppThis = pClosure;` |
|      ! 0 | 10032 | `		return &pMethod->sFunc;` |
|        - | 10033 | `	}` |
|       14 | 10034 |  |
|        - | 10035 | `/*` |
|        - | 10036 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10037 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10038 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10039 | ` */` |
|       46 | 10040 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10041 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10042 |  |
|       48 | 10043 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10044 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10045 | `	sxu32 nFormal, n;` |
|        - | 10046 | `	VmSlot sSlot;` |
|        - | 10047 | `	sxi32 rc;` |
|        - | 10048 | `	/* Install $this for closure/method callables */` |
|       48 | 10049 | `	if( pClosureThis ){` |
|        - | 10050 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10051 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10052 | `		if( pObj ){` |
|      ! 0 | 10053 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10054 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10055 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10056 | `		}` |
|      ! 0 | 10057 | `	}` |
|        - | 10058 | `	/* Install static variables */` |
|       48 | 10059 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10060 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10061 | `		ph7_value *pVal;` |
|      ! 0 | 10062 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10063 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10064 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10065 | `			if( pVal ){` |
|      ! 0 | 10066 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10067 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10068 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10069 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10070 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10071 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10072 | `				}` |
|      ! 0 | 10073 | `			}` |
|      ! 0 | 10074 | `		}` |
|      ! 0 | 10075 | `	}` |
|        - | 10076 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10077 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10078 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10079 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10080 | `		ph7_value *pObj;` |
|       20 | 10081 | `		if( n < (sxu32)nArg ){` |
|        - | 10082 | `			/* Argument provided — install with type casting */` |
|       20 | 10083 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10084 | `			if( pObj ){` |
|       20 | 10085 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10086 | `				/* Type casting */` |
|       20 | 10087 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10088 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10089 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10090 | `						if( xCast ){` |
|      ! 0 | 10091 | `							xCast(pObj);` |
|      ! 0 | 10092 | `						}` |
|      ! 0 | 10093 | `					}` |
|      ! 0 | 10094 | `				}` |
|       20 | 10095 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10096 | `				sSlot.pUserData = 0;` |
|       20 | 10097 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10098 | `			}` |
|        9 | 10099 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10100 | `			/* Default value */` |
|      ! 0 | 10101 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10102 | `			if( pObj ){` |
|      ! 0 | 10103 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10104 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10105 | `					return rc;` |
|        - | 10106 | `				}` |
|      ! 0 | 10107 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10108 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10109 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10110 | `						if( xCast ){` |
|      ! 0 | 10111 | `							xCast(pObj);` |
|      ! 0 | 10112 | `						}` |
|      ! 0 | 10113 | `					}` |
|      ! 0 | 10114 | `				}` |
|      ! 0 | 10115 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10116 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10117 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10118 | `			}` |
|      ! 0 | 10119 | `		}` |
|       11 | 10120 | `	}` |
|        - | 10121 | `	/* Install closure environment (captured variables) */` |
|       48 | 10122 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10123 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10124 | `		ph7_value *pValue;` |
|        - | 10125 | `		sxu32 iEnv;` |
|        3 | 10126 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10127 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10128 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10129 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10130 | `				continue;` |
|        - | 10131 | `			}` |
|        5 | 10132 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10133 | `			if( pValue == 0 ){` |
|      ! 0 | 10134 | `				continue;` |
|        - | 10135 | `			}` |
|        5 | 10136 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10137 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10138 | `		}` |
|        1 | 10139 | `	}` |
|       48 | 10140 | `	return SXRET_OK;` |
|       25 | 10141 |  |
|        - | 10142 | `/*` |
|        - | 10143 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10144 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10145 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10146 | ` */` |
|       26 | 10147 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10148 |  |
|       28 | 10149 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10150 | `	ph7_class_instance *pThis;` |
|        - | 10151 | `	ph7_class_instance *pClosureThis;` |
|        - | 10152 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10153 | `	ph7_vm_func *pFunc;` |
|        - | 10154 | `	ph7_value sResult;` |
|        - | 10155 | `	ph7_value *pCtxAttr;` |
|        - | 10156 | `	SyString sAttrName;` |
|        - | 10157 | `	sxi32 rc;` |
|       28 | 10158 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10159 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10160 | `	}` |
|       28 | 10161 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10162 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10163 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10164 | `	if( pExecCtx != 0 ){` |
|        3 | 10165 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10166 | `			"Cannot start a fiber that has already been started");` |
|        - | 10167 | `	}` |
|        - | 10168 | `	/* Resolve callable */` |
|       26 | 10169 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10170 | `	if( pFunc == 0 ){` |
|      ! 0 | 10171 | `		return PH7_EXCEPTION;` |
|        - | 10172 | `	}` |
|        - | 10173 | `	/* Create execution context now that we know the function */` |
|       26 | 10174 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10175 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10176 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10177 | `			"Fiber::start(): out of memory");` |
|        - | 10178 | `	}` |
|        - | 10179 | `	/* Store context in $this->__ctx */` |
|       26 | 10180 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10181 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10182 | `	if( pCtxAttr ){` |
|       26 | 10183 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10184 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10185 | `	}` |
|        - | 10186 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10187 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10188 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10189 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10190 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10191 | `	/* Unpack the args array and install into the frame */` |
|        - | 10192 | `	{` |
|       26 | 10193 | `		ph7_value **apValues = 0;` |
|       26 | 10194 | `		int nActual = 0;` |
|       26 | 10195 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10196 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10197 | `			ph7_hashmap_node *pNode;` |
|       26 | 10198 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10199 | `			if( nCount > 0 ){` |
|        3 | 10200 | `				sxu32 idx = 0;` |
|        4 | 10201 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10202 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10203 | `				if( apValues ){` |
|        3 | 10204 | `					pNode = pMap->pFirst;` |
|        7 | 10205 | `					while( pNode && idx < nCount ){` |
|        5 | 10206 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10207 | `						idx++;` |
|        5 | 10208 | `						pNode = pNode->pPrev;` |
|        1 | 10209 | `					}` |
|        3 | 10210 | `					nActual = (int)idx;` |
|        1 | 10211 | `				}` |
|        1 | 10212 | `			}` |
|       12 | 10213 | `		}` |
|       26 | 10214 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10215 | `		if( apValues ){` |
|        3 | 10216 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10217 | `		}` |
|        - | 10218 | `	}` |
|        - | 10219 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10220 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10221 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10222 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10223 | `		return PH7_ABORT;` |
|        - | 10224 | `	}` |
|       26 | 10225 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10226 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10227 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10228 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10229 | `		return PH7_ABORT;` |
|        - | 10230 | `	}` |
|       26 | 10231 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10232 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10233 | `		return PH7_EXCEPTION;` |
|        - | 10234 | `	}` |
|       26 | 10235 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10236 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10237 | `	return PH7_OK;` |
|       15 | 10238 |  |
|        - | 10239 | `/*` |
|        - | 10240 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10241 | ` */` |
|       36 | 10242 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10243 |  |
|       38 | 10244 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10245 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10246 | `	ph7_value sResult;` |
|        - | 10247 | `	ph7_value *pResumeVal;` |
|        - | 10248 | `	sxi32 rc;` |
|       38 | 10249 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10250 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10251 | `		return PH7_OK;` |
|        - | 10252 | `	}` |
|       38 | 10253 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10254 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10255 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10256 | `		return PH7_OK;` |
|        - | 10257 | `	}` |
|       38 | 10258 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10259 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10260 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10261 | `	}` |
|       36 | 10262 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10263 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10264 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10265 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10266 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10267 | `		return PH7_ABORT;` |
|        - | 10268 | `	}` |
|       36 | 10269 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10270 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10271 | `		return PH7_EXCEPTION;` |
|        - | 10272 | `	}` |
|       36 | 10273 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10274 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10275 | `	return PH7_OK;` |
|       20 | 10276 |  |
|        - | 10277 | `/*` |
|        - | 10278 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10279 | ` */` |
|        6 | 10280 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10281 |  |
|        8 | 10282 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10283 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10284 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10285 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10286 | `		return PH7_OK;` |
|        - | 10287 | `	}` |
|        8 | 10288 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10289 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10290 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10291 | `		return PH7_OK;` |
|        - | 10292 | `	}` |
|        8 | 10293 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10294 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10295 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10296 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10297 | `		}` |
|      ! 0 | 10298 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10299 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10300 | `	}` |
|        8 | 10301 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10302 | `	return PH7_OK;` |
|        5 | 10303 |  |
|        - | 10304 | `/*` |
|        - | 10305 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10306 | ` */` |
|        6 | 10307 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10308 |  |
|        - | 10309 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10310 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10311 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10312 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10313 | `	return PH7_OK;` |
|        4 | 10314 |  |
|      ! 0 | 10315 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10316 |  |
|        - | 10317 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10318 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10319 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10320 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10321 | `	return PH7_OK;` |
|      ! 0 | 10322 |  |
|        6 | 10323 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10324 |  |
|        - | 10325 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10326 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10327 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10328 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10329 | `	return PH7_OK;` |
|        4 | 10330 |  |
|        6 | 10331 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10332 |  |
|        - | 10333 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10334 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10335 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10336 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10337 | `	return PH7_OK;` |
|        4 | 10338 |  |
|        - | 10339 | `/*` |
|        - | 10340 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10341 | ` */` |
|        4 | 10342 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10343 |  |
|        5 | 10344 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10345 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10346 | `	if( nArg < 1 ){` |
|      ! 0 | 10347 | `		return PH7_OK;` |
|        - | 10348 | `	}` |
|        5 | 10349 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10350 | `	if( pExecCtx ){` |
|        5 | 10351 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10352 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10353 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10354 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10355 | `			SyString sAttrName;` |
|        - | 10356 | `			ph7_value *pAttr;` |
|        5 | 10357 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10358 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10359 | `			if( pAttr ){` |
|        5 | 10360 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10361 | `			}` |
|        2 | 10362 | `		}` |
|        2 | 10363 | `	}` |
|        5 | 10364 | `	return PH7_OK;` |
|        3 | 10365 |  |
|        - | 10366 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10367 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10368 |  |
|        - | 10369 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10370 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10371 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10372 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10373 |  |
|      ! 0 | 10374 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10375 |  |
|        - | 10376 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10377 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10378 | `	ph7_exec_ctx *pCtx;` |
|        - | 10379 | `	ph7_vm_func *pFunc;` |
|        - | 10380 | `	ph7_value *pCallable;` |
|        - | 10381 | `	ph7_value *pCtxAttr;` |
|        - | 10382 | `	SyString sAttrName;` |
|        - | 10383 | `	/* Must not already be started */` |
|      ! 0 | 10384 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10385 | `	if( pCtx != 0 ){` |
|      ! 0 | 10386 | `		return SXERR_INVALID;` |
|        - | 10387 | `	}` |
|      ! 0 | 10388 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10389 | `		return SXERR_INVALID;` |
|        - | 10390 | `	}` |
|      ! 0 | 10391 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10392 | `	/* Get the callable */` |
|      ! 0 | 10393 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10394 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10395 | `	if( pCallable == 0 ){` |
|      ! 0 | 10396 | `		return SXERR_INVALID;` |
|        - | 10397 | `	}` |
|        - | 10398 | `	/* Resolve callable */` |
|      ! 0 | 10399 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10400 | `		SyString sName;` |
|        - | 10401 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10402 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10403 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10404 | `		if( pEntry == 0 ){` |
|      ! 0 | 10405 | `			return SXERR_NOTFOUND;` |
|        - | 10406 | `		}` |
|      ! 0 | 10407 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10408 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10409 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10410 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10411 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10412 | `		if( pMethod == 0 ){` |
|      ! 0 | 10413 | `			return SXERR_INVALID;` |
|        - | 10414 | `		}` |
|      ! 0 | 10415 | `		pClosureThis = pClosure;` |
|      ! 0 | 10416 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10417 | `	}else{` |
|      ! 0 | 10418 | `		return SXERR_INVALID;` |
|        - | 10419 | `	}` |
|        - | 10420 | `	/* Create context */` |
|      ! 0 | 10421 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10422 | `	if( pCtx == 0 ){` |
|      ! 0 | 10423 | `		return SXERR_MEM;` |
|        - | 10424 | `	}` |
|        - | 10425 | `	/* Store in __ctx */` |
|      ! 0 | 10426 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10427 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10428 | `	if( pCtxAttr ){` |
|      ! 0 | 10429 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10430 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10431 | `	}` |
|        - | 10432 | `	/* Set up frame with args */` |
|      ! 0 | 10433 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10434 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10435 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10436 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10437 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10438 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10439 |  |
|      ! 0 | 10440 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10441 |  |
|      ! 0 | 10442 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10443 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10444 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10445 |  |
|      ! 0 | 10446 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10447 |  |
|      ! 0 | 10448 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10449 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10450 |  |
|      ! 0 | 10451 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10452 |  |
|      ! 0 | 10453 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10454 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10455 |  |
|      ! 0 | 10456 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10457 |  |
|      ! 0 | 10458 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10459 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10460 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10461 |  |
|        - | 10462 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10463 | `/*` |
|        - | 10464 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10465 | ` */` |
|       22 | 10466 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10467 |  |
|        - | 10468 | `	ph7_generator *pGen;` |
|       24 | 10469 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10470 | `	if( pGen == 0 ){` |
|      ! 0 | 10471 | `		return 0;` |
|        - | 10472 | `	}` |
|       24 | 10473 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10474 | `	pGen->pCtx = pCtx;` |
|       24 | 10475 | `	pGen->iImplicitKey = 0;` |
|       24 | 10476 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10477 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10478 | `	/* Link the generator back to the exec context */` |
|       24 | 10479 | `	pCtx->pPrivate = pGen;` |
|       24 | 10480 | `	return pGen;` |
|       13 | 10481 |  |
|        - | 10482 | `/*` |
|        - | 10483 | ` * Release a generator and its execution context.` |
|        - | 10484 | ` */` |
|      ! 0 | 10485 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10486 |  |
|      ! 0 | 10487 | `	if( pGen == 0 ){` |
|      ! 0 | 10488 | `		return;` |
|        - | 10489 | `	}` |
|      ! 0 | 10490 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10491 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10492 | `	if( pGen->pCtx ){` |
|      ! 0 | 10493 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10494 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10495 | `		pGen->pCtx = 0;` |
|      ! 0 | 10496 | `	}` |
|      ! 0 | 10497 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10498 |  |
|        - | 10499 | `/*` |
|        - | 10500 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10501 | ` */` |
|      236 | 10502 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10503 |  |
|        - | 10504 | `	ph7_class_instance *pThis;` |
|        - | 10505 | `	SyString sAttr;` |
|        - | 10506 | `	ph7_value *pAttr;` |
|      238 | 10507 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10508 | `		return 0;` |
|        - | 10509 | `	}` |
|      238 | 10510 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10511 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10512 | `		return 0;` |
|        - | 10513 | `	}` |
|      238 | 10514 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10515 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10516 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10517 | `		return 0;` |
|        - | 10518 | `	}` |
|      238 | 10519 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10520 |  |
|        - | 10521 | `/*` |
|        - | 10522 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10523 | ` */` |
|       22 | 10524 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10525 |  |
|        - | 10526 | `	ph7_generator *pGen;` |
|        - | 10527 | `	sxi32 rc;` |
|       24 | 10528 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10529 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10530 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10531 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10532 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10533 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10534 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10535 | `	}` |
|       24 | 10536 | `	return PH7_OK;` |
|       13 | 10537 |  |
|        - | 10538 | `/*` |
|        - | 10539 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10540 | ` */` |
|       68 | 10541 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10542 |  |
|        - | 10543 | `	ph7_generator *pGen;` |
|       70 | 10544 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10545 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10546 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10547 | `	return PH7_OK;` |
|       36 | 10548 |  |
|        - | 10549 | `/*` |
|        - | 10550 | ` * Generator::current() — return the last yielded value.` |
|        - | 10551 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10552 | ` */` |
|       68 | 10553 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10554 |  |
|        - | 10555 | `	ph7_generator *pGen;` |
|        - | 10556 | `	sxi32 rc;` |
|       70 | 10557 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10558 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10559 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10560 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10561 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10562 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10563 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10564 | `	}` |
|       70 | 10565 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10566 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10567 | `	}else{` |
|      ! 0 | 10568 | `		ph7_result_null(pCtx);` |
|        - | 10569 | `	}` |
|       70 | 10570 | `	return PH7_OK;` |
|       36 | 10571 |  |
|        - | 10572 | `/*` |
|        - | 10573 | ` * Generator::key() — return the last yielded key.` |
|        - | 10574 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10575 | ` */` |
|       12 | 10576 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10577 |  |
|        - | 10578 | `	ph7_generator *pGen;` |
|        - | 10579 | `	sxi32 rc;` |
|       13 | 10580 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10581 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10582 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10583 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10584 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10585 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10586 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10587 | `	}` |
|       13 | 10588 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10589 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10590 | `	}else{` |
|      ! 0 | 10591 | `		ph7_result_null(pCtx);` |
|        - | 10592 | `	}` |
|       13 | 10593 | `	return PH7_OK;` |
|        7 | 10594 |  |
|        - | 10595 | `/*` |
|        - | 10596 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10597 | ` */` |
|       60 | 10598 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10599 |  |
|        - | 10600 | `	ph7_generator *pGen;` |
|        - | 10601 | `	sxi32 rc;` |
|       62 | 10602 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10603 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10604 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10605 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10606 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10607 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10608 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10609 | `	}else{` |
|      ! 0 | 10610 | `		return PH7_OK;` |
|        - | 10611 | `	}` |
|       62 | 10612 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10613 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10614 | `	return PH7_OK;` |
|       32 | 10615 |  |
|        - | 10616 | `/*` |
|        - | 10617 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10618 | ` */` |
|        4 | 10619 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10620 |  |
|        - | 10621 | `	ph7_generator *pGen;` |
|        - | 10622 | `	ph7_value *pSendVal;` |
|        - | 10623 | `	sxi32 rc;` |
|        5 | 10624 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10625 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10626 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10627 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10628 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10629 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10630 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10631 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10632 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10633 | `	}else{` |
|      ! 0 | 10634 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10635 | `		return PH7_OK;` |
|        - | 10636 | `	}` |
|        5 | 10637 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10638 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10639 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10640 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10641 | `	}else{` |
|        3 | 10642 | `		ph7_result_null(pCtx);` |
|        - | 10643 | `	}` |
|        5 | 10644 | `	return PH7_OK;` |
|        3 | 10645 |  |
|        - | 10646 | `/*` |
|        - | 10647 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10648 | ` *` |
|        - | 10649 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10650 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10651 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10652 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10653 | ` * the exception to the caller.` |
|        - | 10654 | ` */` |
|      ! 0 | 10655 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10656 |  |
|        - | 10657 | `	ph7_generator *pGen;` |
|        - | 10658 | `	const char *zMsg;` |
|        - | 10659 | `	int nLen;` |
|      ! 0 | 10660 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10661 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10662 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10663 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10664 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10665 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10666 | `			"Cannot throw into a closed generator");` |
|        - | 10667 | `	}` |
|        - | 10668 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10669 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10670 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10671 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10672 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10673 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10674 | `	nLen = 0;` |
|      ! 0 | 10675 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10676 | `		/* Try to get the exception's message */` |
|        - | 10677 | `		SyString sAttr;` |
|        - | 10678 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10679 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10680 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10681 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10682 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10683 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10684 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10685 | `		}` |
|      ! 0 | 10686 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10687 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10688 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10689 | `	}` |
|      ! 0 | 10690 | `	(void)nLen;` |
|      ! 0 | 10691 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10692 |  |
|        - | 10693 | `/*` |
|        - | 10694 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10695 | ` */` |
|        2 | 10696 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10697 |  |
|        - | 10698 | `	ph7_generator *pGen;` |
|        3 | 10699 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10700 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10701 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10702 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10703 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10704 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10705 | `	}` |
|        3 | 10706 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10707 | `	return PH7_OK;` |
|        2 | 10708 |  |
|        - | 10709 | `/*` |
|        - | 10710 | ` * Generator::__destruct() — clean up.` |
|        - | 10711 | ` */` |
|      ! 0 | 10712 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10713 |  |
|        - | 10714 | `	ph7_generator *pGen;` |
|      ! 0 | 10715 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10716 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10717 | `	if( pGen ){` |
|      ! 0 | 10718 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10719 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10720 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10721 | `			SyString sAttrName;` |
|        - | 10722 | `			ph7_value *pAttr;` |
|      ! 0 | 10723 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10724 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10725 | `			if( pAttr ){` |
|      ! 0 | 10726 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10727 | `			}` |
|      ! 0 | 10728 | `		}` |
|      ! 0 | 10729 | `	}` |
|      ! 0 | 10730 | `	return PH7_OK;` |
|      ! 0 | 10731 |  |
|        - | 10732 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10733 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10734 | `/*` |
|        - | 10735 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10736 | ` * the desired message.` |
|        - | 10737 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10738 | ` * in 'api.c' for additional information.` |
|        - | 10739 | ` */` |
|      370 | 10740 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10741 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10742 | `	SyString *pString /* Message to output */` |
|        - | 10743 | `	)` |
|        2 | 10744 |  |
|      372 | 10745 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10746 | `	sxi32 rc = SXRET_OK;` |
|        - | 10747 | `	/* Call the output consumer */` |
|      372 | 10748 | `	if( pString->nByte > 0 ){` |
|      372 | 10749 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10750 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10751 | `	}` |
|      372 | 10752 | `	return rc;` |
|        2 | 10753 |  |
|        - | 10754 | `/*` |
|        - | 10755 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10756 | ` * callback to consume the formatted message.` |
|        - | 10757 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10758 | ` * in 'api.c' for additional information.` |
|        - | 10759 | ` */` |
|        2 | 10760 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10761 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10762 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10763 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10764 | `	)` |
|        1 | 10765 |  |
|        3 | 10766 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10767 | `	sxi32 rc = SXRET_OK;` |
|        - | 10768 | `	SyBlob sWorker;` |
|        - | 10769 | `	/* Format the message and call the output consumer */` |
|        3 | 10770 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10771 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10772 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10773 | `		/* Consume the formatted message */` |
|        3 | 10774 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10775 | `	}` |
|        3 | 10776 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10777 | `	/* Release the working buffer */` |
|        3 | 10778 | `	SyBlobRelease(&sWorker);` |
|        3 | 10779 | `	return rc;` |
|        1 | 10780 |  |
|        - | 10781 | `/*` |
|        - | 10782 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10783 | ` * This function never fail and always return a pointer` |
|        - | 10784 | ` * to a null terminated string.` |
|        - | 10785 | ` */` |
|       12 | 10786 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10787 |  |
|       13 | 10788 | `	const char *zOp = "Unknown     ";` |
|       13 | 10789 | `	switch(nOp){` |
|        3 | 10790 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10791 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10792 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10793 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10794 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10795 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10796 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10797 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10798 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10799 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10800 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10801 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10802 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10803 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10804 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10805 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10806 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10807 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10808 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10809 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10810 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10811 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10812 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10813 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10814 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10815 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10816 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10817 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10818 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10819 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10820 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10821 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10822 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10823 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10824 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10825 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10826 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10827 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10828 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10829 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10830 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10831 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10832 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10833 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10834 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10835 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10836 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10837 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10838 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10839 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10840 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10841 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10842 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10843 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10844 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10845 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10846 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10847 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10848 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10849 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10850 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10851 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10852 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10853 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10854 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10855 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10856 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10857 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10858 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10859 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10860 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10861 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10862 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10863 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10864 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10865 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10866 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10867 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10868 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10869 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10870 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10871 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10872 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10873 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10874 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10875 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10876 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10877 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10878 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10879 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10880 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10881 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10882 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10883 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10884 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10885 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10886 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10887 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10888 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10889 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10890 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10891 | `	default:` |
|      ! 0 | 10892 | `		break;` |
|        - | 10893 | `	}` |
|       13 | 10894 | `	return zOp;` |
|        1 | 10895 |  |
|        - | 10896 | `/*` |
|        - | 10897 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10898 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10899 | ` * is responsible of consuming the generated dump.` |
|        - | 10900 | ` */` |
|        2 | 10901 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10902 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10903 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10904 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10905 | `	)` |
|        1 | 10906 |  |
|        - | 10907 | `	sxi32 rc;` |
|        3 | 10908 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10909 | `	return rc;` |
|        1 | 10910 |  |
|        - | 10911 | `/*` |
|        - | 10912 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10913 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10914 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10915 | ` * in 'compile.c' for additional information.` |
|        - | 10916 | ` */` |
|       14 | 10917 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10918 |  |
|       15 | 10919 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10920 | `	/* Evaluate and expand constant value */` |
|       15 | 10921 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10922 |  |
|        - | 10923 | `/*` |
|        - | 10924 | ` * Section:` |
|        - | 10925 | ` *  Function handling functions.` |
|        - | 10926 | ` * Status:` |
|        - | 10927 | ` *    Stable.` |
|        - | 10928 | ` */` |
|        - | 10929 | `/*` |
|        - | 10930 | ` * int func_num_args(void)` |
|        - | 10931 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10932 | ` * Parameters` |
|        - | 10933 | ` *   None.` |
|        - | 10934 | ` * Return` |
|        - | 10935 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10936 | ` *  or -1 if called from the globe scope.` |
|        - | 10937 | ` */` |
|      980 | 10938 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10939 |  |
|        - | 10940 | `	VmFrame *pFrame;` |
|        - | 10941 | `	ph7_vm *pVm;` |
|        - | 10942 | `	/* Point to the target VM */` |
|      982 | 10943 | `	pVm = pCtx->pVm;` |
|        - | 10944 | `	/* Current frame */` |
|      982 | 10945 | `	pFrame = pVm->pFrame;` |
|      982 | 10946 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 10947 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10948 | `		SXUNUSED(nArg);` |
|      ! 0 | 10949 | `		SXUNUSED(apArg);` |
|        - | 10950 | `		/* Global frame,return -1 */` |
|      ! 0 | 10951 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10952 | `		return SXRET_OK;` |
|        - | 10953 | `	}` |
|        - | 10954 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 10955 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 10956 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 10957 | `	return SXRET_OK;` |
|      492 | 10958 |  |
|        - | 10959 | `/*` |
|        - | 10960 | ` * value func_get_arg(int $arg_num)` |
|        - | 10961 | ` *   Return an item from the argument list.` |
|        - | 10962 | ` * Parameters` |
|        - | 10963 | ` *  Argument number(index start from zero).` |
|        - | 10964 | ` * Return` |
|        - | 10965 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10966 | ` */` |
|       22 | 10967 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10968 |  |
|       24 | 10969 | `	ph7_value *pObj = 0;` |
|       24 | 10970 | `	VmSlot *pSlot = 0;` |
|        - | 10971 | `	VmFrame *pFrame;` |
|        - | 10972 | `	ph7_vm *pVm;` |
|        - | 10973 | `	/* Point to the target VM */` |
|       24 | 10974 | `	pVm = pCtx->pVm;` |
|        - | 10975 | `	/* Current frame */` |
|       24 | 10976 | `	pFrame = pVm->pFrame;` |
|       24 | 10977 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10978 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10979 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10980 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10981 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10982 | `		return SXRET_OK;` |
|        - | 10983 | `	}` |
|        - | 10984 | `	/* Extract the desired index */` |
|       21 | 10985 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10986 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10987 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10988 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10989 | `		return SXRET_OK;` |
|        - | 10990 | `	}` |
|        - | 10991 | `	/* Extract the desired argument */` |
|       21 | 10992 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10993 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10994 | `			/* Return the desired argument */` |
|       21 | 10995 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10996 | `		}else{` |
|        - | 10997 | `			/* No such argument,return false */` |
|      ! 0 | 10998 | `			ph7_result_bool(pCtx,0);` |
|        - | 10999 | `		}` |
|       11 | 11000 | `	}else{` |
|        - | 11001 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11002 | `		ph7_result_bool(pCtx,0);` |
|        - | 11003 | `	}` |
|       21 | 11004 | `	return SXRET_OK;` |
|       13 | 11005 |  |
|        - | 11006 | `/*` |
|        - | 11007 | ` * array func_get_args_byref(void)` |
|        - | 11008 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11009 | ` * Parameters` |
|        - | 11010 | ` *  None.` |
|        - | 11011 | ` * Return` |
|        - | 11012 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11013 | ` *  member of the current user-defined function's argument list.` |
|        - | 11014 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11015 | ` * NOTE:` |
|        - | 11016 | ` *  Arguments are returned to the array by reference.` |
|        - | 11017 | ` */` |
|        2 | 11018 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11019 |  |
|        - | 11020 | `	ph7_value *pArray;` |
|        - | 11021 | `	VmFrame *pFrame;` |
|        - | 11022 | `	VmSlot *aSlot;` |
|        - | 11023 | `	sxu32 n;` |
|        - | 11024 | `	/* Point to the current frame */` |
|        3 | 11025 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11026 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11027 | `	if( pFrame->pParent == 0 ){` |
|        - | 11028 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11029 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11030 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11031 | `		return SXRET_OK;` |
|        - | 11032 | `	}` |
|        - | 11033 | `	/* Create a new array */` |
|        3 | 11034 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11035 | `	if( pArray == 0 ){` |
|      ! 0 | 11036 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11037 | `		SXUNUSED(apArg);` |
|      ! 0 | 11038 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11039 | `		return SXRET_OK;` |
|        - | 11040 | `	}` |
|        - | 11041 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11042 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11043 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11044 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11045 | `	}` |
|        - | 11046 | `	/* Return the freshly created array */` |
|        3 | 11047 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11048 | `	return SXRET_OK;` |
|        2 | 11049 |  |
|        - | 11050 | `/*` |
|        - | 11051 | ` * array func_get_args(void)` |
|        - | 11052 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11053 | ` * Parameters` |
|        - | 11054 | ` *  None.` |
|        - | 11055 | ` * Return` |
|        - | 11056 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11057 | ` *  member of the current user-defined function's argument list.` |
|        - | 11058 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11059 | ` */` |
|       88 | 11060 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11061 |  |
|       90 | 11062 | `	ph7_value *pObj = 0;` |
|        - | 11063 | `	ph7_value *pArray;` |
|        - | 11064 | `	VmFrame *pFrame;` |
|        - | 11065 | `	VmSlot *aSlot;` |
|        - | 11066 | `	sxu32 n;` |
|        - | 11067 | `	/* Point to the current frame */` |
|       90 | 11068 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11069 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11070 | `	if( pFrame->pParent == 0 ){` |
|        - | 11071 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11072 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11073 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11074 | `		return SXRET_OK;` |
|        - | 11075 | `	}` |
|        - | 11076 | `	/* Create a new array */` |
|       90 | 11077 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11078 | `	if( pArray == 0 ){` |
|      ! 0 | 11079 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11080 | `		SXUNUSED(apArg);` |
|      ! 0 | 11081 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11082 | `		return SXRET_OK;` |
|        - | 11083 | `	}` |
|        - | 11084 | `	/* Start filling the array with the given arguments */` |
|       90 | 11085 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11086 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11087 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11088 | `		if( pObj ){` |
|      134 | 11089 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11090 | `		}` |
|       68 | 11091 | `	}` |
|        - | 11092 | `	/* Return the freshly created array */` |
|       90 | 11093 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11094 | `	return SXRET_OK;` |
|       46 | 11095 |  |
|        - | 11096 | `/*` |
|        - | 11097 | ` * bool function_exists(string $name)` |
|        - | 11098 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11099 | ` * Parameters` |
|        - | 11100 | ` *  The name of the desired function.` |
|        - | 11101 | ` * Return` |
|        - | 11102 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11103 | ` */` |
|     1742 | 11104 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11105 |  |
|        - | 11106 | `	const char *zName;` |
|        - | 11107 | `	ph7_vm *pVm;` |
|        - | 11108 | `	int nLen;` |
|        - | 11109 | `	int res;` |
|     1744 | 11110 | `	if( nArg < 1 ){` |
|        - | 11111 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11112 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11113 | `		return SXRET_OK;` |
|        - | 11114 | `	}` |
|        - | 11115 | `	/* Point to the target VM */` |
|     1744 | 11116 | `	pVm = pCtx->pVm;` |
|        - | 11117 | `	/* Extract the function name */` |
|     1744 | 11118 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11119 | `	/* Assume the function is not defined */` |
|     1744 | 11120 | `	res = 0;` |
|        - | 11121 | `	/* Perform the lookup */` |
|     2613 | 11122 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1738 | 11123 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11124 | `			/* Function is defined */` |
|      266 | 11125 | `			res = 1;` |
|      132 | 11126 | `	}` |
|     1744 | 11127 | `	ph7_result_bool(pCtx,res);` |
|     1744 | 11128 | `	return SXRET_OK;` |
|      873 | 11129 |  |
|        - | 11130 | `/*` |
|        - | 11131 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11132 | ` * [i.e: Whether it is callable or not].` |
|        - | 11133 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11134 | ` */` |
|    23476 | 11135 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11136 |  |
|    23478 | 11137 | `	int res = 0;` |
|    23478 | 11138 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11139 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11140 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11141 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11142 | `		 * standard PHP behavior. */` |
|       20 | 11143 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11144 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11145 | `			res = 1;` |
|       10 | 11146 | `		}` |
|        9 | 11147 | `		(void)CallInvoke;` |
|    23469 | 11148 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11149 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11150 | `		if( pMap->nEntry == 2 ){` |
|        - | 11151 | `			ph7_class *pClass;` |
|        - | 11152 | `			ph7_value *pV;` |
|        - | 11153 | `			/* Extract the target class */` |
|       12 | 11154 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11155 | `			if( pV ){` |
|       12 | 11156 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11157 | `				if( pClass ){` |
|        - | 11158 | `					ph7_class_method *pMethod;` |
|        - | 11159 | `					/* Extract the target method */` |
|       10 | 11160 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11161 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11162 | `						/* Perform the lookup */` |
|       10 | 11163 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11164 | `						if( pMethod ){` |
|        - | 11165 | `							/* Method is callable */` |
|        5 | 11166 | `							res = 1;` |
|        2 | 11167 | `						}` |
|        4 | 11168 | `					}` |
|        4 | 11169 | `				}` |
|        5 | 11170 | `			}` |
|        7 | 11171 | `		}` |
|    23447 | 11172 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11173 | `		const char *zName;` |
|        - | 11174 | `		int nLen;` |
|        - | 11175 | `		/* Extract the name */` |
|     5862 | 11176 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11177 | `		/* Perform the lookup */` |
|     5877 | 11178 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11179 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11180 | `				/* Function is callable */` |
|     5844 | 11181 | `				res = 1;` |
|     2921 | 11182 | `		}` |
|     2930 | 11183 | `	}` |
|    23478 | 11184 | `	return res;` |
|        2 | 11185 |  |
|        - | 11186 | `/*` |
|        - | 11187 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11188 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11189 | ` * Parameters` |
|        - | 11190 | ` * $name` |
|        - | 11191 | ` *    The callback function to check` |
|        - | 11192 | ` * $syntax_only` |
|        - | 11193 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11194 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11195 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11196 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11197 | ` *    a string.` |
|        - | 11198 | ` * Return` |
|        - | 11199 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11200 | ` */` |
|       20 | 11201 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11202 |  |
|        - | 11203 | `	ph7_vm *pVm;` |
|        - | 11204 | `	int res;` |
|       21 | 11205 | `	if( nArg < 1 ){` |
|        - | 11206 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11207 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11208 | `		return SXRET_OK;` |
|        - | 11209 | `	}` |
|        - | 11210 | `	/* Point to the target VM */` |
|       21 | 11211 | `	pVm = pCtx->pVm;` |
|        - | 11212 | `	/* Perform the requested operation */` |
|       21 | 11213 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11214 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11215 | `	return SXRET_OK;` |
|       11 | 11216 |  |
|        - | 11217 | `/*` |
|        - | 11218 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11219 | ` * defined below.` |
|        - | 11220 | ` */` |
|     1306 | 11221 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11222 |  |
|     1307 | 11223 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11224 | `	ph7_value sName;` |
|        - | 11225 | `	sxi32 rc;` |
|        - | 11226 | `	/* Prepare the function name for insertion */` |
|     1307 | 11227 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11228 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11229 | `	/* Perform the insertion */` |
|     1307 | 11230 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11231 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11232 | `	return rc;` |
|        1 | 11233 |  |
|        - | 11234 | `/*` |
|        - | 11235 | ` * array get_defined_functions(void)` |
|        - | 11236 | ` *  Returns an array of all defined functions.` |
|        - | 11237 | ` * Parameter` |
|        - | 11238 | ` *  None.` |
|        - | 11239 | ` * Return` |
|        - | 11240 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11241 | ` *  both built-in (internal) and user-defined.` |
|        - | 11242 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11243 | ` *  defined ones using $arr["user"].` |
|        - | 11244 | ` * Note:` |
|        - | 11245 | ` *  NULL is returned on failure.` |
|        - | 11246 | ` */` |
|        2 | 11247 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11248 |  |
|        - | 11249 | `	ph7_value *pArray,*pEntry;` |
|        - | 11250 | `	/* NOTE:` |
|        - | 11251 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11252 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11253 | `	 */` |
|        3 | 11254 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11255 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11256 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11257 | `		SXUNUSED(apArg);` |
|        - | 11258 | `		/* Return NULL */` |
|      ! 0 | 11259 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11260 | `		return SXRET_OK;` |
|        - | 11261 | `	}` |
|        3 | 11262 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11263 | `	if( pEntry == 0 ){` |
|        - | 11264 | `		/* Return NULL */` |
|      ! 0 | 11265 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11266 | `		return SXRET_OK;` |
|        - | 11267 | `	}` |
|        - | 11268 | `	/* Fill with the appropriate information */` |
|        3 | 11269 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11270 | `	/* Create the 'internal' index */` |
|        3 | 11271 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11272 | `	/* Create the user-func array */` |
|        3 | 11273 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11274 | `	if( pEntry == 0 ){` |
|        - | 11275 | `		/* Return NULL */` |
|      ! 0 | 11276 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11277 | `		return SXRET_OK;` |
|        - | 11278 | `	}` |
|        - | 11279 | `	/* Fill with the appropriate information */` |
|        3 | 11280 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11281 | `	/* Create the 'user' index */` |
|        3 | 11282 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11283 | `	/* Return the multi-dimensional array */` |
|        3 | 11284 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11285 | `	return SXRET_OK;` |
|        2 | 11286 |  |
|        - | 11287 | `/*` |
|        - | 11288 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11289 | ` *  Register a function for execution on shutdown.` |
|        - | 11290 | ` * Note` |
|        - | 11291 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11292 | ` *  be called in the same order as they were registered.` |
|        - | 11293 | ` * Parameters` |
|        - | 11294 | ` *  $callback` |
|        - | 11295 | ` *   The shutdown callback to register.` |
|        - | 11296 | ` * $param` |
|        - | 11297 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11298 | ` * Return` |
|        - | 11299 | ` *  Nothing.` |
|        - | 11300 | ` */` |
|       10 | 11301 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11302 |  |
|        - | 11303 | `	VmShutdownCB sEntry;` |
|        - | 11304 | `	int i,j;` |
|       12 | 11305 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11306 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11307 | `		return PH7_OK;` |
|        - | 11308 | `	}` |
|        - | 11309 | `	/* Zero the Entry */` |
|       12 | 11310 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11311 | `	/* Initialize fields */` |
|       12 | 11312 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11313 | `	/* Save the callback name for later invocation name */` |
|       12 | 11314 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11315 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11316 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11317 | `	}` |
|        - | 11318 | `	/* Copy arguments */` |
|       12 | 11319 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11320 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11321 | `			/* Limit reached */` |
|      ! 0 | 11322 | `			break;` |
|        - | 11323 | `		}` |
|      ! 0 | 11324 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11325 | `	}` |
|       12 | 11326 | `	sEntry.nArg = j;` |
|        - | 11327 | `	/* Install the callback */` |
|       12 | 11328 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11329 | `	return PH7_OK;` |
|        7 | 11330 |  |
|        - | 11331 | `/*` |
|        - | 11332 | ` * Section:` |
|        - | 11333 | ` *  Class handling functions.` |
|        - | 11334 | ` * Status:` |
|        - | 11335 | ` *    Stable.` |
|        - | 11336 | ` */` |
|        - | 11337 | `/*` |
|        - | 11338 | ` * Extract the top active class. NULL is returned` |
|        - | 11339 | ` * if the class stack is empty.` |
|        - | 11340 | ` */` |
|      960 | 11341 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11342 |  |
|      962 | 11343 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11344 | `	ph7_class **apClass;` |
|      962 | 11345 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11346 | `		/* Empty stack,return NULL */` |
|       15 | 11347 | `		return 0;` |
|        - | 11348 | `	}` |
|        - | 11349 | `	/* Peek the last entry */` |
|      948 | 11350 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      948 | 11351 | `	return apClass[pSet->nUsed - 1];` |
|      482 | 11352 |  |
|        - | 11353 | `/*` |
|        - | 11354 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11355 | ` *   Get the class that declared the currently executing method.` |
|        - | 11356 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11357 | ` *` |
|        - | 11358 | ` * Parameters` |
|        - | 11359 | ` *   pVm: Target VM` |
|        - | 11360 | ` *` |
|        - | 11361 | ` * Return` |
|        - | 11362 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11363 | ` *   - Not executing within a class method` |
|        - | 11364 | ` *` |
|        - | 11365 | ` * Note` |
|        - | 11366 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11367 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11368 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11369 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11370 | ` *   declaring class.` |
|        - | 11371 | ` */` |
|       98 | 11372 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11373 |  |
|      100 | 11374 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11375 | `	ph7_vm_func *pVmFunc;` |
|        - | 11376 |  |
|        - | 11377 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11378 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11379 |  |
|        - | 11380 | `	/* Check if we're in a method context */` |
|      100 | 11381 | `	if( pFrame->pParent ){` |
|       96 | 11382 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11383 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11384 | `			/* Return the declaring class */` |
|       96 | 11385 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11386 | `		}` |
|      ! 0 | 11387 | `	}` |
|        - | 11388 |  |
|        5 | 11389 | `	return 0;` |
|       51 | 11390 |  |
|        - | 11391 |  |
|        - | 11392 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11393 | `/*` |
|        - | 11394 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11395 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11396 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11397 | ` * return value indicates failure.` |
|        - | 11398 | ` */` |
|        - | 11399 | `/*` |
|        - | 11400 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11401 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11402 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11403 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11404 | ` */` |
|     2456 | 11405 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11406 | `	ph7_vm *pVm,` |
|        - | 11407 | `	ph7_class_instance *pThis,` |
|        - | 11408 | `	ph7_class_method *pMethod,` |
|        - | 11409 | `	ph7_value *pResult,` |
|        - | 11410 | `	int nArg,` |
|        - | 11411 | `	ph7_value **apArg,` |
|        - | 11412 | `	VmCallArgMap *pMap` |
|        - | 11413 | `	)` |
|        2 | 11414 |  |
|        - | 11415 | `	ph7_value *aStack;` |
|        - | 11416 | `	VmInstr aInstr[2];` |
|        - | 11417 | `	int iCursor;` |
|        - | 11418 | `	int i;` |
|        - | 11419 | `	sxi32 rc;` |
|     2458 | 11420 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2458 | 11421 | `	if( aStack == 0 ){` |
|      ! 0 | 11422 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11423 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11424 | `		return SXERR_MEM;` |
|        - | 11425 | `	}` |
|     3992 | 11426 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1536 | 11427 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1536 | 11428 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      769 | 11429 | `	}` |
|     2458 | 11430 | `	iCursor = nArg + 1;` |
|     2458 | 11431 | `	if( pThis ){` |
|     2452 | 11432 | `		pThis->iRef++;` |
|     2452 | 11433 | `		aStack[i].x.pOther = pThis;` |
|     2452 | 11434 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1225 | 11435 | `	}` |
|     2458 | 11436 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2458 | 11437 | `	i++;` |
|     2458 | 11438 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2458 | 11439 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2458 | 11440 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2458 | 11441 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2458 | 11442 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2458 | 11443 | `	aInstr[0].iP1 = nArg;` |
|     2458 | 11444 | `	aInstr[0].iP2 = 0;` |
|     2458 | 11445 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2458 | 11446 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2458 | 11447 | `	aInstr[1].iP1 = 1;` |
|     2458 | 11448 | `	aInstr[1].iP2 = 0;` |
|     2458 | 11449 | `	aInstr[1].p3  = 0;` |
|     2458 | 11450 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2458 | 11451 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11452 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11453 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2458 | 11454 | `	return rc;` |
|     1230 | 11455 |  |
|     1922 | 11456 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11457 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11458 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11459 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11460 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11461 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11462 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11463 | `	)` |
|        2 | 11464 |  |
|     1924 | 11465 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11466 |  |
|        - | 11467 | `/*` |
|        - | 11468 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11469 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11470 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11471 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11472 | ` *` |
|        - | 11473 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11474 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11475 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11476 | ` *` |
|        - | 11477 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11478 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11479 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11480 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11481 | ` *` |
|        - | 11482 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11483 | ` */` |
|      174 | 11484 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11485 | `	ph7_vm *pVm,` |
|        - | 11486 | `	ph7_class_instance *pThis,` |
|        - | 11487 | `	int nArg,` |
|        - | 11488 | `	ph7_value **apArg,` |
|        - | 11489 | `	ph7_value *pResult,` |
|        - | 11490 | `	VmCallArgMap *pMap` |
|        - | 11491 | `	)` |
|        2 | 11492 |  |
|        - | 11493 | `	ph7_class_method *pMethod;` |
|      176 | 11494 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11495 | `	if( pMethod == 0 ){` |
|       13 | 11496 | `		if( pResult ){` |
|       13 | 11497 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11498 | `		}` |
|       13 | 11499 | `		return SXERR_INVALID;` |
|        - | 11500 | `	}` |
|      164 | 11501 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11502 |  |
|        - | 11503 | `/*` |
|        - | 11504 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11505 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11506 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11507 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11508 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11509 | ` * lookup or 'goto Exception').` |
|        - | 11510 | ` *` |
|        - | 11511 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11512 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11513 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11514 | ` * reported.` |
|        - | 11515 | ` */` |
|       12 | 11516 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11517 |  |
|        - | 11518 | `	ph7_class *pErrorClass;` |
|       13 | 11519 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11520 | `	ph7_class_method *pCons;` |
|        - | 11521 | `	VmFrame *pThrowFrame;` |
|        - | 11522 | `	char zMsg[256];` |
|        - | 11523 | `	int nMsg;` |
|        - | 11524 | `	sxi32 rc;` |
|       25 | 11525 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11526 | `		"Object of type %.*s is not callable",` |
|       12 | 11527 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11528 | `		pThis->pClass->sName.zString);` |
|       13 | 11529 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11530 | `	if( pErrorClass ){` |
|       13 | 11531 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11532 | `	}` |
|       13 | 11533 | `	if( pErrInst == 0 ){` |
|        - | 11534 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11535 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11536 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11537 | `		 * visible to the user. */` |
|      ! 0 | 11538 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11539 | `		return SXERR_ABORT;` |
|        - | 11540 | `	}` |
|       13 | 11541 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11542 | `	if( pCons ){` |
|        - | 11543 | `		ph7_value sArg;` |
|        - | 11544 | `		ph7_value *apMsg[1];` |
|        - | 11545 | `		SyString sMsgStr;` |
|       13 | 11546 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11547 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11548 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11549 | `		apMsg[0] = &sArg;` |
|       13 | 11550 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11551 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11552 | `	}` |
|        - | 11553 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11554 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11555 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11556 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11557 | `	if( pThrowFrame ){` |
|       13 | 11558 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11559 | `	}` |
|       13 | 11560 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11561 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11562 | `	return rc;` |
|        7 | 11563 |  |
|        - | 11564 | `/*` |
|        - | 11565 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11566 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11567 | ` * in the apArg[] array.` |
|        - | 11568 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11569 | ` * return value indicates failure.` |
|        - | 11570 | ` */` |
|     1212 | 11571 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11572 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11573 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11574 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11575 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11576 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11577 | `	)` |
|        2 | 11578 |  |
|        - | 11579 | `	ph7_value *aStack;` |
|        - | 11580 | `	VmInstr aInstr[2];` |
|        - | 11581 | `	int i;` |
|     1214 | 11582 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11583 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11584 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11585 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 11586 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 11587 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 11588 | `			nArg,apArg,pResult,0);` |
|        - | 11589 | `	}` |
|     1122 | 11590 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11591 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11592 | `		if( pResult ){` |
|        - | 11593 | `			/* Assume a null return value */` |
|      ! 0 | 11594 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11595 | `		}` |
|      511 | 11596 | `		return SXERR_INVALID;` |
|        - | 11597 | `	}` |
|      612 | 11598 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11599 | `		/* Class method */` |
|       15 | 11600 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 11601 | `		ph7_class_method *pMethod = 0;` |
|       15 | 11602 | `		ph7_class_instance *pThis = 0;` |
|       15 | 11603 | `		ph7_class *pClass = 0;` |
|        - | 11604 | `		ph7_value *pValue;` |
|        - | 11605 | `		sxi32 rc;` |
|       15 | 11606 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11607 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11608 | `			if( pResult ){` |
|        - | 11609 | `				/* Assume a null return value */` |
|      ! 0 | 11610 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11611 | `			}` |
|      ! 0 | 11612 | `			return SXRET_OK;` |
|        - | 11613 | `		}` |
|        - | 11614 | `		/* Extract the class name or an instance of it */` |
|       15 | 11615 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 11616 | `		if( pValue ){` |
|       15 | 11617 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 11618 | `		}` |
|       15 | 11619 | `		if( pClass == 0 ){` |
|        - | 11620 | `			/* No such class,return NULL */` |
|      ! 0 | 11621 | `			if( pResult ){` |
|      ! 0 | 11622 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11623 | `			}` |
|      ! 0 | 11624 | `			return SXRET_OK;` |
|        - | 11625 | `		}` |
|       15 | 11626 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11627 | `			/* Point to the class instance */` |
|        9 | 11628 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 11629 | `		}` |
|        - | 11630 | `		/* Try to extract the method */` |
|       15 | 11631 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 11632 | `		if( pValue ){` |
|       15 | 11633 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 11634 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 11635 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 11636 | `			}` |
|        7 | 11637 | `		}` |
|       15 | 11638 | `		if( pMethod == 0 ){` |
|        - | 11639 | `			/* No such method,return NULL */` |
|      ! 0 | 11640 | `			if( pResult ){` |
|      ! 0 | 11641 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11642 | `			}` |
|      ! 0 | 11643 | `			return SXRET_OK;` |
|        - | 11644 | `		}` |
|        - | 11645 | `		/* Call the class method */` |
|       15 | 11646 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 11647 | `		return rc;` |
|        - | 11648 | `	}` |
|        - | 11649 | `	/* Create a new operand stack */` |
|      598 | 11650 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 11651 | `	if( aStack == 0 ){` |
|      ! 0 | 11652 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11653 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11654 | `		if( pResult ){` |
|        - | 11655 | `			/* Assume a null return value */` |
|      ! 0 | 11656 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11657 | `		}` |
|      ! 0 | 11658 | `		return SXERR_MEM;` |
|        - | 11659 | `	}` |
|        - | 11660 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 11661 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 11662 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11663 | `		/*` |
|        - | 11664 | `		 * Symisc eXtension:` |
|        - | 11665 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11666 | `		 */` |
|     1304 | 11667 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 11668 | `	}` |
|        - | 11669 | `	/* Push the function name */` |
|      598 | 11670 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 11671 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11672 | `	/* Emit the CALL istruction */` |
|      598 | 11673 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 11674 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 11675 | `	aInstr[0].iP2 = 0;` |
|      598 | 11676 | `	aInstr[0].p3  = 0;` |
|        - | 11677 | `	/* Emit the DONE instruction */` |
|      598 | 11678 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 11679 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 11680 | `	aInstr[1].iP2 = 0;` |
|      598 | 11681 | `	aInstr[1].p3  = 0;` |
|        - | 11682 | `	/* Execute the function body (if available) */` |
|        - | 11683 | `	{` |
|        - | 11684 | `		sxi32 rcExec;` |
|      598 | 11685 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11686 | `		/* Clean up the mess left behind */` |
|      598 | 11687 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11688 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 11689 | `		return rcExec;` |
|        - | 11690 | `	}` |
|      608 | 11691 |  |
|        - | 11692 | `/*` |
|        - | 11693 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11694 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11695 | ` * parameter.` |
|        - | 11696 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11697 | ` * return value indicates failure.` |
|        - | 11698 | ` */` |
|      240 | 11699 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11700 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11701 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11702 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11703 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11704 | `	)` |
|        1 | 11705 |  |
|        - | 11706 | `	ph7_value *pArg;` |
|        - | 11707 | `	SySet aArg;` |
|        - | 11708 | `	va_list ap;` |
|        - | 11709 | `	sxi32 rc;` |
|      241 | 11710 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11711 | `	/* Copy arguments one after one */` |
|      241 | 11712 | `	va_start(ap,pResult);` |
|      399 | 11713 | `	for(;;){` |
|      799 | 11714 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 11715 | `		if( pArg == 0 ){` |
|      241 | 11716 | `			break;` |
|        - | 11717 | `		}` |
|      559 | 11718 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11719 | `	}` |
|        - | 11720 | `	/* Call the core routine */` |
|      241 | 11721 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11722 | `	/* Cleanup */` |
|      241 | 11723 | `	SySetRelease(&aArg);` |
|      241 | 11724 | `	return rc;` |
|        1 | 11725 |  |
|        - | 11726 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11727 | `/*` |
|        - | 11728 | ` * bool defined(string $name)` |
|        - | 11729 | ` *  Checks whether a given named constant exists.` |
|        - | 11730 | ` * Parameter:` |
|        - | 11731 | ` *  Name of the desired constant.` |
|        - | 11732 | ` * Return` |
|        - | 11733 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11734 | ` */` |
|       20 | 11735 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11736 |  |
|        - | 11737 | `	const char *zName;` |
|       22 | 11738 | `	int nLen = 0;` |
|       22 | 11739 | `	int res = 0;` |
|       22 | 11740 | `	if( nArg < 1 ){` |
|        - | 11741 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11742 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11743 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11744 | `		return SXRET_OK;` |
|        - | 11745 | `	}` |
|        - | 11746 | `	/* Extract constant name */` |
|       22 | 11747 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11748 | `	/* Perform the lookup */` |
|       22 | 11749 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11750 | `		/* Already defined */` |
|       20 | 11751 | `		res = 1;` |
|        9 | 11752 | `	}` |
|       22 | 11753 | `	ph7_result_bool(pCtx,res);` |
|       22 | 11754 | `	return SXRET_OK;` |
|       12 | 11755 |  |
|        - | 11756 | `/*` |
|        - | 11757 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11758 | ` * below.` |
|        - | 11759 | ` */` |
|       10 | 11760 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11761 |  |
|       12 | 11762 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11763 | `	/* Expand constant value */` |
|       12 | 11764 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11765 |  |
|        - | 11766 | `/*` |
|        - | 11767 | ` * bool define(string $constant_name,expression value)` |
|        - | 11768 | ` *  Defines a named constant at runtime.` |
|        - | 11769 | ` * Parameter:` |
|        - | 11770 | ` *  $constant_name` |
|        - | 11771 | ` *   The name of the constant` |
|        - | 11772 | ` *  $value` |
|        - | 11773 | ` *   Constant value` |
|        - | 11774 | ` * Return:` |
|        - | 11775 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11776 | ` */` |
|       12 | 11777 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11778 |  |
|        - | 11779 | `	const char *zName;  /* Constant name */` |
|        - | 11780 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11781 | `	int nLen = 0;       /* Name length */` |
|        - | 11782 | `	sxi32 rc;` |
|       14 | 11783 | `	if( nArg < 2 ){` |
|        - | 11784 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11785 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11786 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11787 | `		return SXRET_OK;` |
|        - | 11788 | `	}` |
|       14 | 11789 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11790 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11791 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11792 | `		return SXRET_OK;` |
|        - | 11793 | `	}` |
|        - | 11794 | `	/* Extract constant name */` |
|       14 | 11795 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11796 | `	if( nLen < 1 ){` |
|      ! 0 | 11797 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11798 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11799 | `		return SXRET_OK;` |
|        - | 11800 | `	}` |
|        - | 11801 | `	/* Duplicate constant value */` |
|       14 | 11802 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11803 | `	if( pValue == 0 ){` |
|      ! 0 | 11804 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11805 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11806 | `		return SXRET_OK;` |
|        - | 11807 | `	}` |
|        - | 11808 | `	/* Initialize the memory object */` |
|       14 | 11809 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11810 | `	/* Register the constant */` |
|       14 | 11811 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11812 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11813 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11814 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11815 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11816 | `		return SXRET_OK;` |
|        - | 11817 | `	}` |
|        - | 11818 | `	/* Duplicate constant value */` |
|       14 | 11819 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11820 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11821 | `		/* Lower case the constant name */` |
|      ! 0 | 11822 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11823 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11824 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11825 | `				/* UTF-8 stream */` |
|      ! 0 | 11826 | `				zCur++;` |
|      ! 0 | 11827 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11828 | `					zCur++;` |
|      ! 0 | 11829 | `				}` |
|      ! 0 | 11830 | `				continue;` |
|        - | 11831 | `			}` |
|      ! 0 | 11832 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11833 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11834 | `				zCur[0] = (char)c;` |
|      ! 0 | 11835 | `			}` |
|      ! 0 | 11836 | `			zCur++;` |
|      ! 0 | 11837 | `		}` |
|        - | 11838 | `		/* Finally,register the constant */` |
|      ! 0 | 11839 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11840 | `	}` |
|        - | 11841 | `	/* All done,return TRUE */` |
|       14 | 11842 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11843 | `	return SXRET_OK;` |
|        8 | 11844 |  |
|        - | 11845 | `/*` |
|        - | 11846 | ` * value constant(string $name)` |
|        - | 11847 | ` *  Returns the value of a constant` |
|        - | 11848 | ` * Parameter` |
|        - | 11849 | ` *  $name` |
|        - | 11850 | ` *    Name of the constant.` |
|        - | 11851 | ` * Return` |
|        - | 11852 | ` *  Constant value or NULL if not defined.` |
|        - | 11853 | ` */` |
|        8 | 11854 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11855 |  |
|        - | 11856 | `	SyHashEntry *pEntry;` |
|        - | 11857 | `	ph7_constant *pCons;` |
|        - | 11858 | `	const char *zName; /* Constant name */` |
|        - | 11859 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11860 | `	int nLen;` |
|       10 | 11861 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11862 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11863 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11864 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11865 | `		return SXRET_OK;` |
|        - | 11866 | `	}` |
|        - | 11867 | `	/* Extract the constant name */` |
|       10 | 11868 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11869 | `	/* Perform the query */` |
|       10 | 11870 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11871 | `	if( pEntry == 0 ){` |
|        3 | 11872 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11873 | `		ph7_result_null(pCtx);` |
|        3 | 11874 | `		return SXRET_OK;` |
|        - | 11875 | `	}` |
|        8 | 11876 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11877 | `	/* Point to the structure that describe the constant */` |
|        8 | 11878 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11879 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11880 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11881 | `	/* Return that value */` |
|        8 | 11882 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11883 | `	/* Cleanup */` |
|        8 | 11884 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11885 | `	return SXRET_OK;` |
|        6 | 11886 |  |
|        - | 11887 | `/*` |
|        - | 11888 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11889 | ` * defined below.` |
|        - | 11890 | ` */` |
|      466 | 11891 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11892 |  |
|      467 | 11893 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11894 | `	ph7_value sName;` |
|        - | 11895 | `	sxi32 rc;` |
|        - | 11896 | `	/* Prepare the constant name for insertion */` |
|      467 | 11897 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 11898 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11899 | `	/* Perform the insertion */` |
|      467 | 11900 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 11901 | `	PH7_MemObjRelease(&sName);` |
|      467 | 11902 | `	return rc;` |
|        1 | 11903 |  |
|        - | 11904 | `/*` |
|        - | 11905 | ` * array get_defined_constants(void)` |
|        - | 11906 | ` *  Returns an associative array with the names of all defined` |
|        - | 11907 | ` *  constants.` |
|        - | 11908 | ` * Parameters` |
|        - | 11909 | ` *  NONE.` |
|        - | 11910 | ` * Returns` |
|        - | 11911 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11912 | ` */` |
|        2 | 11913 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11914 |  |
|        - | 11915 | `	ph7_value *pArray;` |
|        - | 11916 | `	/* Create the array first*/` |
|        3 | 11917 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11918 | `	if( pArray == 0 ){` |
|      ! 0 | 11919 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11920 | `		SXUNUSED(apArg);` |
|        - | 11921 | `		/* Return NULL */` |
|      ! 0 | 11922 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11923 | `		return SXRET_OK;` |
|        - | 11924 | `	}` |
|        - | 11925 | `	/* Fill the array with the defined constants */` |
|        3 | 11926 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11927 | `	/* Return the created array */` |
|        3 | 11928 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11929 | `	return SXRET_OK;` |
|        2 | 11930 |  |
|        - | 11931 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11932 | `/*` |
|        - | 11933 | ` * Section:` |
|        - | 11934 | ` *  Random numbers/string generators.` |
|        - | 11935 | ` * Status:` |
|        - | 11936 | ` *    Stable.` |
|        - | 11937 | ` */` |
|        - | 11938 | `/*` |
|        - | 11939 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11940 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 11941 | ` * implemented in src/sx/sxrand.c).` |
|        - | 11942 | ` */` |
|     2891 | 11943 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11944 |  |
|        - | 11945 | `	sxu32 iNum;` |
|     2893 | 11946 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2893 | 11947 | `	return iNum;` |
|        2 | 11948 |  |
|        - | 11949 | `/*` |
|        - | 11950 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11951 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11952 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 11953 | ` * implemented in src/sx/sxrand.c).` |
|        - | 11954 | ` */` |
|   236034 | 11955 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11956 |  |
|        - | 11957 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11958 | `	int i;` |
|        - | 11959 | `	/* Generate a binary string first */` |
|   236036 | 11960 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11961 | `	/* Turn the binary string into english based alphabet */` |
|  2596544 | 11962 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2360510 | 11963 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1180256 | 11964 | `	 }` |
|   236036 | 11965 |  |
|        - | 11966 | `/*` |
|        - | 11967 | ` * int rand()` |
|        - | 11968 | ` * int mt_rand()` |
|        - | 11969 | ` * int rand(int $min,int $max)` |
|        - | 11970 | ` * int mt_rand(int $min,int $max)` |
|        - | 11971 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11972 | ` * Parameter` |
|        - | 11973 | ` *  $min` |
|        - | 11974 | ` *    The lowest value to return (default: 0)` |
|        - | 11975 | ` *  $max` |
|        - | 11976 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11977 | ` * Return` |
|        - | 11978 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11979 | ` * Note:` |
|        - | 11980 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11981 | ` *  by te SQLite3 library.` |
|        - | 11982 | ` */` |
|       20 | 11983 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11984 |  |
|        - | 11985 | `	sxu32 iNum;` |
|        - | 11986 | `	/* Generate the random number */` |
|       21 | 11987 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11988 | `	if( nArg > 1 ){` |
|        - | 11989 | `		sxu32 iMin,iMax;` |
|        3 | 11990 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11991 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11992 | `		if( iMin < iMax ){` |
|        3 | 11993 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11994 | `			if( iDiv > 0 ){` |
|        3 | 11995 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11996 | `			}` |
|        1 | 11997 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11998 | `			iNum %= iMax;` |
|      ! 0 | 11999 | `		}` |
|        1 | 12000 | `	}` |
|        - | 12001 | `	/* Return the number */` |
|       21 | 12002 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12003 | `	return SXRET_OK;` |
|        1 | 12004 |  |
|        - | 12005 | `/*` |
|        - | 12006 | ` * int getrandmax(void)` |
|        - | 12007 | ` * int mt_getrandmax(void)` |
|        - | 12008 | ` * int rc4_getrandmax(void)` |
|        - | 12009 | ` *   Show largest possible random value` |
|        - | 12010 | ` * Return` |
|        - | 12011 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12012 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12013 | ` * Note:` |
|        - | 12014 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12015 | ` *  by te SQLite3 library.` |
|        - | 12016 | ` */` |
|        4 | 12017 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12018 |  |
|        2 | 12019 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12020 | `	SXUNUSED(apArg);` |
|        5 | 12021 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12022 | `	return SXRET_OK;` |
|        1 | 12023 |  |
|        - | 12024 | `/*` |
|        - | 12025 | ` * string rand_str()` |
|        - | 12026 | ` * string rand_str(int $len)` |
|        - | 12027 | ` *  Generate a random string (English alphabet).` |
|        - | 12028 | ` * Parameter` |
|        - | 12029 | ` *  $len` |
|        - | 12030 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12031 | ` * Return` |
|        - | 12032 | ` *   A pseudo random string.` |
|        - | 12033 | ` * Note:` |
|        - | 12034 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12035 | ` *  by te SQLite3 library.` |
|        - | 12036 | ` *  This function is a symisc extension.` |
|        - | 12037 | ` */` |
|      120 | 12038 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12039 |  |
|        - | 12040 | `	char zString[1024];` |
|      122 | 12041 | `	int iLen = 0x10;` |
|      122 | 12042 | `	if( nArg > 0 ){` |
|        - | 12043 | `		/* Get the desired length */` |
|      122 | 12044 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12045 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12046 | `			/* Default length */` |
|        3 | 12047 | `			iLen = 0x10;` |
|        1 | 12048 | `		}` |
|       60 | 12049 | `	}` |
|        - | 12050 | `	/* Generate the random string */` |
|      122 | 12051 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12052 | `	/* Return the generated string */` |
|      122 | 12053 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12054 | `	return SXRET_OK;` |
|        2 | 12055 |  |
|        - | 12056 | `/*` |
|        - | 12057 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12058 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12059 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12060 | ` */` |
|      488 | 12061 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12062 |  |
|      488 | 12063 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12064 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12065 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12066 | `			"TypeError",` |
|        - | 12067 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12068 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12069 | `			ph7_type_name(pArg)` |
|        - | 12070 | `			);` |
|        - | 12071 | `	}` |
|      483 | 12072 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12073 | `		int len;` |
|        9 | 12074 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12075 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12076 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12077 | `				"TypeError",` |
|        - | 12078 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12079 | `				zFunc,iArgPos,zParamName` |
|        - | 12080 | `				);` |
|        - | 12081 | `		}` |
|        2 | 12082 | `	}` |
|      479 | 12083 | `	return SXRET_OK;` |
|      245 | 12084 |  |
|        - | 12085 | `/*` |
|        - | 12086 | ` * int random_int(int $min, int $max)` |
|        - | 12087 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12088 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12089 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12090 | ` *  power-of-two mask covering the range.` |
|        - | 12091 | ` */` |
|      242 | 12092 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12093 |  |
|        - | 12094 | `	sxi64 iMin,iMax;` |
|        - | 12095 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12096 | `	unsigned int nAttempt;` |
|        - | 12097 | `	int rc;` |
|      243 | 12098 | `	if( nArg != 2 ){` |
|       10 | 12099 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12100 | `			"ArgumentCountError",` |
|        - | 12101 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12102 | `			nArg` |
|        - | 12103 | `			);` |
|        - | 12104 | `	}` |
|      237 | 12105 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12106 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12107 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12108 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12109 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12110 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12111 | `	if( iMin > iMax ){` |
|        3 | 12112 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12113 | `			"ValueError",` |
|        - | 12114 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12115 | `			);` |
|        - | 12116 | `	}` |
|      229 | 12117 | `	if( iMin == iMax ){` |
|        5 | 12118 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12119 | `		return SXRET_OK;` |
|        - | 12120 | `	}` |
|      225 | 12121 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12122 | `	uMask = uRange;` |
|      225 | 12123 | `	uMask \|= uMask >> 1;` |
|      225 | 12124 | `	uMask \|= uMask >> 2;` |
|      225 | 12125 | `	uMask \|= uMask >> 4;` |
|      225 | 12126 | `	uMask \|= uMask >> 8;` |
|      225 | 12127 | `	uMask \|= uMask >> 16;` |
|      225 | 12128 | `	uMask \|= uMask >> 32;` |
|      225 | 12129 | `	uResult = 0;` |
|      347 | 12130 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12131 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12132 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12133 | `		 * and the low-half mask would always read 0). */` |
|        - | 12134 | `		sxu64 uDraw;` |
|      347 | 12135 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12136 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12137 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12138 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12139 | `				"Exception",` |
|        - | 12140 | `				"Cannot gather sufficient random data"` |
|        - | 12141 | `				);` |
|        - | 12142 | `		}` |
|      347 | 12143 | `		uDraw &= uMask;` |
|      347 | 12144 | `		if( uDraw <= uRange ){` |
|      225 | 12145 | `			uResult = uDraw;` |
|      225 | 12146 | `			break;` |
|        - | 12147 | `		}` |
|       62 | 12148 | `	}` |
|      225 | 12149 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12150 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12151 | `			"Exception",` |
|        - | 12152 | `			"Cannot gather sufficient random data"` |
|        - | 12153 | `			);` |
|        - | 12154 | `	}` |
|      225 | 12155 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12156 | `	return SXRET_OK;` |
|      122 | 12157 |  |
|        - | 12158 | `/*` |
|        - | 12159 | ` * string random_bytes(int $length)` |
|        - | 12160 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12161 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12162 | ` */` |
|       24 | 12163 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12164 |  |
|        - | 12165 | `	sxi64 iLen;` |
|        - | 12166 | `	unsigned char zStack[256];` |
|        - | 12167 | `	void *pBuf;` |
|        - | 12168 | `	int rc;` |
|       25 | 12169 | `	int bHeap = 0;` |
|       25 | 12170 | `	if( nArg != 1 ){` |
|        7 | 12171 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12172 | `			"ArgumentCountError",` |
|        - | 12173 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12174 | `			nArg` |
|        - | 12175 | `			);` |
|        - | 12176 | `	}` |
|       21 | 12177 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12178 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12179 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12180 | `	if( iLen < 1 ){` |
|        5 | 12181 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12182 | `			"ValueError",` |
|        - | 12183 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12184 | `			);` |
|        - | 12185 | `	}` |
|        - | 12186 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12187 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12188 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12189 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12190 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12191 | `			"ValueError",` |
|        - | 12192 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12193 | `			);` |
|        - | 12194 | `	}` |
|       13 | 12195 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12196 | `		pBuf = zStack;` |
|        7 | 12197 | `	}else{` |
|      ! 0 | 12198 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12199 | `		if( pBuf == 0 ){` |
|      ! 0 | 12200 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12201 | `				"Exception",` |
|        - | 12202 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12203 | `				iLen` |
|        - | 12204 | `				);` |
|        - | 12205 | `		}` |
|      ! 0 | 12206 | `		bHeap = 1;` |
|        - | 12207 | `	}` |
|       13 | 12208 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12209 | `		if( bHeap ){` |
|      ! 0 | 12210 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12211 | `		}` |
|      ! 0 | 12212 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12213 | `			"Exception",` |
|        - | 12214 | `			"Cannot gather sufficient random data"` |
|        - | 12215 | `			);` |
|        - | 12216 | `	}` |
|       13 | 12217 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12218 | `	if( bHeap ){` |
|      ! 0 | 12219 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12220 | `	}` |
|       13 | 12221 | `	return SXRET_OK;` |
|       13 | 12222 |  |
|        - | 12223 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12224 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12225 | `/* Unique ID private data */` |
|        - | 12226 | `struct unique_id_data` |
|        - | 12227 |  |
|        - | 12228 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12229 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12230 | `};` |
|        - | 12231 | `/*` |
|        - | 12232 | ` * Binary to hex consumer callback.` |
|        - | 12233 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12234 | ` * defined below.` |
|        - | 12235 | ` */` |
|      192 | 12236 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12237 |  |
|      193 | 12238 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12239 | `	sxu32 nBuflen;` |
|        - | 12240 | `	/* Extract result buffer length */` |
|      193 | 12241 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12242 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12243 | `			/*` |
|        - | 12244 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12245 | `			 * string will be 13 characters long` |
|        - | 12246 | `			 */` |
|       25 | 12247 | `		return SXERR_ABORT;` |
|        - | 12248 | `	}` |
|      169 | 12249 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12250 | `		return SXERR_ABORT;` |
|        - | 12251 | `	}` |
|        - | 12252 | `	/* Safely Consume the hex stream */` |
|      169 | 12253 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12254 | `	return SXRET_OK;` |
|       97 | 12255 |  |
|        - | 12256 | `/*` |
|        - | 12257 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12258 | ` *  Generate a unique ID` |
|        - | 12259 | ` * Parameter` |
|        - | 12260 | ` * $prefix` |
|        - | 12261 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12262 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12263 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12264 | ` * $more_entropy` |
|        - | 12265 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12266 | ` *  that the result will be unique.` |
|        - | 12267 | ` * Return` |
|        - | 12268 | ` *  Returns the unique identifier, as a string.` |
|        - | 12269 | ` */` |
|       24 | 12270 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12271 |  |
|        - | 12272 | `	struct unique_id_data sUniq;` |
|        - | 12273 | `	unsigned char zDigest[20];` |
|       25 | 12274 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12275 | `	const char *zPrefix;` |
|        - | 12276 | `	SHA1Context sCtx;` |
|        - | 12277 | `	char zRandom[7];` |
|        - | 12278 | `	int nPrefix;` |
|        - | 12279 | `	int entropy;` |
|        - | 12280 | `	/* Generate a random string first */` |
|       25 | 12281 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12282 | `	/* Initialize fields */` |
|       25 | 12283 | `	zPrefix = 0;` |
|       25 | 12284 | `	nPrefix = 0;` |
|       25 | 12285 | `	entropy = 0;` |
|       25 | 12286 | `	if( nArg > 0 ){` |
|        - | 12287 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12288 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12289 | `		if( nArg > 1 ){` |
|      ! 0 | 12290 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12291 | `		}` |
|      ! 0 | 12292 | `	}` |
|       25 | 12293 | `	SHA1Init(&sCtx);` |
|        - | 12294 | `	/* Generate the random ID */` |
|       25 | 12295 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12296 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12297 | `	}` |
|        - | 12298 | `	/* Append the random ID */` |
|       25 | 12299 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12300 | `	/* Append the random string */` |
|       25 | 12301 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12302 | `	/* Increment the number */` |
|       25 | 12303 | `	pVm->unique_id++;` |
|       25 | 12304 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12305 | `	/* Hexify the digest */` |
|       25 | 12306 | `	sUniq.pCtx = pCtx;` |
|       25 | 12307 | `	sUniq.entropy = entropy;` |
|       25 | 12308 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12309 | `	/* All done */` |
|       25 | 12310 | `	return PH7_OK;` |
|        1 | 12311 |  |
|        - | 12312 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12313 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12314 | `/*` |
|        - | 12315 | ` * Section:` |
|        - | 12316 | ` *  Language construct implementation as foreign functions.` |
|        - | 12317 | ` * Status:` |
|        - | 12318 | ` *    Stable.` |
|        - | 12319 | ` */` |
|        - | 12320 | `/*` |
|        - | 12321 | ` * void echo($string...)` |
|        - | 12322 | ` *  Output one or more messages.` |
|        - | 12323 | ` * Parameters` |
|        - | 12324 | ` *  $string` |
|        - | 12325 | ` *   Message to output.` |
|        - | 12326 | ` * Return` |
|        - | 12327 | ` *  NULL.` |
|        - | 12328 | ` */` |
|      ! 0 | 12329 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12330 |  |
|        - | 12331 | `	const char *zData;` |
|      ! 0 | 12332 | `	int nDataLen = 0;` |
|        - | 12333 | `	ph7_vm *pVm;` |
|        - | 12334 | `	int i,rc;` |
|        - | 12335 | `	/* Point to the target VM */` |
|      ! 0 | 12336 | `	pVm = pCtx->pVm;` |
|        - | 12337 | `	/* Output */` |
|      ! 0 | 12338 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12339 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12340 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12341 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12342 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12343 | `			if( rc == SXERR_ABORT ){` |
|        - | 12344 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12345 | `				return PH7_ABORT;` |
|        - | 12346 | `			}` |
|      ! 0 | 12347 | `		}` |
|      ! 0 | 12348 | `	}` |
|      ! 0 | 12349 | `	return SXRET_OK;` |
|      ! 0 | 12350 |  |
|        - | 12351 | `/*` |
|        - | 12352 | ` * int print($string...)` |
|        - | 12353 | ` *  Output one or more messages.` |
|        - | 12354 | ` * Parameters` |
|        - | 12355 | ` *  $string` |
|        - | 12356 | ` *   Message to output.` |
|        - | 12357 | ` * Return` |
|        - | 12358 | ` *  1 always.` |
|        - | 12359 | ` */` |
|        2 | 12360 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12361 |  |
|        - | 12362 | `	const char *zData;` |
|        3 | 12363 | `	int nDataLen = 0;` |
|        - | 12364 | `	ph7_vm *pVm;` |
|        - | 12365 | `	int i,rc;` |
|        - | 12366 | `	/* Point to the target VM */` |
|        3 | 12367 | `	pVm = pCtx->pVm;` |
|        - | 12368 | `	/* Output */` |
|        5 | 12369 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12370 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12371 | `		if( nDataLen > 0 ){` |
|        3 | 12372 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12373 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12374 | `			if( rc == SXERR_ABORT ){` |
|        - | 12375 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12376 | `				return PH7_ABORT;` |
|        - | 12377 | `			}` |
|        1 | 12378 | `		}` |
|        2 | 12379 | `	}` |
|        - | 12380 | `	/* Return 1 */` |
|        3 | 12381 | `	ph7_result_int(pCtx,1);` |
|        3 | 12382 | `	return SXRET_OK;` |
|        2 | 12383 |  |
|        - | 12384 | `/*` |
|        - | 12385 | ` * void exit(string $msg)` |
|        - | 12386 | ` * void exit(int $status)` |
|        - | 12387 | ` * void die(string $ms)` |
|        - | 12388 | ` * void die(int $status)` |
|        - | 12389 | ` *   Output a message and terminate program execution.` |
|        - | 12390 | ` * Parameter` |
|        - | 12391 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12392 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12393 | ` *  and not printed` |
|        - | 12394 | ` * Return` |
|        - | 12395 | ` *  NULL` |
|        - | 12396 | ` */` |
|      ! 0 | 12397 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12398 |  |
|      ! 0 | 12399 | `	if( nArg > 0 ){` |
|      ! 0 | 12400 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12401 | `			const char *zData;` |
|      ! 0 | 12402 | `			int iLen = 0;` |
|        - | 12403 | `			/* Print exit message */` |
|      ! 0 | 12404 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12405 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12406 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12407 | `			sxi32 iExitStatus;` |
|        - | 12408 | `			/* Record exit status code */` |
|      ! 0 | 12409 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12410 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12411 | `		}` |
|      ! 0 | 12412 | `	}` |
|        - | 12413 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12414 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12415 | `	 */` |
|      ! 0 | 12416 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12417 | `	return PH7_ABORT;` |
|      ! 0 | 12418 |  |
|        - | 12419 | `/*` |
|        - | 12420 | ` * bool isset($var,...)` |
|        - | 12421 | ` *  Finds out whether a variable is set.` |
|        - | 12422 | ` * Parameters` |
|        - | 12423 | ` *  One or more variable to check.` |
|        - | 12424 | ` * Return` |
|        - | 12425 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12426 | ` */` |
|    92500 | 12427 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12428 |  |
|        - | 12429 | `	ph7_value *pObj;` |
|    92502 | 12430 | `	int res = 0;` |
|        - | 12431 | `	int i;` |
|    92502 | 12432 | `	if( nArg < 1 ){` |
|        - | 12433 | `		/* Missing arguments,return false */` |
|      ! 0 | 12434 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12435 | `		return SXRET_OK;` |
|        - | 12436 | `	}` |
|        - | 12437 | `	/* Iterate over available arguments */` |
|   120922 | 12438 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92512 | 12439 | `		pObj = apArg[i];` |
|    92512 | 12440 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12441 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12442 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12443 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63160 | 12444 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12445 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12446 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12447 | `			}` |
|    31579 | 12448 | `		}` |
|    92512 | 12449 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92512 | 12450 | `		if( !res ){` |
|        - | 12451 | `			/* Variable not set,return FALSE */` |
|    64092 | 12452 | `			ph7_result_bool(pCtx,0);` |
|    64092 | 12453 | `			return SXRET_OK;` |
|        - | 12454 | `		}` |
|    14212 | 12455 | `	}` |
|        - | 12456 | `	/* All given variable are set,return TRUE */` |
|    28412 | 12457 | `	ph7_result_bool(pCtx,1);` |
|    28412 | 12458 | `	return SXRET_OK;` |
|    46252 | 12459 |  |
|        - | 12460 | `/*` |
|        - | 12461 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12462 | ` * frame,the reference table and discard it's contents.` |
|        - | 12463 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12464 | ` */` |
|  3159712 | 12465 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12466 |  |
|        - | 12467 | `	ph7_value *pObj;` |
|        - | 12468 | `	VmRefObj *pRef;` |
|  3159714 | 12469 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3159714 | 12470 | `	if( pObj ){` |
|        - | 12471 | `		/* Release the object */` |
|  3159714 | 12472 | `		PH7_MemObjRelease(pObj);` |
|  1579856 | 12473 | `	}` |
|        - | 12474 | `	/* Remove old reference links */` |
|  3159714 | 12475 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3159714 | 12476 | `	if( pRef ){` |
|  3159708 | 12477 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12478 | `		/* Unlink from the reference table */` |
|  3159708 | 12479 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3159708 | 12480 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12481 | `			VmSlot sFree;` |
|        - | 12482 | `			/* Restore to the free list */` |
|  3159700 | 12483 | `			sFree.nIdx = nObjIdx;` |
|  3159700 | 12484 | `			sFree.pUserData = 0;` |
|  3159700 | 12485 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1579849 | 12486 | `		}` |
|  1579853 | 12487 | `	}` |
|  3159714 | 12488 | `	return SXRET_OK;` |
|        2 | 12489 |  |
|        - | 12490 | `/*` |
|        - | 12491 | ` * void unset($var,...)` |
|        - | 12492 | ` *   Unset one or more given variable.` |
|        - | 12493 | ` * Parameters` |
|        - | 12494 | ` *  One or more variable to unset.` |
|        - | 12495 | ` * Return` |
|        - | 12496 | ` *  Nothing.` |
|        - | 12497 | ` */` |
|     7500 | 12498 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12499 |  |
|        - | 12500 | `	ph7_value *pObj;` |
|        - | 12501 | `	ph7_vm *pVm;` |
|        - | 12502 | `	int i;` |
|        - | 12503 | `	/* Point to the target VM */` |
|     7502 | 12504 | `	pVm = pCtx->pVm;` |
|        - | 12505 | `	/* Iterate and unset */` |
|    15002 | 12506 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7502 | 12507 | `		pObj = apArg[i];` |
|     7502 | 12508 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      818 | 12509 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12510 | `				/* Throw an error */` |
|      ! 0 | 12511 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12512 | `			}` |
|      410 | 12513 | `		}else{` |
|     6686 | 12514 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12515 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6686 | 12516 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6680 | 12517 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3339 | 12518 | `			}` |
|        - | 12519 | `		}` |
|     3752 | 12520 | `	}` |
|     7502 | 12521 | `	return SXRET_OK;` |
|        2 | 12522 |  |
|        - | 12523 | `/*` |
|        - | 12524 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12525 | ` */` |
|      116 | 12526 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12527 |  |
|      117 | 12528 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12529 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12530 | `	ph7_value *pObj;` |
|        - | 12531 | `	sxu32 nIdx;` |
|        - | 12532 | `	/* Extract the memory object */` |
|      117 | 12533 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12534 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12535 | `	if( pObj ){` |
|      117 | 12536 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12537 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12538 | `				SyString sName;` |
|        - | 12539 | `				ph7_value sKey;` |
|        - | 12540 | `				/* Perform the insertion */` |
|      115 | 12541 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12542 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12543 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12544 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12545 | `			}` |
|       57 | 12546 | `		}` |
|       58 | 12547 | `	}` |
|      117 | 12548 | `	return SXRET_OK;` |
|        1 | 12549 |  |
|        - | 12550 | `/*` |
|        - | 12551 | ` * array get_defined_vars(void)` |
|        - | 12552 | ` *  Returns an array of all defined variables.` |
|        - | 12553 | ` * Parameter` |
|        - | 12554 | ` *  None` |
|        - | 12555 | ` * Return` |
|        - | 12556 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12557 | ` */` |
|        2 | 12558 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12559 |  |
|        3 | 12560 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12561 | `	ph7_value *pArray;` |
|        - | 12562 | `	/* Create a new array */` |
|        3 | 12563 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12564 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12565 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12566 | `		SXUNUSED(apArg);` |
|        - | 12567 | `		/* Return NULL */` |
|      ! 0 | 12568 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12569 | `		return SXRET_OK;` |
|        - | 12570 | `	}` |
|        - | 12571 | `	/* Superglobals first */` |
|        3 | 12572 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12573 | `	/* Then variable defined in the current frame */` |
|        3 | 12574 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12575 | `	/* Finally,return the created array */` |
|        3 | 12576 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12577 | `	return SXRET_OK;` |
|        2 | 12578 |  |
|        - | 12579 | `/*` |
|        - | 12580 | ` * bool gettype($var)` |
|        - | 12581 | ` *  Get the type of a variable` |
|        - | 12582 | ` * Parameters` |
|        - | 12583 | ` *   $var` |
|        - | 12584 | ` *    The variable being type checked.` |
|        - | 12585 | ` * Return` |
|        - | 12586 | ` *   String representation of the given variable type.` |
|        - | 12587 | ` */` |
|       32 | 12588 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12589 |  |
|       34 | 12590 | `	const char *zType = "Empty";` |
|       34 | 12591 | `	if( nArg > 0 ){` |
|       34 | 12592 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12593 | `	}` |
|        - | 12594 | `	/* Return the variable type */` |
|       34 | 12595 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12596 | `	return SXRET_OK;` |
|        2 | 12597 |  |
|        - | 12598 | `/*` |
|        - | 12599 | ` * string get_resource_type(resource $handle)` |
|        - | 12600 | ` *  This function gets the type of the given resource.` |
|        - | 12601 | ` * Parameters` |
|        - | 12602 | ` *  $handle` |
|        - | 12603 | ` *  The evaluated resource handle.` |
|        - | 12604 | ` * Return` |
|        - | 12605 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12606 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12607 | ` *  the return value will be the string Unknown.` |
|        - | 12608 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12609 | ` *  is not a resource.` |
|        - | 12610 | ` */` |
|        2 | 12611 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12612 |  |
|        3 | 12613 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12614 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12615 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12616 | `		return PH7_OK;` |
|        - | 12617 | `	}` |
|        3 | 12618 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12619 | `	return SXRET_OK;` |
|        2 | 12620 |  |
|        - | 12621 | `/*` |
|        - | 12622 | ` * void var_dump(expression,....)` |
|        - | 12623 | ` *   var_dump � Dumps information about a variable` |
|        - | 12624 | ` * Parameters` |
|        - | 12625 | ` *   One or more expression to dump.` |
|        - | 12626 | ` * Returns` |
|        - | 12627 | ` *  Nothing.` |
|        - | 12628 | ` */` |
|      218 | 12629 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12630 |  |
|        - | 12631 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12632 | `	int i;` |
|      220 | 12633 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12634 | `	/* Dump one or more expressions */` |
|      444 | 12635 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12636 | `		ph7_value *pObj = apArg[i];` |
|        - | 12637 | `		/* Reset the working buffer */` |
|      226 | 12638 | `		SyBlobReset(&sDump);` |
|        - | 12639 | `		/* Dump the given expression */` |
|      226 | 12640 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12641 | `		/* Output */` |
|      226 | 12642 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12643 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12644 | `		}` |
|      114 | 12645 | `	}` |
|        - | 12646 | `	/* Release the working buffer */` |
|      220 | 12647 | `	SyBlobRelease(&sDump);` |
|      220 | 12648 | `	return SXRET_OK;` |
|        2 | 12649 |  |
|        - | 12650 | `/*` |
|        - | 12651 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12652 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12653 | ` * Parameters` |
|        - | 12654 | ` *   expression: Expression to dump` |
|        - | 12655 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12656 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12657 | ` *            print_r() will return the information rather than print it.` |
|        - | 12658 | ` * Return` |
|        - | 12659 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12660 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12661 | ` */` |
|       16 | 12662 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12663 |  |
|       17 | 12664 | `	int ret_string = 0;` |
|        - | 12665 | `	SyBlob sDump;` |
|       17 | 12666 | `	if( nArg < 1 ){` |
|        - | 12667 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12668 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12669 | `		return SXRET_OK;` |
|        - | 12670 | `	}` |
|       17 | 12671 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12672 | `	if ( nArg > 1 ){` |
|        - | 12673 | `		/* Where to redirect output */` |
|       11 | 12674 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12675 | `	}` |
|        - | 12676 | `	/* Generate dump */` |
|       17 | 12677 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12678 | `	if( !ret_string ){` |
|        - | 12679 | `		/* Output dump */` |
|        7 | 12680 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12681 | `		/* Return true */` |
|        7 | 12682 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12683 | `	}else{` |
|        - | 12684 | `		/* Generated dump as return value */` |
|       11 | 12685 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12686 | `	}` |
|        - | 12687 | `	/* Release the working buffer */` |
|       17 | 12688 | `	SyBlobRelease(&sDump);` |
|       17 | 12689 | `	return SXRET_OK;` |
|        9 | 12690 |  |
|        - | 12691 | `/*` |
|        - | 12692 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12693 | ` * Same job as print_r. (see coment above)` |
|        - | 12694 | ` */` |
|        2 | 12695 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12696 |  |
|        3 | 12697 | `	int ret_string = 0;` |
|        - | 12698 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12699 | `	if( nArg < 1 ){` |
|        - | 12700 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12701 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12702 | `		return SXRET_OK;` |
|        - | 12703 | `	}` |
|        3 | 12704 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12705 | `	if ( nArg > 1 ){` |
|        - | 12706 | `		/* Where to redirect output */` |
|        3 | 12707 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12708 | `	}` |
|        - | 12709 | `	/* Generate dump */` |
|        3 | 12710 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12711 | `	if( !ret_string ){` |
|        - | 12712 | `		/* Output dump */` |
|      ! 0 | 12713 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12714 | `		/* Return NULL */` |
|      ! 0 | 12715 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12716 | `	}else{` |
|        - | 12717 | `		/* Generated dump as return value */` |
|        3 | 12718 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12719 | `	}` |
|        - | 12720 | `	/* Release the working buffer */` |
|        3 | 12721 | `	SyBlobRelease(&sDump);` |
|        3 | 12722 | `	return SXRET_OK;` |
|        2 | 12723 |  |
|        - | 12724 | `/*` |
|        - | 12725 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12726 | ` *  Set/get the various assert flags.` |
|        - | 12727 | ` * Parameter` |
|        - | 12728 | ` * $what` |
|        - | 12729 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12730 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12731 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12732 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12733 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12734 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12735 | ` * $value` |
|        - | 12736 | ` *   An optional new value for the option.` |
|        - | 12737 | ` * Return` |
|        - | 12738 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12739 | ` */` |
|       28 | 12740 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12741 |  |
|       30 | 12742 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12743 | `	int iOption;` |
|        - | 12744 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12745 | `	if( nArg < 1 ){` |
|        3 | 12746 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12747 | `			"ArgumentCountError",` |
|        - | 12748 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12749 | `			);` |
|        - | 12750 | `	}` |
|        - | 12751 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12752 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12753 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12754 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12755 | `			"TypeError",` |
|        - | 12756 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12757 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12758 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12759 | `			);` |
|        - | 12760 | `	}` |
|       28 | 12761 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12762 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12763 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12764 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12765 | `	switch( iOption ){` |
|        5 | 12766 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12767 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12768 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12769 | `		if( nArg > 1 ){` |
|        5 | 12770 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12771 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12772 | `			}else{` |
|        3 | 12773 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12774 | `			}` |
|        2 | 12775 | `		}` |
|       12 | 12776 | `		break;` |
|        1 | 12777 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12778 | `		/* Return old callback or null */` |
|        3 | 12779 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12780 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12781 | `		}else{` |
|        3 | 12782 | `			ph7_result_null(pCtx);` |
|        - | 12783 | `		}` |
|        3 | 12784 | `		if( nArg > 1 ){` |
|      ! 0 | 12785 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12786 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12787 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12788 | `			}else{` |
|      ! 0 | 12789 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12790 | `			}` |
|      ! 0 | 12791 | `		}` |
|        3 | 12792 | `		break;` |
|        5 | 12793 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12794 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12795 | `		if( nArg > 1 ){` |
|        5 | 12796 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12797 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12798 | `			}else{` |
|        3 | 12799 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12800 | `			}` |
|        2 | 12801 | `		}` |
|       11 | 12802 | `		break;` |
|      ! 0 | 12803 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12804 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12805 | `		break;` |
|        1 | 12806 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12807 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12808 | `		break;` |
|      ! 0 | 12809 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12810 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12811 | `		break;` |
|        1 | 12812 | `	default:` |
|        - | 12813 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12814 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12815 | `			"ValueError",` |
|        - | 12816 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12817 | `			);` |
|        - | 12818 | `	}` |
|       26 | 12819 | `	return PH7_OK;` |
|       16 | 12820 |  |
|        - | 12821 | `/*` |
|        - | 12822 | ` * bool assert(mixed $assertion)` |
|        - | 12823 | ` *  Checks if assertion is FALSE.` |
|        - | 12824 | ` * Parameter` |
|        - | 12825 | ` *  $assertion` |
|        - | 12826 | ` *    The assertion to test.` |
|        - | 12827 | ` * Return` |
|        - | 12828 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12829 | ` */` |
|       24 | 12830 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12831 |  |
|       26 | 12832 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12833 | `	int iFlags,iResult;` |
|        - | 12834 | `	const char *zDesc;` |
|        - | 12835 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12836 | `	if( nArg < 1 ){` |
|        3 | 12837 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12838 | `			"ArgumentCountError",` |
|        - | 12839 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12840 | `			);` |
|        - | 12841 | `	}` |
|       24 | 12842 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12843 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12844 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12845 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12846 | `		return PH7_OK;` |
|        - | 12847 | `	}` |
|        - | 12848 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12849 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12850 | `	if( !iResult ){` |
|        - | 12851 | `		/* Assertion failed */` |
|        - | 12852 | `		/* Extract optional description */` |
|       13 | 12853 | `		zDesc = 0;` |
|       13 | 12854 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12855 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12856 | `		}` |
|       13 | 12857 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12858 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12859 | `			ph7_value sFile,sLine;` |
|        - | 12860 | `			ph7_value *apCbArg[3];` |
|        - | 12861 | `			SyString *pFile;` |
|        - | 12862 | `			/* Extract the processed script */` |
|      ! 0 | 12863 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12864 | `			if( pFile == 0 ){` |
|      ! 0 | 12865 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12866 | `			}` |
|        - | 12867 | `			/* Invoke the callback */` |
|      ! 0 | 12868 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12869 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12870 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12871 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12872 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12873 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12874 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12875 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12876 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12877 | `		}` |
|       13 | 12878 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12879 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12880 | `			return PH7_ABORT;` |
|        - | 12881 | `		}` |
|        - | 12882 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12883 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12884 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12885 | `				"AssertionError",` |
|        - | 12886 | `				"%s",` |
|        1 | 12887 | `				zDesc` |
|        - | 12888 | `				);` |
|      ! 0 | 12889 | `		}else{` |
|       11 | 12890 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12891 | `				"AssertionError",` |
|        - | 12892 | `				"assert(false)"` |
|        - | 12893 | `				);` |
|        - | 12894 | `		}` |
|        - | 12895 | `	}` |
|        - | 12896 | `	/* Assertion passed */` |
|       11 | 12897 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12898 | `	return PH7_OK;` |
|       14 | 12899 |  |
|        - | 12900 | `/*` |
|        - | 12901 | ` * Section:` |
|        - | 12902 | ` *  Error reporting functions.` |
|        - | 12903 | ` * Status:` |
|        - | 12904 | ` *    Stable.` |
|        - | 12905 | ` */` |
|        - | 12906 | `/*` |
|        - | 12907 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12908 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12909 | ` * Parameters` |
|        - | 12910 | ` *  $error_msg` |
|        - | 12911 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12912 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12913 | ` * $error_type` |
|        - | 12914 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12915 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12916 | ` * Return` |
|        - | 12917 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12918 | ` */` |
|       12 | 12919 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12920 |  |
|       14 | 12921 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12922 | `	int rc = PH7_OK;` |
|       14 | 12923 | `	if( nArg > 0 ){` |
|        - | 12924 | `		const char *zErr;` |
|        - | 12925 | `		int nLen;` |
|        - | 12926 | `		/* Extract the error message */` |
|       12 | 12927 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12928 | `		if( nArg > 1 ){` |
|        - | 12929 | `			/* Extract the error type */` |
|       12 | 12930 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12931 | `			switch( nErr ){` |
|        1 | 12932 | `			case 1:   /* E_ERROR */` |
|        - | 12933 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12934 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12935 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12936 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12937 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12938 | `				break;` |
|        1 | 12939 | `			case 2:   /* E_WARNING */` |
|        - | 12940 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12941 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12942 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12943 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12944 | `				break;` |
|        3 | 12945 | `			default:` |
|        8 | 12946 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12947 | `				break;` |
|        - | 12948 | `			}` |
|        5 | 12949 | `		}` |
|        - | 12950 | `		/* Report error */` |
|       12 | 12951 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12952 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12953 | `			return rc;` |
|        - | 12954 | `		}` |
|        - | 12955 | `		/* Return true */` |
|       12 | 12956 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12957 | `	}else{` |
|        - | 12958 | `		/* Missing arguments,return FALSE */` |
|        3 | 12959 | `		ph7_result_bool(pCtx,0);` |
|        - | 12960 | `	}` |
|       14 | 12961 | `	return rc;` |
|        8 | 12962 |  |
|        - | 12963 | `/*` |
|        - | 12964 | ` * int error_reporting([int $level])` |
|        - | 12965 | ` *  Sets which PHP errors are reported.` |
|        - | 12966 | ` * Parameters` |
|        - | 12967 | ` *  $level` |
|        - | 12968 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12969 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12970 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12971 | ` *   levels will not always behave as expected.` |
|        - | 12972 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12973 | ` *   in the predefined constants.` |
|        - | 12974 | ` * Return` |
|        - | 12975 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12976 | ` *   parameter is given.` |
|        - | 12977 | ` */` |
|       32 | 12978 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12979 |  |
|       34 | 12980 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12981 | `	int nOld;` |
|        - | 12982 | `	/* Extract the old reporting level */` |
|       34 | 12983 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 12984 | `	if( nArg > 0 ){` |
|        - | 12985 | `		int nNew;` |
|        - | 12986 | `		/* Extract the desired error reporting level */` |
|       28 | 12987 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 12988 | `		if( !nNew ){` |
|        - | 12989 | `			/* Do not report errors at all */` |
|        5 | 12990 | `			pVm->bErrReport = 0;` |
|        3 | 12991 | `		}else{` |
|        - | 12992 | `			/* Report all errors */` |
|       24 | 12993 | `			pVm->bErrReport = 1;` |
|        - | 12994 | `		}` |
|       13 | 12995 | `	}` |
|        - | 12996 | `	/* Return the old level */` |
|       34 | 12997 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 12998 | `	return PH7_OK;` |
|        2 | 12999 |  |
|        - | 13000 | `/*` |
|        - | 13001 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13002 | ` *  Send an error message somewhere.` |
|        - | 13003 | ` * Parameter` |
|        - | 13004 | ` *  $message` |
|        - | 13005 | ` *   The error message that should be logged.` |
|        - | 13006 | ` *  $message_type` |
|        - | 13007 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13008 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13009 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13010 | ` *       This is the default option.` |
|        - | 13011 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13012 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13013 | ` *    2  No longer an option.` |
|        - | 13014 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13015 | ` *       to the end of the message string.` |
|        - | 13016 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13017 | ` *  $destination` |
|        - | 13018 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13019 | ` *  $extra_headers` |
|        - | 13020 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13021 | ` * Return` |
|        - | 13022 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13023 | ` * NOTE:` |
|        - | 13024 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13025 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13026 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13027 | ` *  Otherwise this function is no-op.` |
|        - | 13028 | ` */` |
|        4 | 13029 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13030 |  |
|        - | 13031 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13032 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13033 | `	int iType = 0;` |
|        5 | 13034 | `	if( nArg < 1 ){` |
|        - | 13035 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13036 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13037 | `		return PH7_OK;` |
|        - | 13038 | `	}` |
|        5 | 13039 | `	if( pVm->xErrLog  ){` |
|        - | 13040 | `		/* Invoke the user callback */` |
|      ! 0 | 13041 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13042 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13043 | `		if( nArg > 1 ){` |
|      ! 0 | 13044 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13045 | `			if( nArg > 2 ){` |
|      ! 0 | 13046 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13047 | `				if( nArg > 3 ){` |
|      ! 0 | 13048 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13049 | `				}` |
|      ! 0 | 13050 | `			}` |
|      ! 0 | 13051 | `		}` |
|      ! 0 | 13052 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13053 | `	}` |
|        - | 13054 | `	/* Retun TRUE */` |
|        5 | 13055 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13056 | `	return PH7_OK;` |
|        3 | 13057 |  |
|        - | 13058 | `/*` |
|        - | 13059 | ` * bool restore_exception_handler(void)` |
|        - | 13060 | ` *  Restores the previously defined exception handler function.` |
|        - | 13061 | ` * Parameter` |
|        - | 13062 | ` *  None` |
|        - | 13063 | ` * Return` |
|        - | 13064 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13065 | ` */` |
|        4 | 13066 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13067 |  |
|        5 | 13068 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13069 | `	ph7_value *pOld,*pNew;` |
|        - | 13070 | `	/* Point to the old and the new handler */` |
|        5 | 13071 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13072 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13073 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13074 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13075 | `		SXUNUSED(apArg);` |
|        - | 13076 | `		/* No installed handler,return FALSE */` |
|        5 | 13077 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13078 | `		return PH7_OK;` |
|        - | 13079 | `	}` |
|        - | 13080 | `	/* Copy the old handler */` |
|      ! 0 | 13081 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13082 | `	PH7_MemObjRelease(pOld);` |
|        - | 13083 | `	/* Return TRUE */` |
|      ! 0 | 13084 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13085 | `	return PH7_OK;` |
|        3 | 13086 |  |
|        - | 13087 | `/*` |
|        - | 13088 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13089 | ` *  Sets a user-defined exception handler function.` |
|        - | 13090 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13091 | ` * NOTE` |
|        - | 13092 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13093 | ` *  the satndard PHP engine.` |
|        - | 13094 | ` * Parameters` |
|        - | 13095 | ` *  $exception_handler` |
|        - | 13096 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13097 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13098 | ` *   that was thrown.` |
|        - | 13099 | ` *  Note:` |
|        - | 13100 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13101 | ` * Return` |
|        - | 13102 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13103 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13104 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13105 | ` */` |
|        4 | 13106 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13107 |  |
|        6 | 13108 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13109 | `	ph7_value *pOld,*pNew;` |
|        - | 13110 | `	/* Point to the old and the new handler */` |
|        6 | 13111 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13112 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13113 | `	/* Return the old handler */` |
|        6 | 13114 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13115 | `	if( nArg > 0 ){` |
|        6 | 13116 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13117 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13118 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13119 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13120 | `		}else{` |
|        6 | 13121 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13122 | `			/* Install the new handler */` |
|        6 | 13123 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13124 | `		}` |
|        2 | 13125 | `	}` |
|        6 | 13126 | `	return PH7_OK;` |
|        2 | 13127 |  |
|        - | 13128 | `/*` |
|        - | 13129 | ` * bool restore_error_handler(void)` |
|        - | 13130 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13131 | ` * Parameters:` |
|        - | 13132 | ` *  None.` |
|        - | 13133 | ` * Return` |
|        - | 13134 | ` *  Always TRUE.` |
|        - | 13135 | ` */` |
|        6 | 13136 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13137 |  |
|        7 | 13138 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13139 | `	ph7_value *pOld,*pNew;` |
|        - | 13140 | `	/* Point to the old and the new handler */` |
|        7 | 13141 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13142 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13143 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13144 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13145 | `		SXUNUSED(apArg);` |
|        - | 13146 | `		/* No installed callback,return FALSE */` |
|        7 | 13147 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13148 | `		return PH7_OK;` |
|        - | 13149 | `	}` |
|        - | 13150 | `	/* Copy the old callback */` |
|      ! 0 | 13151 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13152 | `	PH7_MemObjRelease(pOld);` |
|        - | 13153 | `	/* Return TRUE */` |
|      ! 0 | 13154 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13155 | `	return PH7_OK;` |
|        4 | 13156 |  |
|        - | 13157 | `/*` |
|        - | 13158 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13159 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13160 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13161 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13162 | ` *  Sets a user-defined error handler function.` |
|        - | 13163 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13164 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13165 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13166 | ` *  conditions (using trigger_error()).` |
|        - | 13167 | ` * Parameters` |
|        - | 13168 | ` *  $error_handler` |
|        - | 13169 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13170 | ` *   describing the error.` |
|        - | 13171 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13172 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13173 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13174 | ` *   The function can be shown as:` |
|        - | 13175 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13176 | ` *     errno` |
|        - | 13177 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13178 | ` *   errstr` |
|        - | 13179 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13180 | ` *   errfile` |
|        - | 13181 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13182 | ` *     was raised in, as a string.` |
|        - | 13183 | ` *  Note:` |
|        - | 13184 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13185 | ` * Return` |
|        - | 13186 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13187 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13188 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13189 | ` */` |
|    10840 | 13190 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13191 |  |
|    10842 | 13192 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13193 | `	ph7_value *pOld,*pNew;` |
|        - | 13194 | `	/* Point to the old and the new handler */` |
|    10842 | 13195 | `	pOld = &pVm->aErrCB[0];` |
|    10842 | 13196 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13197 | `	/* Return the old handler */` |
|    10842 | 13198 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10842 | 13199 | `	if( nArg > 0 ){` |
|    10842 | 13200 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13201 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5415 | 13202 | `			PH7_MemObjRelease(pNew);` |
|     5415 | 13203 | `			ph7_result_bool(pCtx,1);` |
|     2708 | 13204 | `		}else{` |
|     5428 | 13205 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13206 | `			/* Install the new handler */` |
|     5428 | 13207 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13208 | `		}` |
|     5420 | 13209 | `	}` |
|    10842 | 13210 | `	return PH7_OK;` |
|        2 | 13211 |  |
|        - | 13212 | `/*` |
|        - | 13213 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13214 | ` *  Generates a backtrace.` |
|        - | 13215 | ` * Paramaeter` |
|        - | 13216 | ` *  $options` |
|        - | 13217 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13218 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13219 | ` *   all the function/method arguments, to save memory.` |
|        - | 13220 | ` * $limit` |
|        - | 13221 | ` *   (Not Used)` |
|        - | 13222 | ` * Return` |
|        - | 13223 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13224 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13225 | ` *          Name        Type      Description` |
|        - | 13226 | ` *          ------      ------     -----------` |
|        - | 13227 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13228 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13229 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13230 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13231 | ` *          object      object    The current object.` |
|        - | 13232 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13233 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13234 | ` */` |
|      902 | 13235 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13236 |  |
|      904 | 13237 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13238 | `	ph7_value *pArray;` |
|        - | 13239 | `	ph7_class *pClass;` |
|        - | 13240 | `	ph7_value *pValue;` |
|        - | 13241 | `	SyString *pFile;` |
|        - | 13242 | `	/* Create a new array */` |
|      904 | 13243 | `	pArray = ph7_context_new_array(pCtx);` |
|      904 | 13244 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      904 | 13245 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13246 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13247 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13248 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13249 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13250 | `		SXUNUSED(apArg);` |
|      ! 0 | 13251 | `		return PH7_OK;` |
|        - | 13252 | `	}` |
|        - | 13253 | `	/* Dump running function name and it's arguments  */` |
|      904 | 13254 | `	if( pVm->pFrame->pParent ){` |
|      904 | 13255 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13256 | `		ph7_vm_func *pFunc;` |
|        - | 13257 | `		ph7_value *pArg;` |
|      904 | 13258 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      904 | 13259 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      904 | 13260 | `		if( pFrame->pParent && pFunc ){` |
|      904 | 13261 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      904 | 13262 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      904 | 13263 | `			ph7_value_reset_string_cursor(pValue);` |
|      451 | 13264 | `		}` |
|        - | 13265 | `		/* Function arguments */` |
|      904 | 13266 | `		pArg = ph7_context_new_array(pCtx);` |
|      904 | 13267 | `		if( pArg  ){` |
|        - | 13268 | `			ph7_value *pObj;` |
|        - | 13269 | `			VmSlot *aSlot;` |
|        - | 13270 | `			sxu32 n;` |
|        - | 13271 | `			/* Start filling the array with the given arguments */` |
|      904 | 13272 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3614 | 13273 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2712 | 13274 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2712 | 13275 | `				if( pObj ){` |
|     2712 | 13276 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1355 | 13277 | `				}` |
|     1357 | 13278 | `			}` |
|        - | 13279 | `			/* Save the array */` |
|      904 | 13280 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      451 | 13281 | `		}` |
|      451 | 13282 | `	}` |
|      904 | 13283 | `	ph7_value_int(pValue,1);` |
|        - | 13284 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13285 | `	 * line numbers at run-time. )` |
|        - | 13286 | `	 */` |
|      904 | 13287 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13288 | `	/* Current processed script */` |
|      904 | 13289 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      904 | 13290 | `	if( pFile ){` |
|      904 | 13291 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      904 | 13292 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      904 | 13293 | `		ph7_value_reset_string_cursor(pValue);` |
|      451 | 13294 | `	}` |
|        - | 13295 | `	/* Top class */` |
|      904 | 13296 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      904 | 13297 | `	if( pClass ){` |
|      900 | 13298 | `		ph7_value_reset_string_cursor(pValue);` |
|      900 | 13299 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      900 | 13300 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      449 | 13301 | `	}` |
|        - | 13302 | `	/* Return the freshly created array */` |
|      904 | 13303 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13304 | `	/*` |
|        - | 13305 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13306 | `	 * as soon we return from this function.` |
|        - | 13307 | `	 */` |
|      904 | 13308 | `	return PH7_OK;` |
|      453 | 13309 |  |
|        - | 13310 | `/*` |
|        - | 13311 | ` * Generate a small backtrace.` |
|        - | 13312 | ` * Store the generated dump in the given BLOB` |
|        - | 13313 | ` */` |
|        4 | 13314 | `static int VmMiniBacktrace(` |
|        - | 13315 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13316 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13317 | `	)` |
|        1 | 13318 |  |
|        5 | 13319 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13320 | `	ph7_vm_func *pFunc;` |
|        - | 13321 | `	ph7_class *pClass;` |
|        - | 13322 | `	SyString *pFile;` |
|        - | 13323 | `	/* Called function */` |
|        5 | 13324 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13325 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13326 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13327 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13328 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13329 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13330 | `	}else{` |
|      ! 0 | 13331 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13332 | `	}` |
|        5 | 13333 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13334 | `	/* Current processed script */` |
|        5 | 13335 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13336 | `	if( pFile ){` |
|        5 | 13337 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13338 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13339 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13340 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13341 | `	}` |
|        - | 13342 | `	/* Top class */` |
|        5 | 13343 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13344 | `	if( pClass ){` |
|      ! 0 | 13345 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13346 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13347 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13348 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13349 | `	}` |
|        5 | 13350 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13351 | `	/* All done */` |
|        5 | 13352 | `	return SXRET_OK;` |
|        1 | 13353 |  |
|        - | 13354 | `/*` |
|        - | 13355 | ` * void debug_print_backtrace()` |
|        - | 13356 | ` *  Prints a backtrace` |
|        - | 13357 | ` * Parameters` |
|        - | 13358 | ` * None` |
|        - | 13359 | ` * Return` |
|        - | 13360 | ` * NULL` |
|        - | 13361 | ` */` |
|        2 | 13362 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13363 |  |
|        3 | 13364 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13365 | `	SyBlob sDump;` |
|        3 | 13366 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13367 | `	/* Generate the backtrace */` |
|        3 | 13368 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13369 | `	/* Output backtrace */` |
|        3 | 13370 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13371 | `	/* All done,cleanup */` |
|        3 | 13372 | `	SyBlobRelease(&sDump);` |
|        1 | 13373 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13374 | `	SXUNUSED(apArg);` |
|        3 | 13375 | `	return PH7_OK;` |
|        1 | 13376 |  |
|        - | 13377 | `/*` |
|        - | 13378 | ` * string debug_string_backtrace()` |
|        - | 13379 | ` *  Generate a backtrace` |
|        - | 13380 | ` * Parameters` |
|        - | 13381 | ` * None` |
|        - | 13382 | ` * Return` |
|        - | 13383 | ` *  A mini backtrace().` |
|        - | 13384 | ` * Note that this is a symisc extension.` |
|        - | 13385 | ` */` |
|        2 | 13386 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13387 |  |
|        3 | 13388 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13389 | `	SyBlob sDump;` |
|        3 | 13390 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13391 | `	/* Generate the backtrace */` |
|        3 | 13392 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13393 | `	/* Return the backtrace */` |
|        3 | 13394 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13395 | `	/* All done,cleanup */` |
|        3 | 13396 | `	SyBlobRelease(&sDump);` |
|        1 | 13397 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13398 | `	SXUNUSED(apArg);` |
|        3 | 13399 | `	return PH7_OK;` |
|        1 | 13400 |  |
|        - | 13401 | `/*` |
|        - | 13402 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13403 | ` * exception is triggered.` |
|        - | 13404 | ` */` |
|      512 | 13405 | `static sxi32 VmUncaughtException(` |
|        - | 13406 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13407 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13408 | `	)` |
|        1 | 13409 |  |
|        - | 13410 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13411 | `	int nArg = 1;` |
|        - | 13412 | `	sxi32 rc;` |
|      513 | 13413 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13414 | `		/* Nesting limit reached */` |
|      ! 0 | 13415 | `		return SXRET_OK;` |
|        - | 13416 | `	}` |
|        - | 13417 | `	/* Call any exception handler if available */` |
|      513 | 13418 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13419 | `	if( pThis ){` |
|        - | 13420 | `		/* Load the exception instance */` |
|      513 | 13421 | `		sArg.x.pOther = pThis;` |
|      513 | 13422 | `		pThis->iRef++;` |
|      513 | 13423 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13424 | `	}else{` |
|      ! 0 | 13425 | `		nArg = 0;` |
|        - | 13426 | `	}` |
|      513 | 13427 | `	apArg[0] = &sArg;` |
|        - | 13428 | `	/* Call the exception handler if available */` |
|      513 | 13429 | `	pVm->nExceptDepth++;` |
|      513 | 13430 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13431 | `	pVm->nExceptDepth--;` |
|      513 | 13432 | `	if( rc != SXRET_OK ){` |
|        - | 13433 | `		SyBlob sMsgBuf;` |
|      511 | 13434 | `		const char *zClass = "Exception";` |
|      511 | 13435 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13436 | `		const char *zMsg;` |
|        - | 13437 | `		sxu32 nMsg;` |
|        - | 13438 | `		const char *zFuncName;` |
|        - | 13439 | `		int nFuncLen;` |
|      511 | 13440 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13441 | `		if( pThis ){` |
|        - | 13442 | `			ph7_class_method *pGetMessage;` |
|        - | 13443 | `			ph7_value sMsg;` |
|        - | 13444 | `			const char *zTmp;` |
|        - | 13445 | `			int nTmp;` |
|      511 | 13446 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13447 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13448 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13449 | `			if( pGetMessage ){` |
|      511 | 13450 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13451 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13452 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13453 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13454 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13455 | `					}` |
|      255 | 13456 | `				}` |
|      511 | 13457 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13458 | `			}` |
|      255 | 13459 | `		}` |
|      511 | 13460 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13461 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13462 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13463 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13464 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13465 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13466 | `		rc = SXERR_ABORT;` |
|      255 | 13467 | `	}` |
|      513 | 13468 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13469 | `	return rc;` |
|      257 | 13470 |  |
|        - | 13471 | `/*` |
|        - | 13472 | ` * Throw a user exception.` |
|        - | 13473 | ` *` |
|        - | 13474 | ` * Exception dispatch follows this sequence:` |
|        - | 13475 | ` *` |
|        - | 13476 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13477 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13478 | ` *` |
|        - | 13479 | ` * 2. If NO catch matches:` |
|        - | 13480 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13481 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13482 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13483 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13484 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13485 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13486 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13487 | ` *` |
|        - | 13488 | ` * 3. If a catch DOES match:` |
|        - | 13489 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13490 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13491 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13492 | ` *       finally block.` |
|        - | 13493 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13494 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13495 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13496 | ` *       in pPendingException (step 2c).` |
|        - | 13497 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13498 | ` *    d. Run finally (if present).` |
|        - | 13499 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13500 | ` *       that handlers are restored and finally has run.` |
|        - | 13501 | ` */` |
|      850 | 13502 | `static sxi32 VmThrowException(` |
|        - | 13503 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13504 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13505 | `	)` |
|        2 | 13506 |  |
|        - | 13507 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13508 | `	ph7_exception **apException;` |
|        - | 13509 | `	ph7_exception *pException;` |
|        - | 13510 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13511 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13512 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      852 | 13513 | `	VmCoalesceDisarm(pVm);` |
|        - | 13514 | `	/* Point to the stack of loaded exceptions */` |
|      852 | 13515 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      852 | 13516 | `	pException = 0;` |
|      852 | 13517 | `	pCatch = 0;` |
|      852 | 13518 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13519 | `		ph7_exception_block *aCatch;` |
|        - | 13520 | `		ph7_class *pClass;` |
|        - | 13521 | `		SyString *aNames;` |
|        - | 13522 | `		sxu32 nNames;` |
|        - | 13523 | `		int matched;` |
|        - | 13524 | `		sxu32 j,k;` |
|        - | 13525 | `		/* Locate the appropriate block to execute */` |
|      332 | 13526 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      332 | 13527 | `		(void)SySetPop(&pVm->aException);` |
|      332 | 13528 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      340 | 13529 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13530 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      338 | 13531 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      338 | 13532 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      338 | 13533 | `			matched = 0;` |
|      364 | 13534 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13535 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13536 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13537 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      356 | 13538 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      356 | 13539 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13540 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13541 | `					continue;` |
|        - | 13542 | `				}` |
|      356 | 13543 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      330 | 13544 | `					matched = 1;` |
|      330 | 13545 | `					break;` |
|        - | 13546 | `				}` |
|       14 | 13547 | `			}` |
|      338 | 13548 | `			if( matched ){` |
|        - | 13549 | `				/* Catch block found,break immediately */` |
|      330 | 13550 | `				pCatch = &aCatch[j];` |
|      330 | 13551 | `				break;` |
|        - | 13552 | `			}` |
|        5 | 13553 | `		}` |
|      165 | 13554 | `	}` |
|        - | 13555 | `	/* Execute the cached block if available */` |
|      852 | 13556 | `	if( pCatch == 0 ){` |
|        - | 13557 | `		sxi32 rc;` |
|        - | 13558 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13559 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13560 | `			pException->iFinallyDone = 1;` |
|        3 | 13561 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13562 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13563 | `				return SXERR_ABORT;` |
|        - | 13564 | `			}` |
|        1 | 13565 | `		}` |
|        - | 13566 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13567 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13568 | `			/* Re-throw to the outer handler */` |
|        3 | 13569 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13570 | `		}` |
|        - | 13571 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13572 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13573 | `		 * exception instead of reporting it uncaught.` |
|        - | 13574 | `		 */` |
|      522 | 13575 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13576 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13577 | `			 * by looking for a catch frame on the stack.` |
|        - | 13578 | `			 */` |
|      522 | 13579 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13580 | `			int inCatch = 0;` |
|     1050 | 13581 | `			while( pF ){` |
|      538 | 13582 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13583 | `					inCatch = 1;` |
|        9 | 13584 | `					break;` |
|        - | 13585 | `				}` |
|      529 | 13586 | `				pF = pF->pParent;` |
|        1 | 13587 | `			}` |
|      522 | 13588 | `			if( inCatch ){` |
|        - | 13589 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13590 | `				pThis->iRef++;` |
|        9 | 13591 | `				pVm->pPendingException = pThis;` |
|        9 | 13592 | `				return SXRET_OK;` |
|        - | 13593 | `			}` |
|      256 | 13594 | `		}` |
|        - | 13595 | `		/* Truly uncaught */` |
|      513 | 13596 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13597 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13598 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13599 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13600 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13601 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13602 | `			}` |
|      ! 0 | 13603 | `		}` |
|      513 | 13604 | `		return rc;` |
|      ! 0 | 13605 | `	}else{` |
|      330 | 13606 | `		VmFrame *pFrame = pVm->pFrame;` |
|      330 | 13607 | `		ph7_exception **apSaved = 0;` |
|        - | 13608 | `		sxu32 nSavedCount;` |
|        - | 13609 | `		sxi32 rc;` |
|      330 | 13610 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      330 | 13611 | `		if( pException->pFrame == pFrame ){` |
|      230 | 13612 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      114 | 13613 | `		}` |
|        - | 13614 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13615 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13616 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13617 | `		 */` |
|      330 | 13618 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      330 | 13619 | `		if( nSavedCount > 0 ){` |
|       16 | 13620 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13621 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13622 | `			if( apSaved ){` |
|       16 | 13623 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13624 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13625 | `				SySetReset(&pVm->aException);` |
|        5 | 13626 | `			}` |
|        5 | 13627 | `		}` |
|        - | 13628 | `		/* Create a private frame first */` |
|      330 | 13629 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      330 | 13630 | `		if( rc == SXRET_OK ){` |
|      330 | 13631 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      330 | 13632 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      330 | 13633 | `			if( pObj ){` |
|      330 | 13634 | `				pThis->iRef++;` |
|      330 | 13635 | `				pObj->x.pOther = pThis;` |
|      330 | 13636 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      164 | 13637 | `			}` |
|        - | 13638 | `			/* Execute the catch block */` |
|      330 | 13639 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13640 | `			/* Leave the frame */` |
|      330 | 13641 | `			VmLeaveFrame(&(*pVm));` |
|      164 | 13642 | `		}` |
|        - | 13643 | `		/* Restore the outer exception handlers */` |
|      330 | 13644 | `		if( apSaved ){` |
|        - | 13645 | `			sxu32 k;` |
|        - | 13646 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13647 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13648 | `			 * Restore the original outer entries.` |
|        - | 13649 | `			 */` |
|       11 | 13650 | `			SySetReset(&pVm->aException);` |
|       21 | 13651 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13652 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13653 | `			}` |
|       11 | 13654 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13655 | `		}` |
|        - | 13656 | `		/* Execute the finally block after catch */` |
|      330 | 13657 | `		if( pException->iHasFinally ){` |
|       16 | 13658 | `			pException->iFinallyDone = 1;` |
|        - | 13659 | `			{` |
|       16 | 13660 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13661 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13662 | `					return SXERR_ABORT;` |
|        - | 13663 | `				}` |
|        - | 13664 | `			}` |
|        7 | 13665 | `		}` |
|      330 | 13666 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13667 | `			return SXERR_ABORT;` |
|        - | 13668 | `		}` |
|        - | 13669 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13670 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13671 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13672 | `		 */` |
|      330 | 13673 | `		if( pVm->pPendingException ){` |
|        9 | 13674 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13675 | `			pVm->pPendingException = 0;` |
|        9 | 13676 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13677 | `		}` |
|        - | 13678 | `	}` |
|        - | 13679 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13680 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13681 | `	 */` |
|      322 | 13682 | `	return SXRET_OK;` |
|      427 | 13683 |  |
|        - | 13684 | `/*` |
|        - | 13685 | ` * Section:` |
|        - | 13686 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13687 | ` * Status:` |
|        - | 13688 | ` *    Stable.` |
|        - | 13689 | ` */` |
|        - | 13690 | `/*` |
|        - | 13691 | ` * string ph7version(void)` |
|        - | 13692 | ` *  Returns the running version of the PH7 version.` |
|        - | 13693 | ` * Parameters` |
|        - | 13694 | ` *  None` |
|        - | 13695 | ` * Return` |
|        - | 13696 | ` * Current PH7 version.` |
|        - | 13697 | ` */` |
|        2 | 13698 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13699 |  |
|        1 | 13700 | `	SXUNUSED(nArg);` |
|        1 | 13701 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13702 | `	/* Current engine version */` |
|        3 | 13703 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13704 | `	return PH7_OK;` |
|        1 | 13705 |  |
|        - | 13706 | `/*` |
|        - | 13707 | ` * string phpversion([ string $extension ])` |
|        - | 13708 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 13709 | ` * Parameters` |
|        - | 13710 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 13711 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 13712 | ` * Return` |
|        - | 13713 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 13714 | ` */` |
|        4 | 13715 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13716 |  |
|        2 | 13717 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 13718 | `	if( nArg > 0 ){` |
|      ! 0 | 13719 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13720 | `		return PH7_OK;` |
|        - | 13721 | `	}` |
|        5 | 13722 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 13723 | `	return PH7_OK;` |
|        3 | 13724 |  |
|        - | 13725 | `/*` |
|        - | 13726 | ` * string php_sapi_name(void)` |
|        - | 13727 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 13728 | ` * Parameters` |
|        - | 13729 | ` *  None` |
|        - | 13730 | ` * Return` |
|        - | 13731 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 13732 | ` */` |
|        2 | 13733 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13734 |  |
|        3 | 13735 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 13736 | `	SXUNUSED(nArg);` |
|        1 | 13737 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 13738 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 13739 | `	return PH7_OK;` |
|        1 | 13740 |  |
|        - | 13741 | `/*` |
|        - | 13742 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13743 | ` */` |
|        - | 13744 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13745 | ` "<html><head>"\` |
|        - | 13746 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13747 | ` "<style type=\"text/css\">"\` |
|        - | 13748 | ` "div {"\` |
|        - | 13749 | `     "border: 1px solid #cccccc;"\` |
|        - | 13750 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13751 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13752 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13753 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13754 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13755 | `     "-o-border-radius: 10px;"\` |
|        - | 13756 | `     "border-radius: 10px;"\` |
|        - | 13757 | `     "padding-left: 2em;"\` |
|        - | 13758 | `     "background-color: white;"\` |
|        - | 13759 | `     "margin-left: auto;"\` |
|        - | 13760 | `     "font-family: verdana;"\` |
|        - | 13761 | `     "padding-right: 2em;"\` |
|        - | 13762 | `     "margin-right: auto;"\` |
|        - | 13763 | `     "}"\` |
|        - | 13764 | `     "body {"\` |
|        - | 13765 | `     "padding: 0.2em;"\` |
|        - | 13766 | `     "font-style: normal;"\` |
|        - | 13767 | `     "font-size: medium;"\` |
|        - | 13768 | `     "background-color: #f2f2f2;"\` |
|        - | 13769 | `     "}"\` |
|        - | 13770 | `     "hr {"\` |
|        - | 13771 | `     "border-style: solid none none;"\` |
|        - | 13772 | `     "border-width: 1px medium medium;"\` |
|        - | 13773 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13774 | `     "height: 1px;"\` |
|        - | 13775 | `     "}"\` |
|        - | 13776 | `     "a {"\` |
|        - | 13777 | `     "color: #3366cc;"\` |
|        - | 13778 | `     "text-decoration: none;"\` |
|        - | 13779 | `     "}"\` |
|        - | 13780 | `     "a:hover {"\` |
|        - | 13781 | `     "color: #999999;"\` |
|        - | 13782 | `     "}"\` |
|        - | 13783 | `     "a:active {"\` |
|        - | 13784 | `     "color: #663399;"\` |
|        - | 13785 | `     "}"\` |
|        - | 13786 | `     "h1 {"\` |
|        - | 13787 | `     "margin: 0;"\` |
|        - | 13788 | `     "padding: 0;"\` |
|        - | 13789 | `     "font-family: Verdana;"\` |
|        - | 13790 | `     "font-weight: bold;"\` |
|        - | 13791 | `     "font-style: normal;"\` |
|        - | 13792 | `     "font-size: medium;"\` |
|        - | 13793 | `     "text-transform: capitalize;"\` |
|        - | 13794 | `     "color: #0a328c;"\` |
|        - | 13795 | `     "}"\` |
|        - | 13796 | `     "p {"\` |
|        - | 13797 | `     "margin: 0 auto;"\` |
|        - | 13798 | `     "font-size: medium;"\` |
|        - | 13799 | `     "font-style: normal;"\` |
|        - | 13800 | `     "font-family: verdana;"\` |
|        - | 13801 | `     "}"\` |
|        - | 13802 | `"</style></head><body>"\` |
|        - | 13803 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13804 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13805 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13806 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13807 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13808 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13809 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13810 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13811 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13812 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13813 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13814 |  |
|        - | 13815 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13816 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13817 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13818 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13819 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13820 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13821 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13822 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13823 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13824 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13825 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13826 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13827 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13828 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13829 |  |
|        - | 13830 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13831 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13832 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13833 | `"&nbsp;*<br>"\` |
|        - | 13834 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13835 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13836 | `"&nbsp;* are met:<br>"\` |
|        - | 13837 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13838 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13839 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13840 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13841 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13842 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13843 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13844 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13845 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13846 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13847 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13848 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13849 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13850 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13851 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13852 | `"&nbsp;*<br>"\` |
|        - | 13853 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13854 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13855 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13856 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13857 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13858 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13859 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13860 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13861 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13862 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13863 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13864 | `"&nbsp;*/<br>"\` |
|        - | 13865 | `"</span></small></small></p>"\` |
|        - | 13866 | `"</div></body></html>"` |
|        - | 13867 | `/*` |
|        - | 13868 | ` * bool ph7credits(void)` |
|        - | 13869 | ` * bool ph7info(void)` |
|        - | 13870 | ` * bool ph7copyright(void)` |
|        - | 13871 | ` *  Prints out the credits for PH7 engine` |
|        - | 13872 | ` * Parameters` |
|        - | 13873 | ` *  None` |
|        - | 13874 | ` * Return` |
|        - | 13875 | ` *  Always TRUE` |
|        - | 13876 | ` */` |
|        2 | 13877 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13878 |  |
|        3 | 13879 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13880 | `	/* Expand the HTML page above*/` |
|        3 | 13881 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13882 | `	ph7_context_output_format(` |
|        1 | 13883 | `		pCtx,` |
|        - | 13884 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13885 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13886 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13887 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13888 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13889 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13890 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13891 | `#ifdef __WINNT__` |
|        - | 13892 | `		"Windows NT"` |
|        - | 13893 | `#elif defined(__UNIXES__)` |
|        - | 13894 | `		"UNIX-Like"` |
|        - | 13895 | `#else` |
|        - | 13896 | `		"Other OS"` |
|        - | 13897 | `#endif` |
|        - | 13898 | `		);` |
|        3 | 13899 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13900 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13901 | `	SXUNUSED(apArg);` |
|        - | 13902 | `	/* Return TRUE */` |
|        - | 13903 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13904 | `	return PH7_OK;` |
|        1 | 13905 |  |
|        - | 13906 | `/*` |
|        - | 13907 | ` * Section:` |
|        - | 13908 | ` *    URL related routines.` |
|        - | 13909 | ` * Status:` |
|        - | 13910 | ` *    Stable.` |
|        - | 13911 | ` */` |
|        - | 13912 | `/*` |
|        - | 13913 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13914 | ` *  Parse a URL and return its fields.` |
|        - | 13915 | ` * Parameters` |
|        - | 13916 | ` *  $url` |
|        - | 13917 | ` *   The URL to parse.` |
|        - | 13918 | ` * $component` |
|        - | 13919 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13920 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13921 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13922 | ` *  in which case the return value will be an integer).` |
|        - | 13923 | ` * Return` |
|        - | 13924 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13925 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13926 | ` *  this array are:` |
|        - | 13927 | ` *   scheme - e.g. http` |
|        - | 13928 | ` *   host` |
|        - | 13929 | ` *   port` |
|        - | 13930 | ` *   user` |
|        - | 13931 | ` *   pass` |
|        - | 13932 | ` *   path` |
|        - | 13933 | ` *   query - after the question mark ?` |
|        - | 13934 | ` *   fragment - after the hashmark #` |
|        - | 13935 | ` * Note:` |
|        - | 13936 | ` *  FALSE is returned on failure.` |
|        - | 13937 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13938 | ` *  with the standard PHP engine.` |
|        - | 13939 | ` */` |
|       28 | 13940 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13941 |  |
|        - | 13942 | `	const char *zStr; /* Input string */` |
|        - | 13943 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13944 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13945 | `	int nLen;` |
|        - | 13946 | `	sxi32 rc;` |
|       29 | 13947 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13948 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13949 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13950 | `		return PH7_OK;` |
|        - | 13951 | `	}` |
|        - | 13952 | `	/* Extract the given URI */` |
|       29 | 13953 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13954 | `	if( nLen < 1 ){` |
|        - | 13955 | `		/* Nothing to process,return FALSE */` |
|        3 | 13956 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13957 | `		return PH7_OK;` |
|        - | 13958 | `	}` |
|        - | 13959 | `	/* Get a parse */` |
|       27 | 13960 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13961 | `	if( rc != SXRET_OK ){` |
|        - | 13962 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13963 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13964 | `		return PH7_OK;` |
|        - | 13965 | `	}` |
|       27 | 13966 | `	if( nArg > 1 ){` |
|      ! 0 | 13967 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13968 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13969 | `		switch(nComponent){` |
|      ! 0 | 13970 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13971 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13972 | `			if( pComp->nByte < 1 ){` |
|        - | 13973 | `				/* No available value,return NULL */` |
|      ! 0 | 13974 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13975 | `			}else{` |
|      ! 0 | 13976 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13977 | `			}` |
|      ! 0 | 13978 | `			break;` |
|      ! 0 | 13979 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13980 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13981 | `			if( pComp->nByte < 1 ){` |
|        - | 13982 | `				/* No available value,return NULL */` |
|      ! 0 | 13983 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13984 | `			}else{` |
|      ! 0 | 13985 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13986 | `			}` |
|      ! 0 | 13987 | `			break;` |
|      ! 0 | 13988 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13989 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13990 | `			if( pComp->nByte < 1 ){` |
|        - | 13991 | `				/* No available value,return NULL */` |
|      ! 0 | 13992 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13993 | `			}else{` |
|      ! 0 | 13994 | `				int iPort = 0;` |
|        - | 13995 | `				/* Cast the value to integer */` |
|      ! 0 | 13996 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13997 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13998 | `			}` |
|      ! 0 | 13999 | `			break;` |
|      ! 0 | 14000 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14001 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14002 | `			if( pComp->nByte < 1 ){` |
|        - | 14003 | `				/* No available value,return NULL */` |
|      ! 0 | 14004 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14005 | `			}else{` |
|      ! 0 | 14006 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14007 | `			}` |
|      ! 0 | 14008 | `			break;` |
|      ! 0 | 14009 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14010 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14011 | `			if( pComp->nByte < 1 ){` |
|        - | 14012 | `				/* No available value,return NULL */` |
|      ! 0 | 14013 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14014 | `			}else{` |
|      ! 0 | 14015 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14016 | `			}` |
|      ! 0 | 14017 | `			break;` |
|      ! 0 | 14018 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14019 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14020 | `			if( pComp->nByte < 1 ){` |
|        - | 14021 | `				/* No available value,return NULL */` |
|      ! 0 | 14022 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14023 | `			}else{` |
|      ! 0 | 14024 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14025 | `			}` |
|      ! 0 | 14026 | `			break;` |
|      ! 0 | 14027 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14028 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14029 | `			if( pComp->nByte < 1 ){` |
|        - | 14030 | `				/* No available value,return NULL */` |
|      ! 0 | 14031 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14032 | `			}else{` |
|      ! 0 | 14033 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14034 | `			}` |
|      ! 0 | 14035 | `			break;` |
|      ! 0 | 14036 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14037 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14038 | `			if( pComp->nByte < 1 ){` |
|        - | 14039 | `				/* No available value,return NULL */` |
|      ! 0 | 14040 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14041 | `			}else{` |
|      ! 0 | 14042 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14043 | `			}` |
|      ! 0 | 14044 | `			break;` |
|      ! 0 | 14045 | `		default:` |
|        - | 14046 | `			/* No such entry,return NULL */` |
|      ! 0 | 14047 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14048 | `			break;` |
|        - | 14049 | `		}` |
|      ! 0 | 14050 | `	}else{` |
|        - | 14051 | `		ph7_value *pArray,*pValue;` |
|        - | 14052 | `		/* Return an associative array */` |
|       27 | 14053 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14054 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14055 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14056 | `			/* Out of memory */` |
|      ! 0 | 14057 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14058 | `			/* Return false */` |
|      ! 0 | 14059 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14060 | `			return PH7_OK;` |
|        - | 14061 | `		}` |
|        - | 14062 | `		/* Fill the array */` |
|       27 | 14063 | `		pComp = &sURI.sScheme;` |
|       27 | 14064 | `		if( pComp->nByte > 0 ){` |
|       19 | 14065 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14066 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14067 | `		}` |
|        - | 14068 | `		/* Reset the string cursor */` |
|       27 | 14069 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14070 | `		pComp = &sURI.sHost;` |
|       27 | 14071 | `		if( pComp->nByte > 0 ){` |
|       25 | 14072 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14073 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14074 | `		}` |
|        - | 14075 | `		/* Reset the string cursor */` |
|       27 | 14076 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14077 | `		pComp = &sURI.sPort;` |
|       27 | 14078 | `		if( pComp->nByte > 0 ){` |
|       11 | 14079 | `			int iPort = 0;/* cc warning */` |
|        - | 14080 | `			/* Convert to integer */` |
|       11 | 14081 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14082 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14083 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14084 | `		}` |
|        - | 14085 | `		/* Reset the string cursor */` |
|       27 | 14086 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14087 | `		pComp = &sURI.sUser;` |
|       27 | 14088 | `		if( pComp->nByte > 0 ){` |
|        7 | 14089 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14090 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14091 | `		}` |
|        - | 14092 | `		/* Reset the string cursor */` |
|       27 | 14093 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14094 | `		pComp = &sURI.sPass;` |
|       27 | 14095 | `		if( pComp->nByte > 0 ){` |
|        7 | 14096 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14097 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14098 | `		}` |
|        - | 14099 | `		/* Reset the string cursor */` |
|       27 | 14100 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14101 | `		pComp = &sURI.sPath;` |
|       27 | 14102 | `		if( pComp->nByte > 0 ){` |
|       17 | 14103 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14104 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14105 | `		}` |
|        - | 14106 | `		/* Reset the string cursor */` |
|       27 | 14107 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14108 | `		pComp = &sURI.sQuery;` |
|       27 | 14109 | `		if( pComp->nByte > 0 ){` |
|        5 | 14110 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14111 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14112 | `		}` |
|        - | 14113 | `		/* Reset the string cursor */` |
|       27 | 14114 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14115 | `		pComp = &sURI.sFragment;` |
|       27 | 14116 | `		if( pComp->nByte > 0 ){` |
|        5 | 14117 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14118 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14119 | `		}` |
|        - | 14120 | `		/* Return the created array */` |
|       27 | 14121 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14122 | `		/* NOTE:` |
|        - | 14123 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14124 | `		 * automatically as soon we return from this function.` |
|        - | 14125 | `		 */` |
|        - | 14126 | `	}` |
|        - | 14127 | `	/* All done */` |
|       27 | 14128 | `	return PH7_OK;` |
|       15 | 14129 |  |
|        - | 14130 | `/*` |
|        - | 14131 | ` * Section:` |
|        - | 14132 | ` *   Array related routines.` |
|        - | 14133 | ` * Status:` |
|        - | 14134 | ` *    Stable.` |
|        - | 14135 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14136 | ` *  Array related functions that need access to the underlying` |
|        - | 14137 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14138 | ` */` |
|        - | 14139 | `/*` |
|        - | 14140 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14141 | ` * of the following structure.` |
|        - | 14142 | ` */` |
|        - | 14143 | `struct compact_data` |
|        - | 14144 |  |
|        - | 14145 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14146 | `	int nRecCount;      /* Recursion count */` |
|        - | 14147 | `};` |
|        - | 14148 | `/*` |
|        - | 14149 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14150 | ` */` |
|      ! 0 | 14151 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14152 |  |
|      ! 0 | 14153 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14154 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14155 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14156 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14157 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14158 | `		SyString sVar;` |
|      ! 0 | 14159 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14160 | `		if( sVar.nByte > 0 ){` |
|        - | 14161 | `			/* Query the current frame */` |
|      ! 0 | 14162 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14163 | `			/* ^` |
|        - | 14164 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14165 | `			 */` |
|      ! 0 | 14166 | `			if( pKey ){` |
|        - | 14167 | `				/* Perform the insertion */` |
|      ! 0 | 14168 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14169 | `			}` |
|      ! 0 | 14170 | `		}` |
|      ! 0 | 14171 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14172 | `		int rc;` |
|        - | 14173 | `		/* Recursively traverse this array */` |
|      ! 0 | 14174 | `		pData->nRecCount++;` |
|      ! 0 | 14175 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14176 | `		pData->nRecCount--;` |
|      ! 0 | 14177 | `		return rc;` |
|        - | 14178 | `	}` |
|      ! 0 | 14179 | `	return SXRET_OK;` |
|      ! 0 | 14180 |  |
|        - | 14181 | `/*` |
|        - | 14182 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14183 | ` *  Create array containing variables and their values.` |
|        - | 14184 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14185 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14186 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14187 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14188 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14189 | ` * Parameters` |
|        - | 14190 | ` *  $varname` |
|        - | 14191 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14192 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14193 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14194 | ` *   it recursively.` |
|        - | 14195 | ` * Return` |
|        - | 14196 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14197 | ` */` |
|        2 | 14198 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14199 |  |
|        - | 14200 | `	ph7_value *pArray,*pObj;` |
|        3 | 14201 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14202 | `	const char *zName;` |
|        - | 14203 | `	SyString sVar;` |
|        - | 14204 | `	int i,nLen;` |
|        3 | 14205 | `	if( nArg < 1 ){` |
|        - | 14206 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14207 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14208 | `		return PH7_OK;` |
|        - | 14209 | `	}` |
|        - | 14210 | `	/* Create the array */` |
|        3 | 14211 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14212 | `	if( pArray == 0 ){` |
|        - | 14213 | `		/* Out of memory */` |
|      ! 0 | 14214 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14215 | `		/* Return NULL */` |
|      ! 0 | 14216 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14217 | `		return PH7_OK;` |
|        - | 14218 | `	}` |
|        - | 14219 | `	/* Perform the requested operation */` |
|        7 | 14220 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14221 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14222 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14223 | `				struct compact_data sData;` |
|      ! 0 | 14224 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14225 | `				/* Recursively walk the array */` |
|      ! 0 | 14226 | `				sData.nRecCount = 0;` |
|      ! 0 | 14227 | `				sData.pArray = pArray;` |
|      ! 0 | 14228 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14229 | `			}` |
|      ! 0 | 14230 | `		}else{` |
|        - | 14231 | `			/* Extract variable name */` |
|        5 | 14232 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14233 | `			if( nLen > 0 ){` |
|        5 | 14234 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14235 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14236 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14237 | `				if( pObj ){` |
|        5 | 14238 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14239 | `				}` |
|        2 | 14240 | `			}` |
|        - | 14241 | `		}` |
|        3 | 14242 | `	}` |
|        - | 14243 | `	/* Return the array */` |
|        3 | 14244 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14245 | `	return PH7_OK;` |
|        2 | 14246 |  |
|        - | 14247 | `/*` |
|        - | 14248 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14249 | ` * of the following structure.` |
|        - | 14250 | ` */` |
|        - | 14251 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14252 | `struct extract_aux_data` |
|        - | 14253 |  |
|        - | 14254 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14255 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14256 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14257 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14258 | `	int iFlags;           /* Control flags */` |
|        - | 14259 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14260 | `};` |
|        - | 14261 | `/* Forward declaration */` |
|        - | 14262 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14263 | `/*` |
|        - | 14264 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14265 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14266 | ` * Parameters` |
|        - | 14267 | ` * $var_array` |
|        - | 14268 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14269 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14270 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14271 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14272 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14273 | ` * $extract_type` |
|        - | 14274 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14275 | ` *  It can be one of the following values:` |
|        - | 14276 | ` *   EXTR_OVERWRITE` |
|        - | 14277 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14278 | ` *   EXTR_SKIP` |
|        - | 14279 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14280 | ` *   EXTR_PREFIX_SAME` |
|        - | 14281 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14282 | ` *   EXTR_PREFIX_ALL` |
|        - | 14283 | ` *       Prefix all variable names with prefix.` |
|        - | 14284 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14285 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14286 | ` *   EXTR_IF_EXISTS` |
|        - | 14287 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14288 | ` *       otherwise do nothing.` |
|        - | 14289 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14290 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14291 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14292 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14293 | ` *      the current symbol table.` |
|        - | 14294 | ` * $prefix` |
|        - | 14295 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14296 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14297 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14298 | ` *  underscore character.` |
|        - | 14299 | ` * Return` |
|        - | 14300 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14301 | ` */` |
|        4 | 14302 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14303 |  |
|        - | 14304 | `	extract_aux_data sAux;` |
|        - | 14305 | `	ph7_hashmap *pMap;` |
|        5 | 14306 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14307 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14308 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14309 | `		return PH7_OK;` |
|        - | 14310 | `	}` |
|        - | 14311 | `	/* Point to the target hashmap */` |
|        5 | 14312 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14313 | `	if( pMap->nEntry < 1 ){` |
|        - | 14314 | `		/* Empty map,return  0 */` |
|      ! 0 | 14315 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14316 | `		return PH7_OK;` |
|        - | 14317 | `	}` |
|        - | 14318 | `	/* Prepare the aux data */` |
|        5 | 14319 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14320 | `	if( nArg > 1 ){` |
|        3 | 14321 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14322 | `		if( nArg > 2 ){` |
|      ! 0 | 14323 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14324 | `		}` |
|        1 | 14325 | `	}` |
|        5 | 14326 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14327 | `	/* Invoke the worker callback */` |
|        5 | 14328 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14329 | `	/* Number of variables successfully imported */` |
|        5 | 14330 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14331 | `	return PH7_OK;` |
|        3 | 14332 |  |
|        - | 14333 | `/*` |
|        - | 14334 | ` * Worker callback for the [extract()] function defined` |
|        - | 14335 | ` * below.` |
|        - | 14336 | ` */` |
|        8 | 14337 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14338 |  |
|        9 | 14339 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14340 | `	int iFlags = pAux->iFlags;` |
|        9 | 14341 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14342 | `	ph7_value *pObj;` |
|        - | 14343 | `	SyString sVar;` |
|        9 | 14344 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14345 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14346 | `	}` |
|        - | 14347 | `	/* Perform a string cast */` |
|        9 | 14348 | `	PH7_MemObjToString(pKey);` |
|        9 | 14349 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14350 | `		/* Unavailable variable name */` |
|      ! 0 | 14351 | `		return SXRET_OK;` |
|        - | 14352 | `	}` |
|        9 | 14353 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14354 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14355 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14356 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14357 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14358 | `			);` |
|      ! 0 | 14359 | `	}else{` |
|       13 | 14360 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14361 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14362 | `	}` |
|        9 | 14363 | `	sVar.zString = pAux->zWorker;` |
|        - | 14364 | `	/* Try to extract the variable */` |
|        9 | 14365 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14366 | `	if( pObj ){` |
|        - | 14367 | `		/* Collision */` |
|        5 | 14368 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14369 | `			return SXRET_OK;` |
|        - | 14370 | `		}` |
|        5 | 14371 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14372 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14373 | `				/* Already prefixed */` |
|      ! 0 | 14374 | `				return SXRET_OK;` |
|        - | 14375 | `			}` |
|      ! 0 | 14376 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14377 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14378 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14379 | `				);` |
|      ! 0 | 14380 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14381 | `		}` |
|        3 | 14382 | `	}else{` |
|        - | 14383 | `		/* Create the variable */` |
|        5 | 14384 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14385 | `	}` |
|        9 | 14386 | `	if( pObj ){` |
|        - | 14387 | `		/* Overwrite the old value */` |
|        9 | 14388 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14389 | `		/* Increment counter */` |
|        9 | 14390 | `		pAux->iCount++;` |
|        4 | 14391 | `	}` |
|        9 | 14392 | `	return SXRET_OK;` |
|        5 | 14393 |  |
|        - | 14394 | `/*` |
|        - | 14395 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14396 | ` * defined below.` |
|        - | 14397 | ` */` |
|        2 | 14398 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14399 |  |
|        3 | 14400 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14401 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14402 | `	ph7_value *pObj;` |
|        - | 14403 | `	SyString sVar;` |
|        - | 14404 | `	/* Perform a string cast */` |
|        3 | 14405 | `	PH7_MemObjToString(pKey);` |
|        3 | 14406 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14407 | `		/* Unavailable variable name */` |
|      ! 0 | 14408 | `		return SXRET_OK;` |
|        - | 14409 | `	}` |
|        3 | 14410 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14411 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14412 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14413 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14414 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14415 | `			);` |
|        2 | 14416 | `	}else{` |
|      ! 0 | 14417 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14418 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14419 | `	}` |
|        3 | 14420 | `	sVar.zString = pAux->zWorker;` |
|        - | 14421 | `	/* Extract the variable */` |
|        3 | 14422 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14423 | `	if( pObj ){` |
|        3 | 14424 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14425 | `	}` |
|        3 | 14426 | `	return SXRET_OK;` |
|        2 | 14427 |  |
|        - | 14428 | `/*` |
|        - | 14429 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14430 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14431 | ` * Parameters` |
|        - | 14432 | ` * $types` |
|        - | 14433 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14434 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14435 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14436 | ` *  POST includes the POST uploaded file information.` |
|        - | 14437 | ` *  Note:` |
|        - | 14438 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14439 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14440 | ` * $prefix` |
|        - | 14441 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14442 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14443 | ` *  variable named $pref_userid.` |
|        - | 14444 | ` * Return` |
|        - | 14445 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14446 | ` */` |
|        2 | 14447 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14448 |  |
|        - | 14449 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14450 | `	extract_aux_data sAux;` |
|        - | 14451 | `	int nLen,nPrefixLen;` |
|        - | 14452 | `	ph7_value *pSuper;` |
|        - | 14453 | `	ph7_vm *pVm;` |
|        - | 14454 | `	/* By default import only $_GET variables  */` |
|        3 | 14455 | `	zImport = "G";` |
|        3 | 14456 | `	nLen = (int)sizeof(char);` |
|        3 | 14457 | `	zPrefix = 0;` |
|        3 | 14458 | `	nPrefixLen = 0;` |
|        3 | 14459 | `	if( nArg > 0 ){` |
|        3 | 14460 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14461 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14462 | `		}` |
|        3 | 14463 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14464 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14465 | `		}` |
|        1 | 14466 | `	}` |
|        - | 14467 | `	/* Point to the underlying VM */` |
|        3 | 14468 | `	pVm = pCtx->pVm;` |
|        - | 14469 | `	/* Initialize the aux data */` |
|        3 | 14470 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14471 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14472 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14473 | `	sAux.pVm = pVm;` |
|        - | 14474 | `	/* Extract */` |
|        3 | 14475 | `	zEnd = &zImport[nLen];` |
|        5 | 14476 | `	while( zImport < zEnd ){` |
|        3 | 14477 | `		int c = zImport[0];` |
|        3 | 14478 | `		pSuper = 0;` |
|        3 | 14479 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14480 | `			/* Import $_GET variables */` |
|        3 | 14481 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14482 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14483 | `			/* Import $_POST variables */` |
|      ! 0 | 14484 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14485 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14486 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14487 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14488 | `		}` |
|        3 | 14489 | `		if( pSuper ){` |
|        - | 14490 | `			/* Iterate throw array entries */` |
|        3 | 14491 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14492 | `		}` |
|        - | 14493 | `		/* Advance the cursor */` |
|        3 | 14494 | `		zImport++;` |
|        1 | 14495 | `	}` |
|        - | 14496 | `	/* All done,return TRUE*/` |
|        3 | 14497 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14498 | `	return PH7_OK;` |
|        1 | 14499 |  |
|        - | 14500 | `/*` |
|        - | 14501 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14502 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14503 | ` * information.` |
|        - | 14504 | ` */` |
|    12702 | 14505 | `static sxi32 VmEvalChunk(` |
|        - | 14506 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14507 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14508 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14509 | `	int iFlags,         /* Compile flag */` |
|        - | 14510 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14511 | `	)` |
|        2 | 14512 |  |
|        - | 14513 | `	SySet *pByteCode,aByteCode;` |
|        - | 14514 | `	SyBlob sSavedNs;` |
|    12704 | 14515 | `	ProcConsumer xErr = 0;` |
|    12704 | 14516 | `	void *pErrData = 0;` |
|        - | 14517 | `	/* Initialize bytecode container */` |
|    12704 | 14518 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12704 | 14519 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14520 | `	/* Reset the code generator */` |
|    12704 | 14521 | `	if( bTrueReturn ){` |
|        - | 14522 | `		/* Included file,log compile-time errors */` |
|     9548 | 14523 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9548 | 14524 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4773 | 14525 | `	}` |
|    12704 | 14526 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14527 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14528 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14529 | `	 * the caller's namespace is restored. */` |
|    12704 | 14530 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12704 | 14531 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12704 | 14532 | `	if( bTrueReturn ){` |
|        - | 14533 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9548 | 14534 | `		SyBlobReset(&pVm->sNamespace);` |
|     4773 | 14535 | `	}` |
|        - | 14536 | `	/* Swap bytecode container */` |
|    12704 | 14537 | `	pByteCode = pVm->pByteContainer;` |
|    12704 | 14538 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14539 | `	/* Compile the chunk */` |
|    12704 | 14540 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19055 | 14541 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14542 | `		/* Compilation error,return false */` |
|        3 | 14543 | `		if( pCtx ){` |
|        3 | 14544 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14545 | `		}` |
|        2 | 14546 | `	}else{` |
|        - | 14547 | `		/* Mount any newly defined classes */` |
|        - | 14548 | `		SyHashEntry *pEntry;` |
|        - | 14549 | `		ph7_class *pClass;` |
|        - | 14550 | `		ph7_value sResult; /* Return value */` |
|        - | 14551 | `		sxi32 rc;` |
|    12702 | 14552 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   794892 | 14553 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   775842 | 14554 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14555 | `			/* Only mount classes that haven't been mounted yet */` |
|   775842 | 14556 | `			if( !pClass->bMounted ){` |
|   204686 | 14557 | `				rc = VmMountUserClass(pVm,pClass);` |
|   204686 | 14558 | `				if( rc != SXRET_OK ){` |
|        - | 14559 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14560 | `					if( pCtx ){` |
|      ! 0 | 14561 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14562 | `					}` |
|      ! 0 | 14563 | `					goto Cleanup;` |
|        - | 14564 | `				}` |
|   102342 | 14565 | `			}` |
|        2 | 14566 | `		}` |
|    12702 | 14567 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14568 | `			/* Out of memory */` |
|      ! 0 | 14569 | `			if( pCtx ){` |
|      ! 0 | 14570 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14571 | `			}` |
|      ! 0 | 14572 | `			goto Cleanup;` |
|        - | 14573 | `		}` |
|    12702 | 14574 | `		if( bTrueReturn ){` |
|        - | 14575 | `			/* Assume a boolean true return value */` |
|     9548 | 14576 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4775 | 14577 | `		}else{` |
|        - | 14578 | `			/* Assume a null return value */` |
|     3156 | 14579 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14580 | `		}` |
|        - | 14581 | `		/* Execute the compiled chunk */` |
|    12702 | 14582 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12702 | 14583 | `		if( pCtx ){` |
|        - | 14584 | `			/* Set the execution result */` |
|     9568 | 14585 | `			ph7_result_value(pCtx,&sResult);` |
|     4783 | 14586 | `		}` |
|    12702 | 14587 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14588 | `	}` |
|     6351 | 14589 | `Cleanup:` |
|        - | 14590 | `	/* Cleanup the mess left behind */` |
|    12704 | 14591 | `	pVm->pByteContainer = pByteCode;` |
|    12704 | 14592 | `	SySetRelease(&aByteCode);` |
|        - | 14593 | `	/* Restore caller's namespace state */` |
|    12704 | 14594 | `	SyBlobReset(&pVm->sNamespace);` |
|    12704 | 14595 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12704 | 14596 | `	SyBlobRelease(&sSavedNs);` |
|    12704 | 14597 | `	return SXRET_OK;` |
|        2 | 14598 |  |
|        - | 14599 | `/*` |
|        - | 14600 | ` * value eval(string $code)` |
|        - | 14601 | ` *   Evaluate a string as PHP code.` |
|        - | 14602 | ` * Parameter` |
|        - | 14603 | ` *  code: PHP code to evaluate.` |
|        - | 14604 | ` * Return` |
|        - | 14605 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14606 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14607 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14608 | ` */` |
|       24 | 14609 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14610 |  |
|        - | 14611 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 14612 | `	if( nArg < 1 ){` |
|        - | 14613 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14614 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14615 | `		return SXRET_OK;` |
|        - | 14616 | `	}` |
|        - | 14617 | `	/* Chunk to evaluate */` |
|       26 | 14618 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 14619 | `	if( sChunk.nByte < 1 ){` |
|        - | 14620 | `		/* Empty string,return NULL */` |
|        3 | 14621 | `		ph7_result_null(pCtx);` |
|        3 | 14622 | `		return SXRET_OK;` |
|        - | 14623 | `	}` |
|        - | 14624 | `	/* Eval the chunk */` |
|       24 | 14625 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 14626 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14627 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 14628 | `		return PH7_ABORT;` |
|        - | 14629 | `	}` |
|       22 | 14630 | `	return SXRET_OK;` |
|       14 | 14631 |  |
|        - | 14632 | `/*` |
|        - | 14633 | ` * Check if a file path is already included.` |
|        - | 14634 | ` */` |
|    19088 | 14635 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14636 |  |
|        - | 14637 | `	SyString *aEntries;` |
|        - | 14638 | `	sxu32 n;` |
|    19090 | 14639 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14640 | `	/* Perform a linear search */` |
| 90955328 | 14641 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 90936246 | 14642 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14643 | `			/* Already included */` |
|        7 | 14644 | `			return TRUE;` |
|        - | 14645 | `		}` |
| 45468121 | 14646 | `	}` |
|    19084 | 14647 | `	return FALSE;` |
|     9546 | 14648 |  |
|        - | 14649 | `/*` |
|        - | 14650 | ` * Push a file path in the appropriate VM container.` |
|        - | 14651 | ` */` |
|    22214 | 14652 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14653 |  |
|        - | 14654 | `	SyString sPath;` |
|        - | 14655 | `	char *zDup;` |
|        - | 14656 | `#ifdef __WINNT__` |
|        - | 14657 | `	char *zCur;` |
|        - | 14658 | `#endif` |
|        - | 14659 | `	sxi32 rc;` |
|    22216 | 14660 | `	if( nLen < 0 ){` |
|     3128 | 14661 | `		nLen = SyStrlen(zPath);` |
|     1563 | 14662 | `	}` |
|        - | 14663 | `	/* Duplicate the file path first */` |
|    22216 | 14664 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22216 | 14665 | `	if( zDup == 0 ){` |
|      ! 0 | 14666 | `		return SXERR_MEM;` |
|        - | 14667 | `	}` |
|        - | 14668 | `#ifdef __WINNT__` |
|        - | 14669 | `	/* Normalize path on windows` |
|        - | 14670 | `	 * Example:` |
|        - | 14671 | `	 *    Path/To/File.php` |
|        - | 14672 | `	 * becomes` |
|        - | 14673 | `	 *   path\to\file.php` |
|        - | 14674 | `	 */` |
|        2 | 14675 | `	zCur = zDup;` |
|        2 | 14676 | `	while( zCur[0] != 0 ){` |
|        2 | 14677 | `		if( zCur[0] == '/' ){` |
|        2 | 14678 | `			zCur[0] = '\\';` |
|        2 | 14679 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14680 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14681 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14682 | `		}` |
|        2 | 14683 | `		zCur++;` |
|        2 | 14684 | `	}` |
|        - | 14685 | `#endif` |
|        - | 14686 | `	/* Install the file path */` |
|    22216 | 14687 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22216 | 14688 | `	if( !bMain ){` |
|    19090 | 14689 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14690 | `			/* Already included */` |
|        7 | 14691 | `			*pNew = 0;` |
|        4 | 14692 | `		}else{` |
|        - | 14693 | `			/* Insert in the corresponding container */` |
|    19084 | 14694 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19084 | 14695 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14696 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14697 | `				return rc;` |
|        - | 14698 | `			}` |
|    19084 | 14699 | `			*pNew = 1;` |
|        - | 14700 | `		}` |
|     9544 | 14701 | `	}` |
|    22216 | 14702 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22216 | 14703 | `	return SXRET_OK;` |
|    11109 | 14704 |  |
|        - | 14705 | `/*` |
|        - | 14706 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14707 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14708 | ` * indicates failure.` |
|        - | 14709 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14710 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14711 | ` * operations.` |
|        - | 14712 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14713 | ` * this function is a no-op.` |
|        - | 14714 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14715 | ` * constructs for more information.` |
|        - | 14716 | ` */` |
|     9556 | 14717 | `static sxi32 VmExecIncludedFile(` |
|        - | 14718 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14719 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14720 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14721 | `	 )` |
|        2 | 14722 |  |
|        - | 14723 | `	sxi32 rc;` |
|        - | 14724 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14725 | `	const ph7_io_stream *pStream;` |
|        - | 14726 | `	SyBlob sContents;` |
|        - | 14727 | `	void *pHandle;` |
|        - | 14728 | `	ph7_vm *pVm;` |
|        - | 14729 | `	int isNew;` |
|        - | 14730 | `	/* Initialize fields */` |
|     9558 | 14731 | `	pVm = pCtx->pVm;` |
|     9558 | 14732 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9558 | 14733 | `	isNew = 0;` |
|        - | 14734 | `	/* Extract the associated stream */` |
|     9558 | 14735 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14736 | `	/*` |
|        - | 14737 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14738 | `	 * in a read-only mode.` |
|        - | 14739 | `	 */` |
|     9558 | 14740 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9558 | 14741 | `	if( pHandle == 0 ){` |
|        8 | 14742 | `		return SXERR_IO;` |
|        - | 14743 | `	}` |
|     9552 | 14744 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9552 | 14745 | `	if( IncludeOnce && !isNew ){` |
|        - | 14746 | `		/* Already included */` |
|        5 | 14747 | `		rc = SXERR_EXISTS;` |
|        3 | 14748 | `	}else{` |
|        - | 14749 | `		/* Read the whole file contents */` |
|     9548 | 14750 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9548 | 14751 | `		if( rc == SXRET_OK ){` |
|        - | 14752 | `			SyString sScript;` |
|        - | 14753 | `			/* Compile and execute the script */` |
|     9548 | 14754 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9548 | 14755 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4773 | 14756 | `		}` |
|        - | 14757 | `	}` |
|        - | 14758 | `	/* Pop from the set of included file */` |
|     9552 | 14759 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14760 | `	/* Close the handle */` |
|     9552 | 14761 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14762 | `	/* Release the working buffer */` |
|     9552 | 14763 | `	SyBlobRelease(&sContents);` |
|        - | 14764 | `#else` |
|        - | 14765 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14766 | `	SXUNUSED(pPath);` |
|        - | 14767 | `	SXUNUSED(IncludeOnce);` |
|        - | 14768 | `	rc = SXERR_IO;` |
|        - | 14769 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9552 | 14770 | `	return rc;` |
|     4780 | 14771 |  |
|        - | 14772 | `/*` |
|        - | 14773 | ` * string get_include_path(void)` |
|        - | 14774 | ` *  Gets the current include_path configuration option.` |
|        - | 14775 | ` * Parameter` |
|        - | 14776 | ` *  None` |
|        - | 14777 | ` * Return` |
|        - | 14778 | ` *  Included paths as a string` |
|        - | 14779 | ` */` |
|        2 | 14780 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14781 |  |
|        3 | 14782 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14783 | `	SyString *aEntry;` |
|        - | 14784 | `	int dir_sep;` |
|        - | 14785 | `	sxu32 n;` |
|        - | 14786 | `#ifdef __WINNT__` |
|        1 | 14787 | `	dir_sep = ';';` |
|        - | 14788 | `#else` |
|        - | 14789 | `	/* Assume UNIX path separator */` |
|        2 | 14790 | `	dir_sep = ':';` |
|        - | 14791 | `#endif` |
|        1 | 14792 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14793 | `	SXUNUSED(apArg);` |
|        - | 14794 | `	/* Point to the list of import paths */` |
|        3 | 14795 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14796 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14797 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14798 | `		if( n > 0 ){` |
|        - | 14799 | `			/* Append dir seprator */` |
|      ! 0 | 14800 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14801 | `		}` |
|        - | 14802 | `		/* Append path */` |
|        3 | 14803 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14804 | `	}` |
|        3 | 14805 | `	return PH7_OK;` |
|        1 | 14806 |  |
|        - | 14807 | `/*` |
|        - | 14808 | ` * string get_get_included_files(void)` |
|        - | 14809 | ` *  Gets the current include_path configuration option.` |
|        - | 14810 | ` * Parameter` |
|        - | 14811 | ` *  None` |
|        - | 14812 | ` * Return` |
|        - | 14813 | ` *  Included paths as a string` |
|        - | 14814 | ` */` |
|        2 | 14815 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14816 |  |
|        3 | 14817 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14818 | `	ph7_value *pArray,*pWorker;` |
|        - | 14819 | `	SyString *pEntry;` |
|        - | 14820 | `	int c,d;` |
|        - | 14821 | `	/* Create an array and a working value */` |
|        3 | 14822 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14823 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14824 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14825 | `		/* Out of memory,return null */` |
|      ! 0 | 14826 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14827 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14828 | `		SXUNUSED(apArg);` |
|      ! 0 | 14829 | `		return PH7_OK;` |
|        - | 14830 | `	}` |
|        3 | 14831 | `	c = d = '/';` |
|        - | 14832 | `#ifdef __WINNT__` |
|        1 | 14833 | `	d = '\\';` |
|        - | 14834 | `#endif` |
|        - | 14835 | `	/* Iterate throw entries */` |
|        3 | 14836 | `	SySetResetCursor(pFiles);` |
|     3917 | 14837 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14838 | `		const char *zBase,*zEnd;` |
|        - | 14839 | `		int iLen;` |
|        - | 14840 | `		/* reset the string cursor */` |
|     3915 | 14841 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14842 | `		/* Extract base name */` |
|     3915 | 14843 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14844 | `		/* Ignore trailing '/' */` |
|     5872 | 14845 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14846 | `			zEnd--;` |
|      ! 0 | 14847 | `		}` |
|     3915 | 14848 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 14849 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 14850 | `			zEnd--;` |
|        1 | 14851 | `		}` |
|     3915 | 14852 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 14853 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14854 | `		/* Copy entry name */` |
|     3915 | 14855 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14856 | `		/* Perform the insertion */` |
|     3915 | 14857 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14858 | `	}` |
|        - | 14859 | `	/* All done,return the created array */` |
|        3 | 14860 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14861 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14862 | `	 * by the engine as soon we return from this foreign` |
|        - | 14863 | `	 * function.` |
|        - | 14864 | `	 */` |
|        3 | 14865 | `	return PH7_OK;` |
|        2 | 14866 |  |
|        - | 14867 | `/*` |
|        - | 14868 | ` * include:` |
|        - | 14869 | ` * According to the PHP reference manual.` |
|        - | 14870 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14871 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14872 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14873 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14874 | ` *  and the current working directory before failing. The include()` |
|        - | 14875 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14876 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14877 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14878 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14879 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14880 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14881 | ` *  directory to find the requested file.` |
|        - | 14882 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14883 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14884 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14885 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14886 | ` */` |
|     9538 | 14887 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14888 |  |
|        - | 14889 | `	SyString sFile;` |
|        - | 14890 | `	sxi32 rc;` |
|     9540 | 14891 | `	if( nArg < 1 ){` |
|        - | 14892 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14893 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14894 | `		return SXRET_OK;` |
|        - | 14895 | `	}` |
|        - | 14896 | `	/* File to include */` |
|     9540 | 14897 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9540 | 14898 | `	if( sFile.nByte < 1 ){` |
|        - | 14899 | `		/* Empty string,return NULL */` |
|      ! 0 | 14900 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14901 | `		return SXRET_OK;` |
|        - | 14902 | `	}` |
|        - | 14903 | `	/* Open,compile and execute the desired script */` |
|     9540 | 14904 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9540 | 14905 | `	if( rc != SXRET_OK ){` |
|        - | 14906 | `		/* Emit a warning and return false */` |
|        3 | 14907 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14908 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14909 | `	}` |
|     9540 | 14910 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14911 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 14912 | `		return PH7_ABORT;` |
|        - | 14913 | `	}` |
|     9536 | 14914 | `	return SXRET_OK;` |
|     4771 | 14915 |  |
|        - | 14916 | `/*` |
|        - | 14917 | ` * include_once:` |
|        - | 14918 | ` *  According to the PHP reference manual.` |
|        - | 14919 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14920 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14921 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14922 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14923 | ` *   just once.` |
|        - | 14924 | ` */` |
|        4 | 14925 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14926 |  |
|        - | 14927 | `	SyString sFile;` |
|        - | 14928 | `	sxi32 rc;` |
|        5 | 14929 | `	if( nArg < 1 ){` |
|        - | 14930 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14931 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14932 | `		return SXRET_OK;` |
|        - | 14933 | `	}` |
|        - | 14934 | `	/* File to include */` |
|        5 | 14935 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14936 | `	if( sFile.nByte < 1 ){` |
|        - | 14937 | `		/* Empty string,return NULL */` |
|      ! 0 | 14938 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14939 | `		return SXRET_OK;` |
|        - | 14940 | `	}` |
|        - | 14941 | `	/* Open,compile and execute the desired script */` |
|        5 | 14942 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14943 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14944 | `		/* File already included,return TRUE */` |
|        3 | 14945 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14946 | `		return SXRET_OK;` |
|        - | 14947 | `	}` |
|        3 | 14948 | `	if( rc != SXRET_OK ){` |
|        - | 14949 | `		/* Emit a warning and return false */` |
|      ! 0 | 14950 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14951 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14952 | ` 	}` |
|        3 | 14953 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14954 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 14955 | `		return PH7_ABORT;` |
|        - | 14956 | `	}` |
|        3 | 14957 | `	return SXRET_OK;` |
|        3 | 14958 |  |
|        - | 14959 | `/*` |
|        - | 14960 | ` * require.` |
|        - | 14961 | ` *  According to the PHP reference manual.` |
|        - | 14962 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14963 | ` *   also produce a fatal level error.` |
|        - | 14964 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14965 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14966 | ` */` |
|        6 | 14967 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14968 |  |
|        - | 14969 | `	SyString sFile;` |
|        - | 14970 | `	sxi32 rc;` |
|        8 | 14971 | `	if( nArg < 1 ){` |
|        - | 14972 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14973 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14974 | `		return SXRET_OK;` |
|        - | 14975 | `	}` |
|        - | 14976 | `	/* File to include */` |
|        8 | 14977 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14978 | `	if( sFile.nByte < 1 ){` |
|        - | 14979 | `		/* Empty string,return NULL */` |
|      ! 0 | 14980 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14981 | `		return SXRET_OK;` |
|        - | 14982 | `	}` |
|        - | 14983 | `	/* Open,compile and execute the desired script */` |
|        8 | 14984 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14985 | `	if( rc != SXRET_OK ){` |
|        - | 14986 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14987 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14988 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14989 | `		return PH7_ABORT;` |
|        - | 14990 | `	}` |
|        8 | 14991 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14992 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 14993 | `		return PH7_ABORT;` |
|        - | 14994 | `	}` |
|        8 | 14995 | `	return SXRET_OK;` |
|        5 | 14996 |  |
|        - | 14997 | `/*` |
|        - | 14998 | ` * require_once:` |
|        - | 14999 | ` *  According to the PHP reference manual.` |
|        - | 15000 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15001 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15002 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15003 | ` *   and how it differs from its non _once siblings.` |
|        - | 15004 | ` */` |
|        4 | 15005 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15006 |  |
|        - | 15007 | `	SyString sFile;` |
|        - | 15008 | `	sxi32 rc;` |
|        5 | 15009 | `	if( nArg < 1 ){` |
|        - | 15010 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15011 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15012 | `		return SXRET_OK;` |
|        - | 15013 | `	}` |
|        - | 15014 | `	/* File to include */` |
|        5 | 15015 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15016 | `	if( sFile.nByte < 1 ){` |
|        - | 15017 | `		/* Empty string,return NULL */` |
|      ! 0 | 15018 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15019 | `		return SXRET_OK;` |
|        - | 15020 | `	}` |
|        - | 15021 | `	/* Open,compile and execute the desired script */` |
|        5 | 15022 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15023 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15024 | `		/* File already included,return TRUE */` |
|        3 | 15025 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15026 | `		return SXRET_OK;` |
|        - | 15027 | `	}` |
|        3 | 15028 | `	if( rc != SXRET_OK ){` |
|        - | 15029 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15030 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15031 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15032 | `		return PH7_ABORT;` |
|        - | 15033 | `	}` |
|        3 | 15034 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15035 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15036 | `		return PH7_ABORT;` |
|        - | 15037 | `	}` |
|        3 | 15038 | `	return SXRET_OK;` |
|        3 | 15039 |  |
|        - | 15040 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15041 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15042 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15043 | `/*` |
|        - | 15044 | ` * Section:` |
|        - | 15045 | ` *  SPL Autoloading functions.` |
|        - | 15046 | ` * Status:` |
|        - | 15047 | ` *  Stable.` |
|        - | 15048 | ` */` |
|        - | 15049 | `/*` |
|        - | 15050 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15051 | ` *  Register given function as __autoload() implementation.` |
|        - | 15052 | ` * Parameters` |
|        - | 15053 | ` *  callback` |
|        - | 15054 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15055 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15056 | ` *  throw` |
|        - | 15057 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15058 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15059 | ` *  prepend` |
|        - | 15060 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15061 | ` *   autoload stack instead of appending it.` |
|        - | 15062 | ` * Return` |
|        - | 15063 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15064 | ` */` |
|       34 | 15065 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15066 |  |
|        - | 15067 | `	VmAutoloadCB sEntry;` |
|       36 | 15068 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15069 | `	int iPrepend = 0;` |
|        - | 15070 | `	sxu32 n;` |
|       36 | 15071 | `	if( nArg < 1 ){` |
|        - | 15072 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15073 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15074 | `		/* Check for duplicates first */` |
|        9 | 15075 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15076 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15077 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15078 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15079 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15080 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15081 | `				return SXRET_OK;` |
|        - | 15082 | `			}` |
|      ! 0 | 15083 | `		}` |
|        5 | 15084 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15085 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15086 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15087 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15088 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15089 | `		return SXRET_OK;` |
|        - | 15090 | `	}` |
|        - | 15091 | `	/* Validate that the callback is callable */` |
|       28 | 15092 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15093 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15094 | `		if( nArg >= 2 ){` |
|      ! 0 | 15095 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15096 | `		}` |
|      ! 0 | 15097 | `		if( iThrow ){` |
|      ! 0 | 15098 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15099 | `				"Argument is not callable");` |
|      ! 0 | 15100 | `		}` |
|      ! 0 | 15101 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15102 | `		return SXRET_OK;` |
|        - | 15103 | `	}` |
|        - | 15104 | `	/* Check for duplicates */` |
|       46 | 15105 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15106 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15107 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15108 | `			/* Already registered */` |
|      ! 0 | 15109 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15110 | `			return SXRET_OK;` |
|        - | 15111 | `		}` |
|       11 | 15112 | `	}` |
|        - | 15113 | `	/* Check prepend flag */` |
|       28 | 15114 | `	if( nArg >= 3 ){` |
|        3 | 15115 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15116 | `	}` |
|        - | 15117 | `	/* Store the callback */` |
|       28 | 15118 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15119 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15120 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15121 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15122 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15123 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15124 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15125 | `		VmAutoloadCB *aBase;` |
|        3 | 15126 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15127 | `		/* Rotate: move last entry to front */` |
|        3 | 15128 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15129 | `		if( aBase ){` |
|        - | 15130 | `			VmAutoloadCB sTemp;` |
|        - | 15131 | `			sxu32 i;` |
|        3 | 15132 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15133 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15134 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15135 | `			}` |
|        3 | 15136 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15137 | `		}` |
|        2 | 15138 | `	}else{` |
|       26 | 15139 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15140 | `	}` |
|       28 | 15141 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15142 | `	return SXRET_OK;` |
|       19 | 15143 |  |
|        - | 15144 | `/*` |
|        - | 15145 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15146 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15147 | ` * Parameters` |
|        - | 15148 | ` *  callback` |
|        - | 15149 | ` *   The autoload function being unregistered.` |
|        - | 15150 | ` * Return` |
|        - | 15151 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15152 | ` */` |
|       32 | 15153 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15154 |  |
|       34 | 15155 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15156 | `	sxu32 n,nEntry;` |
|       34 | 15157 | `	if( nArg < 1 ){` |
|      ! 0 | 15158 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15159 | `		return SXRET_OK;` |
|        - | 15160 | `	}` |
|       34 | 15161 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15162 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15163 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15164 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15165 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15166 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15167 | `			sxu32 i;` |
|       32 | 15168 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15169 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15170 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15171 | `			}` |
|        - | 15172 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15173 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15174 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15175 | `			return SXRET_OK;` |
|        - | 15176 | `		}` |
|        3 | 15177 | `	}` |
|        3 | 15178 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15179 | `	return SXRET_OK;` |
|       18 | 15180 |  |
|        - | 15181 | `/*` |
|        - | 15182 | ` * array spl_autoload_functions(void)` |
|        - | 15183 | ` *  Return all registered __autoload() functions.` |
|        - | 15184 | ` * Return` |
|        - | 15185 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15186 | ` *  an empty array is returned.` |
|        - | 15187 | ` */` |
|       20 | 15188 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15189 |  |
|       21 | 15190 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15191 | `	ph7_value *pArray;` |
|        - | 15192 | `	sxu32 n,nEntry;` |
|       10 | 15193 | `	SXUNUSED(nArg);` |
|       10 | 15194 | `	SXUNUSED(apArg);` |
|       21 | 15195 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15196 | `	if( pArray == 0 ){` |
|      ! 0 | 15197 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15198 | `		return SXRET_OK;` |
|        - | 15199 | `	}` |
|       21 | 15200 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15201 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15202 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15203 | `		if( pEntry ){` |
|       15 | 15204 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15205 | `		}` |
|        8 | 15206 | `	}` |
|       21 | 15207 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15208 | `	return SXRET_OK;` |
|       11 | 15209 |  |
|        - | 15210 | `/*` |
|        - | 15211 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15212 | ` *  Default implementation of __autoload().` |
|        - | 15213 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15214 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15215 | ` * Parameters` |
|        - | 15216 | ` *  class` |
|        - | 15217 | ` *   The class name being searched.` |
|        - | 15218 | ` *  file_extensions` |
|        - | 15219 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15220 | ` */` |
|        2 | 15221 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15222 |  |
|        - | 15223 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15224 | `	SyBlob sPath;` |
|        - | 15225 | `	int nClass;` |
|        - | 15226 | `	sxi32 rc;` |
|        3 | 15227 | `	if( nArg < 1 ){` |
|      ! 0 | 15228 | `		return SXRET_OK;` |
|        - | 15229 | `	}` |
|        3 | 15230 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15231 | `	if( nClass < 1 ){` |
|      ! 0 | 15232 | `		return SXRET_OK;` |
|        - | 15233 | `	}` |
|        - | 15234 | `	/* Default extensions */` |
|        3 | 15235 | `	zExt = ".php,.inc";` |
|        3 | 15236 | `	if( nArg >= 2 ){` |
|        - | 15237 | `		int nExt;` |
|      ! 0 | 15238 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15239 | `		if( nExt < 1 ){` |
|      ! 0 | 15240 | `			zExt = ".php,.inc";` |
|      ! 0 | 15241 | `		}` |
|      ! 0 | 15242 | `	}` |
|        3 | 15243 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15244 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15245 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15246 | `	zCur = zExt;` |
|        7 | 15247 | `	while( zCur < zEnd ){` |
|        - | 15248 | `		const char *zComma;` |
|        - | 15249 | `		SyString sFile;` |
|        - | 15250 | `		int i;` |
|        - | 15251 | `		/* Find next comma or end */` |
|        5 | 15252 | `		zComma = zCur;` |
|       21 | 15253 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15254 | `			zComma++;` |
|        1 | 15255 | `		}` |
|        - | 15256 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15257 | `		SyBlobReset(&sPath);` |
|       69 | 15258 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15259 | `			char c = zClass[i];` |
|       65 | 15260 | `			if( c == '\\' ){` |
|      ! 0 | 15261 | `				c = '/';` |
|       65 | 15262 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15263 | `				c = c + ('a' - 'A');` |
|        6 | 15264 | `			}` |
|       65 | 15265 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15266 | `		}` |
|        - | 15267 | `		/* Append extension */` |
|        5 | 15268 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15269 | `		/* Try to include the file */` |
|        5 | 15270 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15271 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15272 | `		if( rc == SXRET_OK ){` |
|        - | 15273 | `			/* File included successfully */` |
|      ! 0 | 15274 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15275 | `			return SXRET_OK;` |
|        - | 15276 | `		}` |
|        - | 15277 | `		/* Move past the comma */` |
|        5 | 15278 | `		zCur = zComma;` |
|        5 | 15279 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15280 | `			zCur++;` |
|        1 | 15281 | `		}` |
|        1 | 15282 | `	}` |
|        3 | 15283 | `	SyBlobRelease(&sPath);` |
|        3 | 15284 | `	return SXRET_OK;` |
|        2 | 15285 |  |
|        - | 15286 | `/* Table of built-in VM functions. */` |
|        - | 15287 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15288 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15289 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15290 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15291 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15292 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15293 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15294 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15295 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15296 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15297 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15298 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15299 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15300 | `	    /* Constants management */` |
|        - | 15301 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15302 | `	{ "define",   vm_builtin_define               },` |
|        - | 15303 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15304 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15305 | `	   /* Class/Object functions */` |
|        - | 15306 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15307 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15308 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15309 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15310 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15311 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15312 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15313 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15314 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15315 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15316 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15317 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15318 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15319 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15320 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15321 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15322 | `	   /* SPL Autoloading */` |
|        - | 15323 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15324 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15325 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15326 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15327 | `	   /* Random numbers/strings generators */` |
|        - | 15328 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15329 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15330 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15331 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15332 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15333 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15334 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15335 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15336 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15337 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15338 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15339 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15340 | `	   /* Language constructs functions */` |
|        - | 15341 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15342 | `	{ "print", vm_builtin_print                   },` |
|        - | 15343 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15344 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15345 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15346 | `	  /* Variable handling functions */` |
|        - | 15347 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15348 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15349 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15350 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15351 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15352 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15353 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15354 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15355 | `	  /* Ouput control functions */` |
|        - | 15356 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15357 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15358 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15359 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15360 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15361 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15362 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15363 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15364 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15365 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15366 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15367 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15368 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15369 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15370 | `	  /* Assertion functions */` |
|        - | 15371 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15372 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15373 | `	  /* Error reporting functions */` |
|        - | 15374 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15375 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15376 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15377 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15378 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15379 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15380 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15381 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15382 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15383 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15384 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15385 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15386 | `	  /* Release info */` |
|        - | 15387 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15388 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15389 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15390 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15391 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15392 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15393 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15394 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15395 | `	  /* hashmap */` |
|        - | 15396 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15397 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15398 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15399 | `	  /* URL related function */` |
|        - | 15400 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15401 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15402 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15403 | `	   /* XML processing functions */` |
|        - | 15404 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15405 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15406 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15407 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15408 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15409 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15410 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15411 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15412 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15413 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15414 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15415 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15416 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15417 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15418 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15419 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15420 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15421 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15422 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15423 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15424 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15425 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15426 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15427 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15428 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15429 | `	   /* Command line processing */` |
|        - | 15430 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15431 | `	   /* JSON encoding/decoding */` |
|        - | 15432 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15433 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15434 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15435 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15436 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15437 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15438 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15439 | `	   /* Files/URI inclusion facility */` |
|        - | 15440 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15441 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15442 | `	{ "include",      vm_builtin_include          },` |
|        - | 15443 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15444 | `	{ "require",      vm_builtin_require          },` |
|        - | 15445 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15446 | `};` |
|        - | 15447 | `/*` |
|        - | 15448 | ` * Register the built-in VM functions defined above.` |
|        - | 15449 | ` */` |
|     2820 | 15450 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15451 |  |
|        - | 15452 | `	sxi32 rc;` |
|        - | 15453 | `	sxu32 n;` |
|   380702 | 15454 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15455 | `		/* Note that these special functions have access` |
|        - | 15456 | `		 * to the underlying virtual machine as their` |
|        - | 15457 | `		 * private data.` |
|        - | 15458 | `		 */` |
|   377882 | 15459 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   377882 | 15460 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15461 | `			return rc;` |
|        - | 15462 | `		}` |
|   188942 | 15463 | `	}` |
|     2822 | 15464 | `	return SXRET_OK;` |
|     1412 | 15465 |  |
|        - | 15466 | `/*` |
|        - | 15467 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15468 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15469 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15470 | ` */` |
|   100480 | 15471 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15472 |  |
|   100482 | 15473 | `	if( !iLoadable ){` |
|    98426 | 15474 | `		return pClass;` |
|        - | 15475 | `	}` |
|     2062 | 15476 | `	while(pClass){` |
|     2058 | 15477 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2054 | 15478 | `			return pClass;` |
|        - | 15479 | `		}` |
|        5 | 15480 | `		pClass = pClass->pNextName;` |
|        1 | 15481 | `	}` |
|        5 | 15482 | `	return 0;` |
|    50242 | 15483 |  |
|        - | 15484 | `/*` |
|        - | 15485 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15486 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15487 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15488 | ` * registered in the VM's class table.` |
|        - | 15489 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15490 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15491 | ` */` |
|       38 | 15492 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15493 |  |
|        - | 15494 | `	VmAutoloadCB *pEntry;` |
|        - | 15495 | `	ph7_value sArg,sResult;` |
|        - | 15496 | `	SyHashEntry *pHashEntry;` |
|        - | 15497 | `	ph7_class *pClass;` |
|        - | 15498 | `	sxu32 n,nEntry;` |
|       40 | 15499 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15500 | `	if( nEntry < 1 ){` |
|       26 | 15501 | `		return 0;` |
|        - | 15502 | `	}` |
|        - | 15503 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15504 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15505 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15506 | `	}` |
|        - | 15507 | `	/* Mark this class as being autoloaded */` |
|       14 | 15508 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15509 | `	/* Prepare the class name argument */` |
|       14 | 15510 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15511 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15512 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15513 | `	pClass = 0;` |
|       28 | 15514 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15515 | `		ph7_value *apArg[1];` |
|       24 | 15516 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15517 | `		if( pEntry == 0 ){` |
|      ! 0 | 15518 | `			continue;` |
|        - | 15519 | `		}` |
|       24 | 15520 | `		apArg[0] = &sArg;` |
|       24 | 15521 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15522 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15523 | `			continue;` |
|        - | 15524 | `		}` |
|        - | 15525 | `		/* Check if the class is now available */` |
|       24 | 15526 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15527 | `		if( pHashEntry ){` |
|       10 | 15528 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15529 | `			if( pClass ){` |
|       10 | 15530 | `				break;` |
|        - | 15531 | `			}` |
|      ! 0 | 15532 | `		}` |
|        9 | 15533 | `	}` |
|       14 | 15534 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15535 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15536 | `	/* Remove reentrancy guard */` |
|       14 | 15537 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15538 | `	return pClass;` |
|       21 | 15539 |  |
|        - | 15540 | `/*` |
|        - | 15541 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15542 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15543 | ` */` |
|       18 | 15544 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15545 |  |
|       20 | 15546 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15547 |  |
|        - | 15548 | `/*` |
|        - | 15549 | ` * Check if the given name refer to an installed class.` |
|        - | 15550 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15551 | ` */` |
|   100492 | 15552 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15553 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15554 | `	const char *zName,  /* Name of the target class */` |
|        - | 15555 | `	sxu32 nByte,        /* zName length */` |
|        - | 15556 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15557 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15558 | `						 */` |
|        - | 15559 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15560 | `	)` |
|        2 | 15561 |  |
|        - | 15562 | `	SyHashEntry *pEntry;` |
|        - | 15563 | `	ph7_class *pClass;` |
|    50246 | 15564 | `	SXUNUSED(iNest);` |
|        - | 15565 | `	/* Exact class lookup.` |
|        - | 15566 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15567 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   100494 | 15568 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   100494 | 15569 | `	if( pEntry == 0 ){` |
|        - | 15570 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15571 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15572 | `	}` |
|   100474 | 15573 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   100474 | 15574 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    50248 | 15575 |  |
|        - | 15576 | `/*` |
|        - | 15577 | ` * Reference Table Implementation` |
|        - | 15578 | ` * Status: stable <chm@symisc.net>` |
|        - | 15579 | ` * Intro` |
|        - | 15580 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15581 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15582 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15583 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15584 | ` *  Refer to the official for more information on this powerful` |
|        - | 15585 | ` *  extension.` |
|        - | 15586 | ` */` |
|        - | 15587 | `/*` |
|        - | 15588 | ` * Allocate a new reference entry.` |
|        - | 15589 | ` */` |
|  3200850 | 15590 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15591 |  |
|        - | 15592 | `	VmRefObj *pRef;` |
|        - | 15593 | `	/* Allocate a new instance */` |
|  3200852 | 15594 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3200852 | 15595 | `	if( pRef == 0 ){` |
|      ! 0 | 15596 | `		return 0;` |
|        - | 15597 | `	}` |
|        - | 15598 | `	/* Zero the structure */` |
|  3200852 | 15599 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15600 | `	/* Initialize fields */` |
|  3200852 | 15601 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3200852 | 15602 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3200852 | 15603 | `	pRef->nIdx = nIdx;` |
|  3200852 | 15604 | `	return pRef;` |
|  1600427 | 15605 |  |
|        - | 15606 | `/*` |
|        - | 15607 | ` * Default hash function used by the reference table` |
|        - | 15608 | ` * for lookup/insertion operations.` |
|        - | 15609 | ` */` |
| 17533145 | 15610 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15611 |  |
|        - | 15612 | `	/* Calculate the hash based on the memory object index */` |
| 17533147 | 15613 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15614 |  |
|        - | 15615 | `/*` |
|        - | 15616 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15617 | ` * in the reference table.` |
|        - | 15618 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15619 | ` * otherwise.` |
|        - | 15620 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15621 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15622 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15623 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15624 | ` * Refer to the official for more information on this powerful` |
|        - | 15625 | ` * extension.` |
|        - | 15626 | ` */` |
|  9543214 | 15627 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15628 |  |
|        - | 15629 | `	VmRefObj *pRef;` |
|        - | 15630 | `	sxu32 nBucket;` |
|        - | 15631 | `	/* Point to the appropriate bucket */` |
|  9543216 | 15632 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15633 | `	/* Perform the lookup */` |
|  9543216 | 15634 | `	pRef = pVm->apRefObj[nBucket];` |
| 20989991 | 15635 | `	for(;;){` |
| 41962861 | 15636 | `		if( pRef == 0 ){` |
|  3305918 | 15637 | `			break;` |
|        - | 15638 | `		}` |
| 38656945 | 15639 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15640 | `			/* Entry found */` |
|  6237300 | 15641 | `			return pRef;` |
|        - | 15642 | `		}` |
|        - | 15643 | `		/* Point to the next entry */` |
| 32419647 | 15644 | `		pRef = pRef->pNextCollide;` |
|        2 | 15645 | `	}` |
|        - | 15646 | `	/* No such entry,return NULL */` |
|  3305918 | 15647 | `	return 0;` |
|  4771609 | 15648 |  |
|        - | 15649 | `/*` |
|        - | 15650 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15651 | ` *` |
|        - | 15652 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15653 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15654 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15655 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15656 | ` * Refer to the official for more information on this powerful` |
|        - | 15657 | ` * extension.` |
|        - | 15658 | ` */` |
|  3200850 | 15659 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15660 |  |
|        - | 15661 | `	sxu32 nBucket;` |
|  3200852 | 15662 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15663 | `		VmRefObj **apNew;` |
|        - | 15664 | `		sxu32 nNew;` |
|        - | 15665 | `		/* Allocate a larger table */` |
|     4472 | 15666 | `		nNew = pVm->nRefSize << 1;` |
|     4472 | 15667 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4472 | 15668 | `		if( apNew ){` |
|     4472 | 15669 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15670 | `			sxu32 n;` |
|        - | 15671 | `			/* Zero the structure */` |
|     4472 | 15672 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15673 | `			/* Rehash all referenced entries */` |
|  2847974 | 15674 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15675 | `				/* Remove old collision links */` |
|  2843504 | 15676 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15677 | `				/* Point to the appropriate bucket */` |
|  2843504 | 15678 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15679 | `				/* Insert the entry  */` |
|  2843504 | 15680 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843504 | 15681 | `				if( apNew[nBucket] ){` |
|  2301116 | 15682 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15683 | `				}` |
|  2843504 | 15684 | `				apNew[nBucket] = pEntry;` |
|        - | 15685 | `				/* Point to the next entry */` |
|  2843504 | 15686 | `				pEntry = pEntry->pNext;` |
|  1421753 | 15687 | `			}` |
|        - | 15688 | `			/* Release the old table */` |
|     4472 | 15689 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15690 | `			/* Install the new one */` |
|     4472 | 15691 | `			pVm->apRefObj = apNew;` |
|     4472 | 15692 | `			pVm->nRefSize = nNew;` |
|     2235 | 15693 | `		}` |
|     2235 | 15694 | `	}` |
|        - | 15695 | `	/* Point to the appropriate bucket */` |
|  3200852 | 15696 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15697 | `	/* Insert the entry */` |
|  3200852 | 15698 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3200852 | 15699 | `	if( pVm->apRefObj[nBucket] ){` |
|  2614283 | 15700 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307149 | 15701 | `	}` |
|  3200852 | 15702 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3200852 | 15703 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3200852 | 15704 | `	pVm->nRefUsed++;` |
|  3200852 | 15705 | `	return SXRET_OK;` |
|        2 | 15706 |  |
|        - | 15707 | `/*` |
|        - | 15708 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15709 | ` * the reference table.` |
|        - | 15710 | ` * This function is invoked when the user perform an unset` |
|        - | 15711 | ` * call [i.e: unset($var); ].` |
|        - | 15712 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15713 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15714 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15715 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15716 | ` * Refer to the official for more information on this powerful` |
|        - | 15717 | ` * extension.` |
|        - | 15718 | ` */` |
|  3159706 | 15719 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15720 |  |
|        - | 15721 | `	ph7_hashmap_node **apNode;` |
|        - | 15722 | `	SyHashEntry **apEntry;` |
|        - | 15723 | `	sxu32 n;` |
|        - | 15724 | `	/* Point to the reference table */` |
|  3159708 | 15725 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3159708 | 15726 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15727 | `	/* Unlink the entry from the reference table */` |
|  3270638 | 15728 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   110932 | 15729 | `		if( apEntry[n] ){` |
|   110882 | 15730 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55440 | 15731 | `		}` |
|    55467 | 15732 | `	}` |
|  6208620 | 15733 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3048914 | 15734 | `		if( apNode[n] ){` |
|     6812 | 15735 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3405 | 15736 | `		}` |
|  1524458 | 15737 | `	}` |
|  3159708 | 15738 | `	if( pRef->pPrevCollide ){` |
|  1214129 | 15739 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   607705 | 15740 | `	}else{` |
|  1945581 | 15741 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15742 | `	}` |
|  3159708 | 15743 | `	if( pRef->pNextCollide ){` |
|  1801362 | 15744 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   900677 | 15745 | `	}` |
|  3159708 | 15746 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15747 | `	/* Release the node */` |
|  3159708 | 15748 | `	SySetRelease(&pRef->aReference);` |
|  3159708 | 15749 | `	SySetRelease(&pRef->aArrEntries);` |
|  3159708 | 15750 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3159708 | 15751 | `	pVm->nRefUsed--;` |
|  3159708 | 15752 | `	return SXRET_OK;` |
|        2 | 15753 |  |
|        - | 15754 | `/*` |
|        - | 15755 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15756 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15757 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15758 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15759 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15760 | ` * Refer to the official for more information on this powerful` |
|        - | 15761 | ` * extension.` |
|        - | 15762 | ` */` |
|  3236262 | 15763 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15764 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15765 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15766 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15767 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15768 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15769 | `	)` |
|        2 | 15770 |  |
|  3236264 | 15771 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15772 | `	VmRefObj *pRef;` |
|        - | 15773 | `	/* Check if the referenced object already exists */` |
|  3236264 | 15774 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3236264 | 15775 | `	if( pRef == 0 ){` |
|        - | 15776 | `		/* Create a new entry */` |
|  3200852 | 15777 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3200852 | 15778 | `		if( pRef == 0 ){` |
|      ! 0 | 15779 | `			return SXERR_MEM;` |
|        - | 15780 | `		}` |
|  3200852 | 15781 | `		pRef->iFlags = iFlags;` |
|        - | 15782 | `		/* Install the entry */` |
|  3200852 | 15783 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1600425 | 15784 | `	}` |
|  3236264 | 15785 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3236264 | 15786 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15787 | `		VmSlot sRef;` |
|        - | 15788 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15789 | `		 * be deleted when we leave this frame.` |
|        - | 15790 | `		 */` |
|   105176 | 15791 | `		sRef.nIdx = nIdx;` |
|   105176 | 15792 | `		sRef.pUserData = pEntry;` |
|   105176 | 15793 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15794 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15795 | `		}` |
|    52587 | 15796 | `	}` |
|  3236264 | 15797 | `	if( pEntry ){` |
|        - | 15798 | `		/* Address of the hash-entry */` |
|   140364 | 15799 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70181 | 15800 | `	}` |
|  3236264 | 15801 | `	if( pMapEntry ){` |
|        - | 15802 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3087656 | 15803 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1543827 | 15804 | `	}` |
|  3236264 | 15805 | `	return SXRET_OK;` |
|  1618133 | 15806 |  |
|        - | 15807 | `/*` |
|        - | 15808 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15809 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15810 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15811 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15812 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15813 | ` * Refer to the official for more information on this powerful` |
|        - | 15814 | ` * extension.` |
|        - | 15815 | ` */` |
|  3147240 | 15816 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15817 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15818 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15819 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15820 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15821 | `	)` |
|        2 | 15822 |  |
|        - | 15823 | `	VmRefObj *pRef;` |
|        - | 15824 | `	sxu32 n;` |
|        - | 15825 | `	/* Check if the referenced object already exists */` |
|  3147242 | 15826 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3147242 | 15827 | `	if( pRef == 0 ){` |
|        - | 15828 | `		/* Not such entry */` |
|   105062 | 15829 | `		return SXERR_NOTFOUND;` |
|        - | 15830 | `	}` |
|        - | 15831 | `	/* Remove the desired entry */` |
|  3042182 | 15832 | `	if( pEntry ){` |
|        - | 15833 | `		SyHashEntry **apEntry;` |
|       74 | 15834 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 15835 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 15836 | `			if( apEntry[n] == pEntry ){` |
|        - | 15837 | `				/* Nullify the entry */` |
|       74 | 15838 | `				apEntry[n] = 0;` |
|        - | 15839 | `				/*` |
|        - | 15840 | `				 * NOTE:` |
|        - | 15841 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15842 | `				 * we avoid wasting spaces.` |
|        - | 15843 | `				 */` |
|       36 | 15844 | `			}` |
|       97 | 15845 | `		}` |
|       36 | 15846 | `	}` |
|  3042182 | 15847 | `	if( pMapEntry ){` |
|        - | 15848 | `		ph7_hashmap_node **apNode;` |
|  3042110 | 15849 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6084312 | 15850 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3042204 | 15851 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15852 | `				/* nullify the entry */` |
|  3042110 | 15853 | `				apNode[n] = 0;` |
|  1521054 | 15854 | `			}` |
|  1521103 | 15855 | `		}` |
|  1521054 | 15856 | `	}` |
|  3042182 | 15857 | `	return SXRET_OK;` |
|  1573622 | 15858 |  |
|        - | 15859 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15860 | `/*` |
|        - | 15861 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15862 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15863 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15864 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15865 | ` * For more information on how to register IO stream devices,please` |
|        - | 15866 | ` * refer to the official documentation.` |
|        - | 15867 | ` */` |
|    29048 | 15868 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15869 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15870 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15871 | `	int nByte              /* *pzDevice length*/` |
|        - | 15872 | `	)` |
|        2 | 15873 |  |
|        - | 15874 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15875 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15876 | `	SyString sDev,sCur;` |
|        - | 15877 | `	sxu32 n,nEntry;` |
|        - | 15878 | `	int rc;` |
|        - | 15879 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29050 | 15880 | `	zNext = zCur = zIn = *pzDevice;` |
|    29050 | 15881 | `	zEnd = &zIn[nByte];` |
|  1855801 | 15882 | `	while( zIn < zEnd ){` |
|  1826755 | 15883 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15884 | `			/* Got one */` |
|        3 | 15885 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15886 | `			break;` |
|        - | 15887 | `		}` |
|        - | 15888 | `		/* Advance the cursor */` |
|  1826753 | 15889 | `		zIn++;` |
|        2 | 15890 | `	}` |
|    29050 | 15891 | `	if( zIn >= zEnd ){` |
|        - | 15892 | `		/* No such scheme,return the default stream */` |
|    29048 | 15893 | `		return pVm->pDefStream;` |
|        - | 15894 | `	}` |
|        3 | 15895 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15896 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15897 | `	SyStringFullTrim(&sDev);` |
|        - | 15898 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15899 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15900 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15901 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15902 | `		pStream = apStream[n];` |
|        3 | 15903 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15904 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15905 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15906 | `		if( rc == 0 ){` |
|        - | 15907 | `			/* Stream device found */` |
|        3 | 15908 | `			*pzDevice = zNext;` |
|        3 | 15909 | `			return pStream;` |
|        - | 15910 | `		}` |
|      ! 0 | 15911 | `	}` |
|        - | 15912 | `	/* No such stream,return NULL */` |
|      ! 0 | 15913 | `	return 0;` |
|    14526 | 15914 |  |
|        - | 15915 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15916 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15917 |  |
