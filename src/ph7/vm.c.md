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
|        - |  2658 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  2659 | ` *` |
|        - |  2660 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  2661 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  2662 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  2663 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  2664 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  2665 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  2666 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  2667 | ` * calling it from a VM op.` |
|        - |  2668 | ` */` |
|      ! 0 |  2669 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  2670 |  |
|      ! 0 |  2671 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  2672 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  2673 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  2674 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  2675 | `	return PH7_ABORT;` |
|      ! 0 |  2676 |  |
|        - |  2677 | `/*` |
|        - |  2678 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  2679 | ` */` |
|      ! 0 |  2680 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  2681 |  |
|      ! 0 |  2682 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  2683 |  |
|        - |  2684 | `/*` |
|        - |  2685 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2686 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2687 | ` * information.` |
|        - |  2688 | ` */` |
|       40 |  2689 | `static sxi32 VmThrowErrorAp(` |
|        - |  2690 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2691 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2692 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2693 | `	const char *zFormat, /* Format message */` |
|        - |  2694 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2695 | `	)` |
|        2 |  2696 |  |
|       42 |  2697 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2698 | `	SyBlob sMsg;` |
|        - |  2699 | `	SyString *pFile;` |
|        - |  2700 | `	char *zErr;` |
|       42 |  2701 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2702 | `	if( !pVm->bErrReport ){` |
|        - |  2703 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2704 | `		return SXRET_OK;` |
|        - |  2705 | `	}` |
|        - |  2706 | `	/* Reset the working buffer */` |
|       42 |  2707 | `	SyBlobReset(pWorker);` |
|        - |  2708 | `	/* Peek the processed file if available */` |
|       42 |  2709 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2710 | `	if( pFile ){` |
|        - |  2711 | `		/* Append file name */` |
|       42 |  2712 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2713 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2714 | `	}` |
|        - |  2715 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2716 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2717 | `	 * the correct errno value. */` |
|       42 |  2718 | `	zErr = "Error:  ";` |
|       42 |  2719 | `	switch(iErr){` |
|        4 |  2720 | `	case PH7_CTX_WARNING:` |
|        9 |  2721 | `		zErr = "Warning:  ";` |
|        9 |  2722 | `		break;` |
|        3 |  2723 | `	case PH7_CTX_NOTICE:` |
|        7 |  2724 | `		zErr = "Notice:  ";` |
|        6 |  2725 | `		break;` |
|       13 |  2726 | `	default:` |
|        - |  2727 | `		/* do not change iErr */` |
|       26 |  2728 | `		break;` |
|        - |  2729 | `	}` |
|       42 |  2730 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2731 | `	if( pFuncName ){` |
|        - |  2732 | `		/* Append function name first */` |
|       26 |  2733 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2734 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2735 | `	}` |
|        - |  2736 | `	/* Format the raw message */` |
|       42 |  2737 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2738 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2739 | `	/* Check if a user error handler is installed */` |
|       42 |  2740 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2741 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2742 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2743 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2744 | `	}` |
|       42 |  2745 | `	SyBlobRelease(&sMsg);` |
|       42 |  2746 | `	return rc;` |
|       22 |  2747 |  |
|        - |  2748 | `/*` |
|        - |  2749 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2750 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2751 | ` * possible.` |
|        - |  2752 | ` */` |
|       38 |  2753 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2754 |  |
|        - |  2755 | `	ph7_class *pClass;` |
|       39 |  2756 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2757 | `	ph7_class_instance *pThis;` |
|        - |  2758 | `	ph7_class_method *pCons;` |
|        - |  2759 | `	ph7_value sArg;` |
|        - |  2760 | `	ph7_value *apArg[1];` |
|        - |  2761 | `	SyBlob sMsg;` |
|        - |  2762 | `	SyString sMsgStr;` |
|        - |  2763 | `	VmFrame *pFrame;` |
|        - |  2764 | `	sxi32 rc;` |
|       39 |  2765 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2766 | `	if( pClass == 0 ){` |
|      ! 0 |  2767 | `		return PH7_ABORT;` |
|        - |  2768 | `	}` |
|       39 |  2769 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2770 | `	if( pThis == 0 ){` |
|      ! 0 |  2771 | `		return PH7_ABORT;` |
|        - |  2772 | `	}` |
|       39 |  2773 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2774 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2775 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2776 | `	{` |
|       39 |  2777 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2778 | `		if( pOwner ){` |
|       39 |  2779 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2780 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2781 | `		}else{` |
|      ! 0 |  2782 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2783 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2784 | `		}` |
|        - |  2785 | `	}` |
|       39 |  2786 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2787 | `	if( pCons ){` |
|       39 |  2788 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2789 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2790 | `		apArg[0] = &sArg;` |
|       39 |  2791 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2792 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2793 | `	}` |
|       39 |  2794 | `	SyBlobRelease(&sMsg);` |
|       39 |  2795 | `	pFrame = pVm->pFrame;` |
|       39 |  2796 | `	if( pFrame ){` |
|       39 |  2797 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2798 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2799 | `	}` |
|       39 |  2800 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2801 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2802 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2803 | `		return PH7_ABORT;` |
|        - |  2804 | `	}` |
|       39 |  2805 | `	return PH7_EXCEPTION;` |
|       20 |  2806 |  |
|        - |  2807 |  |
|        - |  2808 | `/*` |
|        - |  2809 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2810 | ` */` |
|        4 |  2811 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2812 |  |
|        - |  2813 | `	ph7_class *pErrClass;` |
|        - |  2814 | `	ph7_class_instance *pThis;` |
|        - |  2815 | `	ph7_class_method *pCons;` |
|        - |  2816 | `	ph7_value sArg;` |
|        - |  2817 | `	ph7_value *apArg[1];` |
|        - |  2818 | `	SyBlob sMsg;` |
|        - |  2819 | `	SyString sMsgStr;` |
|        - |  2820 | `	VmFrame *pFrame;` |
|        - |  2821 | `	sxi32 rc;` |
|        5 |  2822 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2823 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2824 | `		return PH7_ABORT;` |
|        - |  2825 | `	}` |
|        5 |  2826 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2827 | `	if( pThis == 0 ){` |
|      ! 0 |  2828 | `		return PH7_ABORT;` |
|        - |  2829 | `	}` |
|        5 |  2830 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2831 | `	{` |
|        5 |  2832 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2833 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2834 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2835 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2836 | `	}` |
|        5 |  2837 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2838 | `	if( pCons ){` |
|        5 |  2839 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2840 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2841 | `		apArg[0] = &sArg;` |
|        5 |  2842 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2843 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2844 | `	}` |
|        5 |  2845 | `	SyBlobRelease(&sMsg);` |
|        5 |  2846 | `	pFrame = pVm->pFrame;` |
|        5 |  2847 | `	if( pFrame ){` |
|        5 |  2848 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2849 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2850 | `	}` |
|        5 |  2851 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2852 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2853 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2854 | `		return PH7_ABORT;` |
|        - |  2855 | `	}` |
|        5 |  2856 | `	return PH7_EXCEPTION;` |
|        3 |  2857 |  |
|        - |  2858 |  |
|        - |  2859 | `/*` |
|        - |  2860 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2861 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2862 | ` * For class types, instanceof is verified.` |
|        - |  2863 | ` *` |
|        - |  2864 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2865 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2866 | ` */` |
|        - |  2867 | `/*` |
|        - |  2868 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2869 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2870 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2871 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2872 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2873 | ` */` |
|       20 |  2874 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2875 |  |
|        - |  2876 | `	const char *z, *zEnd, *zTail;` |
|        - |  2877 | `	sxu32 n;` |
|        - |  2878 | `	sxu8 bReal;` |
|        - |  2879 | `	sxi32 rc;` |
|       22 |  2880 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2881 | `		return 0;` |
|        - |  2882 | `	}` |
|       22 |  2883 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2884 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2885 | `	zEnd = z + n;` |
|       22 |  2886 | `	if( n == 0 ){` |
|      ! 0 |  2887 | `		return 0;` |
|        - |  2888 | `	}` |
|       22 |  2889 | `	zTail = 0;` |
|       22 |  2890 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2891 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2892 | `		return 0;` |
|        - |  2893 | `	}` |
|        - |  2894 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2895 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2896 | `		zTail++;` |
|      ! 0 |  2897 | `	}` |
|       16 |  2898 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2899 |  |
|        - |  2900 |  |
|        - |  2901 | `/*` |
|        - |  2902 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2903 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2904 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2905 | ` *   0 if it's not strictly numeric.` |
|        - |  2906 | ` */` |
|       16 |  2907 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2908 |  |
|        - |  2909 | `	const char *z, *zEnd, *zTail;` |
|        - |  2910 | `	sxu32 n;` |
|       18 |  2911 | `	sxu8 bReal = 0;` |
|        - |  2912 | `	sxi32 rc;` |
|       18 |  2913 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2914 | `		return 0;` |
|        - |  2915 | `	}` |
|       18 |  2916 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2917 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2918 | `	zEnd = z + n;` |
|       18 |  2919 | `	if( n == 0 ) return 0;` |
|       18 |  2920 | `	zTail = 0;` |
|       18 |  2921 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2922 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2923 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2924 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2925 | `	return bReal ? 2 : 1;` |
|       10 |  2926 |  |
|        - |  2927 |  |
|        - |  2928 | `/*` |
|        - |  2929 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2930 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2931 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2932 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2933 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2934 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2935 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2936 | ` * throw.` |
|        - |  2937 | ` *` |
|        - |  2938 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2939 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2940 | ` */` |
|       98 |  2941 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2942 |  |
|        - |  2943 | `	sxu32 i;` |
|        - |  2944 | `	ph7_type_alt *aAlts;` |
|        - |  2945 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2946 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2947 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2948 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2949 | `	}` |
|       88 |  2950 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2951 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2952 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2953 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2954 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2955 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2956 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2957 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2958 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2959 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2960 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2961 | `	}` |
|        - |  2962 | `	/* Object handling */` |
|       88 |  2963 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2964 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2965 | `		if( bHasClassAlt ){` |
|       14 |  2966 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2967 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2968 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2969 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2970 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2971 | `			}` |
|       26 |  2972 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2973 | `				ph7_class *pExpected;` |
|        - |  2974 | `				SyString *pCN;` |
|       22 |  2975 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2976 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2977 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2978 | `					pExpected = pSelfNow;` |
|       22 |  2979 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2980 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2981 | `				}else{` |
|       22 |  2982 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2983 | `				}` |
|       22 |  2984 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2985 | `					return SXRET_OK;` |
|        - |  2986 | `				}` |
|        8 |  2987 | `			}` |
|        2 |  2988 | `		}` |
|        9 |  2989 | `		return SXERR_INVALID;` |
|        - |  2990 | `	}` |
|        - |  2991 | `	/* Array handling */` |
|       72 |  2992 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2993 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2994 | `	}` |
|        - |  2995 | `	/* Scalar handling — exact match first */` |
|       66 |  2996 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2997 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2998 | `	}` |
|       42 |  2999 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3000 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3001 | `	}` |
|       38 |  3002 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  3003 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3004 | `	}` |
|       18 |  3005 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3006 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3007 | `	}` |
|       18 |  3008 | `	if( bStrict ){` |
|        - |  3009 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3010 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3011 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3012 | `			return SXRET_OK;` |
|        - |  3013 | `		}` |
|      ! 0 |  3014 | `		return SXERR_INVALID;` |
|        - |  3015 | `	}` |
|        - |  3016 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3017 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3018 | `	 * to match PHP's union RFC. */` |
|        - |  3019 | `	{` |
|       18 |  3020 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3021 | `		if( bHasInt ){` |
|        - |  3022 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3023 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3024 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3025 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3026 | `				return SXRET_OK;` |
|        - |  3027 | `			}` |
|       18 |  3028 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3029 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3030 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3031 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3032 | `					return SXRET_OK;` |
|        - |  3033 | `				}` |
|      ! 0 |  3034 | `			}` |
|       18 |  3035 | `			if( kind == 1 ){` |
|        9 |  3036 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3037 | `				return SXRET_OK;` |
|        - |  3038 | `			}` |
|        4 |  3039 | `		}` |
|       10 |  3040 | `		if( bHasFloat ){` |
|       10 |  3041 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3042 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3043 | `				return SXRET_OK;` |
|        - |  3044 | `			}` |
|       10 |  3045 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3046 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3047 | `				return SXRET_OK;` |
|        - |  3048 | `			}` |
|        1 |  3049 | `		}` |
|        3 |  3050 | `		if( bHasString ){` |
|      ! 0 |  3051 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3052 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3053 | `				return SXRET_OK;` |
|        - |  3054 | `			}` |
|      ! 0 |  3055 | `		}` |
|        3 |  3056 | `		if( bHasBool ){` |
|      ! 0 |  3057 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3058 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3059 | `				return SXRET_OK;` |
|        - |  3060 | `			}` |
|      ! 0 |  3061 | `		}` |
|        - |  3062 | `	}` |
|        3 |  3063 | `	return SXERR_INVALID;` |
|       51 |  3064 |  |
|        - |  3065 |  |
|        - |  3066 | `/*` |
|        - |  3067 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3068 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3069 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3070 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3071 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3072 | ` */` |
|       36 |  3073 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3074 |  |
|       38 |  3075 | `	if( bStrict ){` |
|        - |  3076 | `		/* Only int -> float widening is allowed implicitly. */` |
|       12 |  3077 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3078 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3079 | `			return SXRET_OK;` |
|        - |  3080 | `		}` |
|       10 |  3081 | `		return SXERR_INVALID;` |
|        - |  3082 | `	}` |
|        - |  3083 | `	{` |
|       28 |  3084 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3085 | `		if( xCast ) xCast(pVal);` |
|        - |  3086 | `	}` |
|       28 |  3087 | `	return SXRET_OK;` |
|       20 |  3088 |  |
|        - |  3089 |  |
|        - |  3090 | `/*` |
|        - |  3091 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3092 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3093 | ` *` |
|        - |  3094 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3095 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3096 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3097 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3098 | ` */` |
|       10 |  3099 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        2 |  3100 |  |
|       12 |  3101 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       12 |  3102 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       12 |  3103 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       12 |  3104 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       12 |  3105 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        5 |  3106 | `		}` |
|       12 |  3107 | `		zBuf[nCopy] = 0;` |
|       12 |  3108 | `		return zBuf;` |
|        - |  3109 | `	}` |
|      ! 0 |  3110 | `	switch( nType ){` |
|      ! 0 |  3111 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3112 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3113 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3114 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3115 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3116 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3117 | `		default:             return "scalar";` |
|        - |  3118 | `	}` |
|        7 |  3119 |  |
|        - |  3120 |  |
|        - |  3121 | `/*` |
|        - |  3122 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3123 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3124 | ` */` |
|       18 |  3125 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3126 |  |
|       19 |  3127 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3128 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3129 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3130 | `	return zBuf;` |
|        1 |  3131 |  |
|        - |  3132 |  |
|    14524 |  3133 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3134 |  |
|        - |  3135 | `	SyHashEntry *pSlot;` |
|        - |  3136 | `	VmClassAttr *pVmAttr;` |
|        - |  3137 | `	ph7_class_attr *pAttr;` |
|    14526 |  3138 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14526 |  3139 | `	if( pSlot == 0 ){` |
|    14318 |  3140 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3141 | `	}` |
|      210 |  3142 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3143 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3144 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3145 | `		return SXRET_OK;` |
|        - |  3146 | `	}` |
|        - |  3147 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3148 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3149 | `	 * matching PHP's documented behavior. */` |
|      210 |  3150 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3151 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3152 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3153 |  |
|       16 |  3154 | `		if( rc == SXRET_OK ){` |
|        9 |  3155 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3156 | `			return SXRET_OK;` |
|        - |  3157 | `		}` |
|        7 |  3158 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3159 | `			char zBuf[128];` |
|        4 |  3160 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3161 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3162 | `		}` |
|        5 |  3163 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3164 | `	}` |
|        - |  3165 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3166 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3167 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3168 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3169 | `			return SXRET_OK;` |
|        - |  3170 | `		}` |
|        3 |  3171 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3172 | `	}` |
|        - |  3173 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3174 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3175 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3176 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3177 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3178 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3179 | `			return SXRET_OK;` |
|        - |  3180 | `		}` |
|        7 |  3181 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3182 | `	}` |
|      174 |  3183 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3184 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3185 | `		 * currently active on the self-stack. */` |
|       26 |  3186 | `		ph7_class *pExpected = 0;` |
|       26 |  3187 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3188 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3189 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3190 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3191 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3192 | `		}` |
|       26 |  3193 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3194 | `			pExpected = pSelfNow;` |
|       24 |  3195 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3196 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3197 | `		}else{` |
|       22 |  3198 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3199 | `		}` |
|       26 |  3200 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3201 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3202 | `		}` |
|       26 |  3203 | `		if( pExpected ){` |
|       22 |  3204 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3205 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3206 | `				char zBuf[128];` |
|        7 |  3207 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3208 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3209 | `			}` |
|        8 |  3210 | `		}` |
|       22 |  3211 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3212 | `		return SXRET_OK;` |
|        - |  3213 | `	}` |
|        - |  3214 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3215 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3216 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3217 | `		char zBuf[128];` |
|       10 |  3218 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3219 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3220 | `	}` |
|      144 |  3221 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3222 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3223 | `		if( xCast ){` |
|        - |  3224 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3225 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3226 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3227 | `			}` |
|       24 |  3228 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3229 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3230 | `			}` |
|        - |  3231 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3232 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3233 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3234 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3235 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3236 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3237 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3238 | `			}` |
|       12 |  3239 | `			xCast(pValue);` |
|        5 |  3240 | `		}` |
|        5 |  3241 | `	}` |
|      130 |  3242 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3243 | `	return SXRET_OK;` |
|     7264 |  3244 |  |
|        - |  3245 |  |
|        - |  3246 | `/*` |
|        - |  3247 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3248 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3249 | ` * information.` |
|        - |  3250 | ` * ------------------------------------` |
|        - |  3251 | ` * Simple boring wrapper function.` |
|        - |  3252 | ` * ------------------------------------` |
|        - |  3253 | ` */` |
|       16 |  3254 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3255 |  |
|        - |  3256 | `	va_list ap;` |
|        - |  3257 | `	sxi32 rc;` |
|       17 |  3258 | `	va_start(ap,zFormat);` |
|       17 |  3259 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3260 | `	va_end(ap);` |
|       17 |  3261 | `	return rc;` |
|        1 |  3262 |  |
|        - |  3263 | `/*` |
|        - |  3264 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3265 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3266 | ` */` |
|       36 |  3267 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        2 |  3268 |  |
|        - |  3269 | `	ph7_class *pClass;` |
|        - |  3270 | `	ph7_class_instance *pThis;` |
|        - |  3271 | `	ph7_class_method *pCons;` |
|        - |  3272 | `	ph7_value sArg;` |
|        - |  3273 | `	ph7_value *apArg[1];` |
|        - |  3274 | `	SyBlob sMsg;` |
|        - |  3275 | `	SyString sMsgStr;` |
|        - |  3276 | `	VmFrame *pFrame;` |
|        - |  3277 | `	sxi32 rc;` |
|       38 |  3278 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       38 |  3279 | `	if( pClass == 0 ){` |
|      ! 0 |  3280 | `		return PH7_ABORT;` |
|        - |  3281 | `	}` |
|       38 |  3282 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       38 |  3283 | `	if( pThis == 0 ){` |
|      ! 0 |  3284 | `		return PH7_ABORT;` |
|        - |  3285 | `	}` |
|       38 |  3286 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       38 |  3287 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       18 |  3288 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       38 |  3289 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       38 |  3290 | `	if( pCons ){` |
|       38 |  3291 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       38 |  3292 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       38 |  3293 | `		apArg[0] = &sArg;` |
|       38 |  3294 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       38 |  3295 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  3296 | `	}` |
|       38 |  3297 | `	SyBlobRelease(&sMsg);` |
|       38 |  3298 | `	pFrame = pVm->pFrame;` |
|       38 |  3299 | `	if( pFrame ){` |
|       38 |  3300 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  3301 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  3302 | `	}` |
|       38 |  3303 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  3304 | `	PH7_ClassInstanceUnref(pThis);` |
|       38 |  3305 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3306 | `		return PH7_ABORT;` |
|        - |  3307 | `	}` |
|       34 |  3308 | `	return PH7_EXCEPTION;` |
|       20 |  3309 |  |
|        - |  3310 | `/*` |
|        - |  3311 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3312 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3313 | ` */` |
|        6 |  3314 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3315 |  |
|        - |  3316 | `	ph7_class *pClass;` |
|        - |  3317 | `	ph7_class_instance *pThis;` |
|        - |  3318 | `	ph7_class_method *pCons;` |
|        - |  3319 | `	ph7_value sArg;` |
|        - |  3320 | `	ph7_value *apArg[1];` |
|        - |  3321 | `	SyBlob sMsg;` |
|        - |  3322 | `	SyString sMsgStr;` |
|        - |  3323 | `	VmFrame *pFrame;` |
|        - |  3324 | `	sxi32 rc;` |
|        7 |  3325 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3326 | `	if( pClass == 0 ){` |
|      ! 0 |  3327 | `		return PH7_ABORT;` |
|        - |  3328 | `	}` |
|        7 |  3329 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3330 | `	if( pThis == 0 ){` |
|      ! 0 |  3331 | `		return PH7_ABORT;` |
|        - |  3332 | `	}` |
|        7 |  3333 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3334 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3335 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3336 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3337 | `	if( pCons ){` |
|        7 |  3338 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3339 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3340 | `		apArg[0] = &sArg;` |
|        7 |  3341 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3342 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3343 | `	}` |
|        7 |  3344 | `	SyBlobRelease(&sMsg);` |
|        7 |  3345 | `	pFrame = pVm->pFrame;` |
|        7 |  3346 | `	if( pFrame ){` |
|        7 |  3347 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3348 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3349 | `	}` |
|        7 |  3350 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3351 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3352 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3353 | `		return PH7_ABORT;` |
|        - |  3354 | `	}` |
|      ! 0 |  3355 | `	return PH7_EXCEPTION;` |
|        4 |  3356 |  |
|        - |  3357 | `/*` |
|        - |  3358 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3359 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3360 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3361 | ` */` |
|       16 |  3362 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3363 |  |
|       17 |  3364 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3365 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3366 | `	}` |
|       13 |  3367 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3368 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3369 | `		if( pThis && pThis->pClass ){` |
|        5 |  3370 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3371 | `			sxu32 n = pName->nByte;` |
|        5 |  3372 | `			if( n >= nBuf ){` |
|      ! 0 |  3373 | `				n = nBuf - 1;` |
|      ! 0 |  3374 | `			}` |
|        5 |  3375 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3376 | `			zBuf[n] = 0;` |
|        5 |  3377 | `			return zBuf;` |
|        - |  3378 | `		}` |
|      ! 0 |  3379 | `		return "object";` |
|        - |  3380 | `	}` |
|        9 |  3381 | `	return ph7_type_name(pVal);` |
|        9 |  3382 |  |
|        - |  3383 | `/*` |
|        - |  3384 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3385 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3386 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3387 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3388 | ` */` |
|       16 |  3389 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3390 |  |
|        - |  3391 | `	ph7_class *pClass;` |
|        - |  3392 | `	ph7_class_instance *pThis;` |
|        - |  3393 | `	ph7_class_method *pCons;` |
|        - |  3394 | `	ph7_value sArg;` |
|        - |  3395 | `	ph7_value *apArg[1];` |
|        - |  3396 | `	SyBlob sMsg;` |
|        - |  3397 | `	SyString sMsgStr;` |
|        - |  3398 | `	VmFrame *pFrame;` |
|        - |  3399 | `	sxi32 rc;` |
|       17 |  3400 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3401 | `	char zNameBuf[64];` |
|       17 |  3402 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3403 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3404 | `	if( pClass == 0 ){` |
|      ! 0 |  3405 | `		return PH7_ABORT;` |
|        - |  3406 | `	}` |
|       17 |  3407 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3408 | `	if( pThis == 0 ){` |
|      ! 0 |  3409 | `		return PH7_ABORT;` |
|        - |  3410 | `	}` |
|       17 |  3411 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3412 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3413 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3414 | `	if( pCons ){` |
|       17 |  3415 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3416 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3417 | `		apArg[0] = &sArg;` |
|       17 |  3418 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3419 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3420 | `	}` |
|       17 |  3421 | `	SyBlobRelease(&sMsg);` |
|       17 |  3422 | `	pFrame = pVm->pFrame;` |
|       17 |  3423 | `	if( pFrame ){` |
|       17 |  3424 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3425 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3426 | `	}` |
|       17 |  3427 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3428 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3429 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3430 | `		return PH7_ABORT;` |
|        - |  3431 | `	}` |
|       17 |  3432 | `	return PH7_EXCEPTION;` |
|        9 |  3433 |  |
|        - |  3434 | `/*` |
|        - |  3435 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3436 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3437 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3438 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3439 | ` */` |
|        - |  3440 | `/*` |
|        - |  3441 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3442 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3443 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3444 | ` */` |
|       24 |  3445 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3446 |  |
|        - |  3447 | `	sxu32 nCopy;` |
|       26 |  3448 | `	if( nBuf == 0 ) return "";` |
|       26 |  3449 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3450 | `		zBuf[0] = 0;` |
|      ! 0 |  3451 | `		return zBuf;` |
|        - |  3452 | `	}` |
|       26 |  3453 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3454 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3455 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3456 | `	zBuf[nCopy] = 0;` |
|       26 |  3457 | `	return zBuf;` |
|       14 |  3458 |  |
|        - |  3459 |  |
|      396 |  3460 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3461 |  |
|      398 |  3462 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3463 | `	const char *zGiven;` |
|        - |  3464 | `	char zBuf[128];` |
|        - |  3465 | `	char zTypeBuf[128];` |
|        - |  3466 | `	/* Untyped function: no enforcement. */` |
|      398 |  3467 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3468 | `		return SXRET_OK;` |
|        - |  3469 | `	}` |
|        - |  3470 | `	/* void return type: the function must not produce a value. */` |
|      398 |  3471 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3472 | `		if( pValue == 0 ){` |
|      134 |  3473 | `			return SXRET_OK;` |
|        - |  3474 | `		}` |
|        - |  3475 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3476 | `		 * still counts as "returned a value" here. */` |
|        3 |  3477 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3478 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3479 | `	}` |
|        - |  3480 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3481 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3482 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      264 |  3483 | `	if( pValue == 0 ){` |
|      ! 0 |  3484 | `		const char *zExpected = "value";` |
|      ! 0 |  3485 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3486 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3487 | `		}` |
|      ! 0 |  3488 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3489 | `	}` |
|        - |  3490 | ``	/* `mixed` accepts any explicitly returned value, including null. It is`` |
|        - |  3491 | `	 * parsed as a class-name atom (SXU32_HIGH, sReturnClass = "mixed") since` |
|        - |  3492 | `	 * it is not a scalar keyword, so short-circuit it here before the null /` |
|        - |  3493 | `	 * class-type checks below — which would otherwise demand an object. */` |
|      272 |  3494 | `	if( pFunc->nReturnType == SXU32_HIGH` |
|      143 |  3495 | `	 && pFunc->sReturnClass.nByte == 5` |
|       24 |  3496 | `	 && SyStrnicmp(pFunc->sReturnClass.zString,"mixed",5) == 0 ){` |
|       21 |  3497 | `		return SXRET_OK;` |
|        - |  3498 | `	}` |
|        - |  3499 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3500 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3501 | `	 * bNullable=0 here. */` |
|      244 |  3502 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3503 | `		sxi32 rcU;` |
|      ! 0 |  3504 | `		int bNullable = 0;` |
|      ! 0 |  3505 | `		const char *zExpected = "union";` |
|        - |  3506 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3507 | `		{` |
|        - |  3508 | `			sxu32 i;` |
|      ! 0 |  3509 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3510 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3511 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3512 | `			}` |
|        - |  3513 | `		}` |
|      ! 0 |  3514 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3515 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3516 | `			return SXRET_OK;` |
|        - |  3517 | `		}` |
|      ! 0 |  3518 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3519 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3520 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3521 | `			zGiven = "null";` |
|      ! 0 |  3522 | `		}else{` |
|      ! 0 |  3523 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3524 | `		}` |
|      ! 0 |  3525 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3526 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3527 | `		}` |
|      ! 0 |  3528 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3529 | `	}` |
|        - |  3530 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3531 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3532 | `	 * it into the TypeError message. */` |
|      244 |  3533 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3534 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3535 | `		const char *zExpected;` |
|        - |  3536 | `		ph7_class *pExpected;` |
|        6 |  3537 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3538 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3539 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3540 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3541 | `		}` |
|        6 |  3542 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3543 | `			pExpected = pSelfNow;` |
|        4 |  3544 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3545 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3546 | `		}else{` |
|        3 |  3547 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3548 | `		}` |
|        6 |  3549 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3550 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3551 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3552 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3553 | `		}` |
|        6 |  3554 | `		if( pExpected ){` |
|        6 |  3555 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3556 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3557 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3558 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3559 | `			}` |
|        2 |  3560 | `		}` |
|        6 |  3561 | `		return SXRET_OK;` |
|        - |  3562 | `	}` |
|        - |  3563 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3564 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3565 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3566 | `	 * via the type-text leading '?'. */` |
|      240 |  3567 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3568 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3569 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3570 | `			return SXRET_OK;` |
|        - |  3571 | `		}` |
|      ! 0 |  3572 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3573 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3574 | `			"null");` |
|        - |  3575 | `	}` |
|        - |  3576 | `	/* Exact match? Done. */` |
|      234 |  3577 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3578 | `		return SXRET_OK;` |
|        - |  3579 | `	}` |
|        - |  3580 | `	/* Object->scalar is never compatible. */` |
|        8 |  3581 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3582 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3583 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3584 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3585 | `			zGiven);` |
|        - |  3586 | `	}` |
|        - |  3587 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3588 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3589 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3590 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3591 | `			ph7_type_name(pValue));` |
|        - |  3592 | `	}` |
|        - |  3593 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3594 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3595 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3596 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3597 | `	if( !bStrict` |
|        5 |  3598 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3599 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3600 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3601 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3602 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3603 | `			"string");` |
|        - |  3604 | `	}` |
|        6 |  3605 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3606 | `		return SXRET_OK;` |
|        - |  3607 | `	}` |
|        4 |  3608 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3609 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3610 | `		ph7_type_name(pValue));` |
|      200 |  3611 |  |
|        - |  3612 | `/*` |
|        - |  3613 | ` * Report a fatal named-argument error.` |
|        - |  3614 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3615 | ` */` |
|        6 |  3616 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3617 |  |
|        7 |  3618 | `	const char *zFunc = 0;` |
|        7 |  3619 | `	int nFunc = 0;` |
|        7 |  3620 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3621 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3622 |  |
|        - |  3623 | `/*` |
|        - |  3624 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3625 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3626 | ` * information.` |
|        - |  3627 | ` * ------------------------------------` |
|        - |  3628 | ` * Simple boring wrapper function.` |
|        - |  3629 | ` * ------------------------------------` |
|        - |  3630 | ` */` |
|       24 |  3631 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3632 |  |
|        - |  3633 | `	sxi32 rc;` |
|       26 |  3634 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3635 | `	return rc;` |
|        2 |  3636 |  |
|        - |  3637 | `/*` |
|        - |  3638 | ` * Resolve function context from the current frame.` |
|        - |  3639 | ` */` |
|     1018 |  3640 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3641 |  |
|        - |  3642 | `	VmFrame *pFrame;` |
|        - |  3643 | `	ph7_vm_func *pFunc;` |
|     1019 |  3644 | `	*pzFuncName = 0;` |
|     1019 |  3645 | `	*pnFuncLen = 0;` |
|     1019 |  3646 | `	pFrame = pVm->pFrame;` |
|     1019 |  3647 | `	if( pFrame == 0 ){` |
|      ! 0 |  3648 | `		return;` |
|        - |  3649 | `	}` |
|     1019 |  3650 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3651 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3652 | `		return;` |
|        - |  3653 | `	}` |
|       25 |  3654 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3655 | `	if( pFunc == 0 ){` |
|      ! 0 |  3656 | `		return;` |
|        - |  3657 | `	}` |
|       25 |  3658 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3659 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3660 |  |
|        - |  3661 | `/*` |
|        - |  3662 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3663 | ` */` |
|      524 |  3664 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3665 |  |
|        - |  3666 | `	SyBlob sOut;` |
|        - |  3667 | `	SyString *pFile;` |
|      525 |  3668 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3669 | `		return PH7_OK;` |
|        - |  3670 | `	}` |
|      525 |  3671 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3672 | `		zClass = "Exception";` |
|      ! 0 |  3673 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3674 | `	}` |
|      525 |  3675 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3676 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3677 | `	}` |
|      525 |  3678 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3679 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3680 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3681 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3682 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3683 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3684 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3685 | `	}` |
|      525 |  3686 | `	if( pFile ){` |
|      525 |  3687 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  3688 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3689 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  3690 | `	}` |
|      525 |  3691 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  3692 | `	if( pFile ){` |
|      525 |  3693 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  3694 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3695 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3696 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3697 | `		}else{` |
|      501 |  3698 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3699 | `		}` |
|      262 |  3700 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3701 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3702 | `	}else{` |
|      ! 0 |  3703 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3704 | `	}` |
|      525 |  3705 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  3706 | `	if( pFile ){` |
|      525 |  3707 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  3708 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  3709 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3710 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  3711 | `	}` |
|      525 |  3712 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  3713 | `	SyBlobRelease(&sOut);` |
|      525 |  3714 | `	return PH7_ABORT;` |
|      263 |  3715 |  |
|        - |  3716 | `/*` |
|        - |  3717 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  3718 | ` *` |
|        - |  3719 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  3720 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  3721 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  3722 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  3723 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  3724 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  3725 | ` */` |
|      862 |  3726 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  3727 |  |
|      864 |  3728 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  3729 | `		if( pVm->pCoalesceObj ){` |
|        7 |  3730 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  3731 | `		}` |
|        7 |  3732 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  3733 | `		pVm->pCoalesceObj = 0;` |
|        7 |  3734 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  3735 | `	}` |
|      864 |  3736 |  |
|        - |  3737 | `/*` |
|        - |  3738 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  3739 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  3740 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  3741 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  3742 | ` *` |
|        - |  3743 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  3744 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  3745 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  3746 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  3747 | ` */` |
|        4 |  3748 | `static sxi32 VmThrowFromVm(` |
|        - |  3749 | `	ph7_vm *pVm,` |
|        - |  3750 | `	const char *zClass,` |
|        - |  3751 | `	const char *zMsg,` |
|        - |  3752 | `	sxu32 nMsg` |
|        1 |  3753 | `){` |
|        - |  3754 | `	ph7_class *pClass;` |
|        - |  3755 | `	ph7_class_instance *pThis;` |
|        - |  3756 | `	ph7_class_method *pCons;` |
|        - |  3757 | `	VmFrame *pFrame;` |
|        - |  3758 | `	sxi32 rc;` |
|        5 |  3759 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  3760 | `	if( pClass == 0 ){` |
|      ! 0 |  3761 | `		return SXERR_ABORT;` |
|        - |  3762 | `	}` |
|        5 |  3763 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  3764 | `	if( pThis == 0 ){` |
|      ! 0 |  3765 | `		return SXERR_ABORT;` |
|        - |  3766 | `	}` |
|        5 |  3767 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3768 | `	if( pCons ){` |
|        - |  3769 | `		ph7_value sArg;` |
|        - |  3770 | `		ph7_value *apArg[1];` |
|        - |  3771 | `		SyString sMsgStr;` |
|        5 |  3772 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  3773 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  3774 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  3775 | `		apArg[0] = &sArg;` |
|        5 |  3776 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  3777 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3778 | `	}` |
|        5 |  3779 | `	pFrame = pVm->pFrame;` |
|        5 |  3780 | `	if( pFrame ){` |
|        5 |  3781 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3782 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3783 | `	}` |
|        5 |  3784 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  3785 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3786 | `	return rc;` |
|        3 |  3787 |  |
|        - |  3788 | `/*` |
|        - |  3789 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3790 | ` */` |
|      574 |  3791 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3792 |  |
|        - |  3793 | `	ph7_vm *pVm;` |
|        - |  3794 | `	ph7_class *pClass;` |
|        - |  3795 | `	ph7_class_instance *pThis;` |
|        - |  3796 | `	ph7_class_method *pCons;` |
|        - |  3797 | `	ph7_value sArg;` |
|        - |  3798 | `	ph7_value *apArg[1];` |
|        - |  3799 | `	SyBlob sMsg;` |
|        - |  3800 | `	SyString sMsgStr;` |
|        - |  3801 | `	VmFrame *pFrame;` |
|        - |  3802 | `	va_list ap;` |
|        - |  3803 | `	sxi32 rc;` |
|        - |  3804 |  |
|      576 |  3805 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3806 | `		return PH7_ABORT;` |
|        - |  3807 | `	}` |
|      576 |  3808 | `	pVm = pCtx->pVm;` |
|      576 |  3809 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3810 | `		zClass = "Error";` |
|      ! 0 |  3811 | `	}` |
|      576 |  3812 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  3813 | `	if( pClass == 0 ){` |
|      ! 0 |  3814 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3815 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3816 | `			zClass` |
|        - |  3817 | `			);` |
|        - |  3818 | `	}` |
|      576 |  3819 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  3820 | `	if( pThis == 0 ){` |
|      ! 0 |  3821 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3822 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3823 | `			);` |
|        - |  3824 | `	}` |
|        - |  3825 |  |
|      576 |  3826 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  3827 | `	va_start(ap,zFormat);` |
|      576 |  3828 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  3829 | `	va_end(ap);` |
|        - |  3830 |  |
|      576 |  3831 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  3832 | `	if( pCons ){` |
|      576 |  3833 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  3834 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  3835 | `		apArg[0] = &sArg;` |
|      576 |  3836 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  3837 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  3838 | `	}` |
|      576 |  3839 | `	SyBlobRelease(&sMsg);` |
|        - |  3840 |  |
|      576 |  3841 | `	pFrame = pVm->pFrame;` |
|      576 |  3842 | `	if( pFrame ){` |
|      576 |  3843 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  3844 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  3845 | `	}` |
|      576 |  3846 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  3847 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  3848 | `	if( rc == SXERR_ABORT ){` |
|      491 |  3849 | `		return PH7_ABORT;` |
|        - |  3850 | `	}` |
|       86 |  3851 | `	return PH7_EXCEPTION;` |
|      289 |  3852 |  |
|        - |  3853 | `/*` |
|        - |  3854 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3855 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3856 | ` */` |
|      ! 0 |  3857 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3858 |  |
|        - |  3859 | `	ph7_vm *pVm;` |
|        - |  3860 | `	SyBlob sMsg;` |
|      ! 0 |  3861 | `	const char *zFuncName = 0;` |
|      ! 0 |  3862 | `	int nFuncLen = 0;` |
|        - |  3863 | `	va_list ap;` |
|        - |  3864 | `	sxi32 rc;` |
|        - |  3865 |  |
|      ! 0 |  3866 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3867 | `		return PH7_OK;` |
|        - |  3868 | `	}` |
|      ! 0 |  3869 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3870 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3871 | `		zClass = "Error";` |
|      ! 0 |  3872 | `	}` |
|        - |  3873 |  |
|      ! 0 |  3874 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3875 |  |
|      ! 0 |  3876 | `	va_start(ap,zFormat);` |
|      ! 0 |  3877 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3878 | `	va_end(ap);` |
|        - |  3879 |  |
|      ! 0 |  3880 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3881 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3882 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3883 | `	}` |
|      ! 0 |  3884 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3885 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3886 | `	}` |
|      ! 0 |  3887 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3888 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3889 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3890 | `	return rc;` |
|      ! 0 |  3891 |  |
|        - |  3892 | `/*` |
|        - |  3893 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3894 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3895 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3896 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3897 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3898 | ` * when VmByteCodeExec returns.` |
|        - |  3899 | ` */` |
|      144 |  3900 | `static sxi32 VmSuspendCtx(` |
|        - |  3901 | `	ph7_vm *pVm,` |
|        - |  3902 | `	ph7_exec_ctx *pCtx,` |
|        - |  3903 | `	sxi32 pc,` |
|        - |  3904 | `	sxi32 nTos` |
|        - |  3905 | `	)` |
|        2 |  3906 |  |
|       72 |  3907 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3908 | `	pCtx->pc = pc;` |
|      146 |  3909 | `	pCtx->nTos = nTos;` |
|      146 |  3910 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3911 | `	return PH7_SUSPEND;` |
|        2 |  3912 |  |
|        - |  3913 | `/*` |
|        - |  3914 | ` * Resolve named-argument mapping.` |
|        - |  3915 | ` *` |
|        - |  3916 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3917 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3918 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3919 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3920 | ` * every formal parameter that received a value.` |
|        - |  3921 | ` *` |
|        - |  3922 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3923 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3924 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3925 | ` */` |
|       98 |  3926 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3927 | `	ph7_vm *pVm,` |
|        - |  3928 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3929 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3930 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3931 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3932 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3933 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3934 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3935 |  |
|        2 |  3936 |  |
|      100 |  3937 | `	sxi32 posIdx = 0;` |
|        - |  3938 | `	sxu32 i;` |
|        - |  3939 | `	char zErrMsg[256];` |
|      100 |  3940 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3941 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3942 | `		aSlot[i] = -2;` |
|      100 |  3943 | `	}` |
|      290 |  3944 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3945 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3946 | `			/* Named argument — find formal by name */` |
|      184 |  3947 | `			int found = 0;` |
|        - |  3948 | `			sxu32 k;` |
|      304 |  3949 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3950 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3951 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3952 | `						pMap->aNames[i].zString,` |
|      402 |  3953 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3954 | `					if( aUsed[k] ){` |
|        7 |  3955 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3956 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3957 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3958 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3959 | `						return PH7_ABORT;` |
|        - |  3960 | `					}` |
|      168 |  3961 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3962 | `					aUsed[k] = 1;` |
|      168 |  3963 | `					found = 1;` |
|      168 |  3964 | `					break;` |
|        - |  3965 | `				}` |
|       62 |  3966 | `			}` |
|      180 |  3967 | `			if( !found ){` |
|       14 |  3968 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3969 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3970 | `				}else{` |
|        4 |  3971 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3972 | `						"Unknown named parameter $%.*s",` |
|        2 |  3973 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3974 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3975 | `					return PH7_ABORT;` |
|        - |  3976 | `				}` |
|        5 |  3977 | `			}` |
|       90 |  3978 | `		}else{` |
|        - |  3979 | `			/* Positional argument */` |
|       16 |  3980 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3981 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3982 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3983 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3984 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3985 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3986 | `					return PH7_ABORT;` |
|        - |  3987 | `				}` |
|       16 |  3988 | `				aSlot[i] = posIdx;` |
|       16 |  3989 | `				aUsed[posIdx] = 1;` |
|        7 |  3990 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3991 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3992 | `			}` |
|       16 |  3993 | `			posIdx++;` |
|        - |  3994 | `		}` |
|       97 |  3995 | `	}` |
|       93 |  3996 | `	return SXRET_OK;` |
|       51 |  3997 |  |
|        - |  3998 | `/*` |
|        - |  3999 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4000 | ` *` |
|        - |  4001 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4002 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4003 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4004 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4005 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4006 | ` * then the program execution is halted.` |
|        - |  4007 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4008 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4009 | ` * or to reset the VM to it's initial state.` |
|        - |  4010 | ` */` |
|    44834 |  4011 | `static sxi32 VmByteCodeExec(` |
|        - |  4012 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4013 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4014 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4015 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4016 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4017 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4018 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4019 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4020 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4021 | `	)` |
|        2 |  4022 |  |
|        - |  4023 | `	VmInstr *pInstr;` |
|        - |  4024 | `	ph7_value *pTos;` |
|        - |  4025 | `	SySet aArg;` |
|        - |  4026 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4027 | `	sxi32 pc;` |
|        - |  4028 | `	sxi32 rc;` |
|        - |  4029 | `	/* Argument container */` |
|    44836 |  4030 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    44836 |  4031 | `	if( nTos < 0 ){` |
|    41686 |  4032 | `		pTos = &pStack[-1];` |
|    20844 |  4033 | `	}else{` |
|     3152 |  4034 | `		pTos = &pStack[nTos];` |
|        - |  4035 | `	}` |
|    44836 |  4036 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    44836 |  4037 | `	pc = nPc;` |
|        - |  4038 | `/*` |
|        - |  4039 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4040 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4041 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4042 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4043 | ` */` |
|        - |  4044 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4045 | `	{ \` |
|        - |  4046 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4047 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4048 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4049 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4050 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4051 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4052 | `				break; \` |
|        - |  4053 | `			} \` |
|        - |  4054 | `			goto Exception; \` |
|        - |  4055 | `		} \` |
|        - |  4056 | `	}` |
|        - |  4057 | `	/* Execute as much as we can */` |
|  5896175 |  4058 | `	for(;;){` |
|        - |  4059 | `		/* Fetch the instruction to execute */` |
| 11791648 |  4060 | `		pInstr = &aInstr[pc];` |
| 11791648 |  4061 | `		rc = SXRET_OK;` |
|        - |  4062 | `/*` |
|        - |  4063 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4064 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4065 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4066 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4067 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4068 | ` */` |
| 11791648 |  4069 | `		switch(pInstr->iOp){` |
|        - |  4070 | `/*` |
|        - |  4071 | ` * DONE: P1 * *` |
|        - |  4072 | ` *` |
|        - |  4073 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4074 | ` * and return immediately.` |
|        - |  4075 | ` */` |
|    22039 |  4076 | `case PH7_OP_DONE:` |
|        - |  4077 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4078 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4079 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4080 | `	 * callback trampolines, and the main script. */` |
|    44078 |  4081 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      402 |  4082 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4083 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4084 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4085 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4086 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4087 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4088 | `		 * exception. */` |
|      398 |  4089 | `		ph7_value *pRetVal = 0;` |
|      398 |  4090 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      266 |  4091 | `			pRetVal = pTos;` |
|      132 |  4092 | `		}` |
|      398 |  4093 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      398 |  4094 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      392 |  4095 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4096 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4097 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4098 | `				pTos--;` |
|      ! 0 |  4099 | `			}` |
|      ! 0 |  4100 | `			goto Exception;` |
|        - |  4101 | `		}` |
|        - |  4102 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4103 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4104 | `		 * defensively we clear the pointer after a successful check). */` |
|      392 |  4105 | `		pEnforceRetFunc = 0;` |
|      195 |  4106 | `	}` |
|    44074 |  4107 | `	if( pInstr->iP1 ){` |
|        - |  4108 | `#ifdef UNTRUST` |
|        - |  4109 | `		if( pTos < pStack ){` |
|        - |  4110 | `			goto Abort;` |
|        - |  4111 | `		}` |
|        - |  4112 | `#endif` |
|    26782 |  4113 | `		if( pLastRef ){` |
|    16384 |  4114 | `			*pLastRef = pTos->nIdx;` |
|     8191 |  4115 | `		}` |
|    26782 |  4116 | `		if( pResult ){` |
|        - |  4117 | `			/* Execution result */` |
|    25306 |  4118 | `			PH7_MemObjStore(pTos,pResult);` |
|    12652 |  4119 | `		}` |
|    26782 |  4120 | `		VmPopOperand(&pTos,1);` |
|    30684 |  4121 | `	}else if( pLastRef ){` |
|        - |  4122 | `		/* Nothing referenced */` |
|     1936 |  4123 | `		*pLastRef = SXU32_HIGH;` |
|      967 |  4124 | `	}` |
|        - |  4125 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4126 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4127 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4128 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4129 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4130 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4131 | `	 * block can override it.` |
|        - |  4132 | `	 */` |
|    44076 |  4133 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4134 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4135 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4136 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4137 | `		pExc->pFrame = 0;` |
|        3 |  4138 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4139 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4140 | `			pExc->iFinallyDone = 1;` |
|        - |  4141 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4142 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4143 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4144 | `				goto Abort;` |
|        - |  4145 | `			}` |
|        1 |  4146 | `		}` |
|        1 |  4147 | `	}` |
|    44074 |  4148 | `	goto Done;` |
|        - |  4149 | `/*` |
|        - |  4150 | ` * HALT: P1 * *` |
|        - |  4151 | ` *` |
|        - |  4152 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4153 | ` * and abort immediately.` |
|        - |  4154 | ` */` |
|        7 |  4155 | `case PH7_OP_HALT:` |
|       15 |  4156 | `	if( pInstr->iP1 ){` |
|        - |  4157 | `#ifdef UNTRUST` |
|        - |  4158 | `		if( pTos < pStack ){` |
|        - |  4159 | `			goto Abort;` |
|        - |  4160 | `		}` |
|        - |  4161 | `#endif` |
|       15 |  4162 | `		if( pLastRef ){` |
|        3 |  4163 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4164 | `		}` |
|       15 |  4165 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       11 |  4166 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4167 | `				/* Output the exit message */` |
|       16 |  4168 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4169 | `					pVm->sVmConsumer.pUserData);` |
|       11 |  4170 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        6 |  4171 | `			}` |
|       10 |  4172 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4173 | `			/* Record exit status */` |
|        5 |  4174 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4175 | `		}` |
|       15 |  4176 | `		VmPopOperand(&pTos,1);` |
|        7 |  4177 | `	}else if( pLastRef ){` |
|        - |  4178 | `		/* Nothing referenced */` |
|      ! 0 |  4179 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4180 | `	}` |
|        - |  4181 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4182 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4183 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4184 | `	 */` |
|       15 |  4185 | `	pVm->bHaltRequested = 1;` |
|       15 |  4186 | `	goto Abort;` |
|        - |  4187 | `/*` |
|        - |  4188 | ` * JMP: * P2 *` |
|        - |  4189 | ` *` |
|        - |  4190 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4191 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4192 | ` */` |
|   251177 |  4193 | `case PH7_OP_JMP:` |
|   502400 |  4194 | `	pc = pInstr->iP2 - 1;` |
|   502400 |  4195 | `	break;` |
|        - |  4196 | `/*` |
|        - |  4197 | ` * JZ: P1 P2 *` |
|        - |  4198 | ` *` |
|        - |  4199 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4200 | ` * entry in the stack if P1 is zero.` |
|        - |  4201 | ` */` |
|   596373 |  4202 | `case PH7_OP_JZ:` |
|        - |  4203 | `#ifdef UNTRUST` |
|        - |  4204 | `	if( pTos < pStack ){` |
|        - |  4205 | `		goto Abort;` |
|        - |  4206 | `	}` |
|        - |  4207 | `#endif` |
|        - |  4208 | `	/* Get a boolean value */` |
|  1192836 |  4209 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      174 |  4210 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4211 | `	}` |
|  1192836 |  4212 | `	if( !pTos->x.iVal ){` |
|        - |  4213 | `		/* Take the jump */` |
|   613646 |  4214 | `		pc = pInstr->iP2 - 1;` |
|   306822 |  4215 | `	}` |
|  1192836 |  4216 | `	if( !pInstr->iP1 ){` |
|   945322 |  4217 | `		VmPopOperand(&pTos,1);` |
|   472682 |  4218 | `	}` |
|  1192836 |  4219 | `	break;` |
|        - |  4220 | `/*` |
|        - |  4221 | ` * JNZ: P1 P2 *` |
|        - |  4222 | ` *` |
|        - |  4223 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4224 | ` * entry in the stack if P1 is zero.` |
|        - |  4225 | ` */` |
|    61356 |  4226 | `case PH7_OP_JNZ:` |
|        - |  4227 | `#ifdef UNTRUST` |
|        - |  4228 | `	if( pTos < pStack ){` |
|        - |  4229 | `		goto Abort;` |
|        - |  4230 | `	}` |
|        - |  4231 | `#endif` |
|        - |  4232 | `	/* Get a boolean value */` |
|   122714 |  4233 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4234 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4235 | `	}` |
|   122714 |  4236 | `	if( pTos->x.iVal ){` |
|        - |  4237 | `		/* Take the jump */` |
|     5586 |  4238 | `		pc = pInstr->iP2 - 1;` |
|     2792 |  4239 | `	}` |
|   122714 |  4240 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4241 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4242 | `	}` |
|   122714 |  4243 | `	break;` |
|        - |  4244 | `/*` |
|        - |  4245 | ` * NOOP: * * *` |
|        - |  4246 | ` *` |
|        - |  4247 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4248 | ` * destination.` |
|        - |  4249 | ` */` |
|      ! 0 |  4250 | `case PH7_OP_NOOP:` |
|      ! 0 |  4251 | `	break;` |
|        - |  4252 | `/*` |
|        - |  4253 | ` * POP: P1 * *` |
|        - |  4254 | ` *` |
|        - |  4255 | ` * Pop P1 elements from the operand stack.` |
|        - |  4256 | ` */` |
|   462346 |  4257 | `case PH7_OP_POP: {` |
|   924738 |  4258 | `	sxi32 n = pInstr->iP1;` |
|   924738 |  4259 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4260 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4261 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4262 | `	}` |
|   924738 |  4263 | `	VmPopOperand(&pTos,n);` |
|   924738 |  4264 | `	break;` |
|        - |  4265 | `				 }` |
|        - |  4266 | `/*` |
|        - |  4267 | ` * DUP: * * *` |
|        - |  4268 | ` *` |
|        - |  4269 | ` * Duplicate the top of the stack.` |
|        - |  4270 | ` */` |
|       41 |  4271 | `case PH7_OP_DUP:` |
|        - |  4272 | `#ifdef UNTRUST` |
|        - |  4273 | `	if( pTos < pStack ){` |
|        - |  4274 | `		goto Abort;` |
|        - |  4275 | `	}` |
|        - |  4276 | `#endif` |
|       84 |  4277 | `	pTos++;` |
|       84 |  4278 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4279 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4280 | `	break;` |
|        - |  4281 | `/*` |
|        - |  4282 | ` * NSSWITCH: * * P3` |
|        - |  4283 | ` *` |
|        - |  4284 | ` * Switch the active namespace at runtime.` |
|        - |  4285 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4286 | ` */` |
|     7810 |  4287 | `case PH7_OP_NSSWITCH:` |
|    15622 |  4288 | `	SyBlobReset(&pVm->sNamespace);` |
|    15622 |  4289 | `	if( pInstr->p3 ){` |
|      100 |  4290 | `		const char *zNs = (const char *)pInstr->p3;` |
|      100 |  4291 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4292 | `	}` |
|        - |  4293 | `	/* Clear namespace-scoped use-const imports */` |
|    15622 |  4294 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15622 |  4295 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15622 |  4296 | `	break;` |
|        - |  4297 | `/* OP_USECONST P1 * P3` |
|        - |  4298 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4299 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4300 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4301 | ` */` |
|        7 |  4302 | `case PH7_OP_USECONST: {` |
|       16 |  4303 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4304 | `	if( azPair ){` |
|       16 |  4305 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4306 | `	}` |
|       16 |  4307 | `	break;` |
|        - |  4308 | `				}` |
|        - |  4309 | `/*` |
|        - |  4310 | ` * CVT_INT: * * *` |
|        - |  4311 | ` *` |
|        - |  4312 | ` * Force the top of the stack to be an integer.` |
|        - |  4313 | ` */` |
|       80 |  4314 | `case PH7_OP_CVT_INT:` |
|        - |  4315 | `#ifdef UNTRUST` |
|        - |  4316 | `	if( pTos < pStack ){` |
|        - |  4317 | `		goto Abort;` |
|        - |  4318 | `	}` |
|        - |  4319 | `#endif` |
|      162 |  4320 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4321 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4322 | `	}` |
|        - |  4323 | `	/* Invalidate any prior representation */` |
|      162 |  4324 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4325 | `	break;` |
|        - |  4326 | `/*` |
|        - |  4327 | ` * CVT_REAL: * * *` |
|        - |  4328 | ` *` |
|        - |  4329 | ` * Force the top of the stack to be a real.` |
|        - |  4330 | ` */` |
|        5 |  4331 | `case PH7_OP_CVT_REAL:` |
|        - |  4332 | `#ifdef UNTRUST` |
|        - |  4333 | `	if( pTos < pStack ){` |
|        - |  4334 | `		goto Abort;` |
|        - |  4335 | `	}` |
|        - |  4336 | `#endif` |
|       11 |  4337 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4338 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4339 | `	}` |
|        - |  4340 | `	/* Invalidate any prior representation */` |
|       11 |  4341 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4342 | `	break;` |
|        - |  4343 | `/*` |
|        - |  4344 | ` * CVT_STR: * * *` |
|        - |  4345 | ` *` |
|        - |  4346 | ` * Force the top of the stack to be a string.` |
|        - |  4347 | ` */` |
|      163 |  4348 | `case PH7_OP_CVT_STR:` |
|        - |  4349 | `#ifdef UNTRUST` |
|        - |  4350 | `	if( pTos < pStack ){` |
|        - |  4351 | `		goto Abort;` |
|        - |  4352 | `	}` |
|        - |  4353 | `#endif` |
|      328 |  4354 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      308 |  4355 | `		PH7_MemObjToString(pTos);` |
|      153 |  4356 | `	}` |
|      328 |  4357 | `	break;` |
|        - |  4358 | `/*` |
|        - |  4359 | ` * CVT_BOOL: * * *` |
|        - |  4360 | ` *` |
|        - |  4361 | ` * Force the top of the stack to be a boolean.` |
|        - |  4362 | ` */` |
|        5 |  4363 | `case PH7_OP_CVT_BOOL:` |
|        - |  4364 | `#ifdef UNTRUST` |
|        - |  4365 | `	if( pTos < pStack ){` |
|        - |  4366 | `		goto Abort;` |
|        - |  4367 | `	}` |
|        - |  4368 | `#endif` |
|       11 |  4369 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4370 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4371 | `	}` |
|       11 |  4372 | `	break;` |
|        - |  4373 | `/*` |
|        - |  4374 | ` * CVT_NULL: * * *` |
|        - |  4375 | ` *` |
|        - |  4376 | ` * Nullify the top of the stack.` |
|        - |  4377 | ` */` |
|        3 |  4378 | `case PH7_OP_CVT_NULL:` |
|        - |  4379 | `#ifdef UNTRUST` |
|        - |  4380 | `	if( pTos < pStack ){` |
|        - |  4381 | `		goto Abort;` |
|        - |  4382 | `	}` |
|        - |  4383 | `#endif` |
|        7 |  4384 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4385 | `	break;` |
|        - |  4386 | `/*` |
|        - |  4387 | ` * CVT_NUMC: * * *` |
|        - |  4388 | ` *` |
|        - |  4389 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4390 | ` */` |
|      ! 0 |  4391 | `case PH7_OP_CVT_NUMC:` |
|        - |  4392 | `#ifdef UNTRUST` |
|        - |  4393 | `	if( pTos < pStack ){` |
|        - |  4394 | `		goto Abort;` |
|        - |  4395 | `	}` |
|        - |  4396 | `#endif` |
|        - |  4397 | `	/* Force a numeric cast */` |
|      ! 0 |  4398 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4399 | `	break;` |
|        - |  4400 | `/*` |
|        - |  4401 | ` * CVT_ARRAY: * * *` |
|        - |  4402 | ` *` |
|        - |  4403 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4404 | ` */` |
|       10 |  4405 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4406 | `#ifdef UNTRUST` |
|        - |  4407 | `	if( pTos < pStack ){` |
|        - |  4408 | `		goto Abort;` |
|        - |  4409 | `	}` |
|        - |  4410 | `#endif` |
|        - |  4411 | `	/* Force a hashmap cast */` |
|       21 |  4412 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4413 | `	if( rc != SXRET_OK ){` |
|        - |  4414 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4415 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4416 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4417 | `	}` |
|       21 |  4418 | `	break;` |
|        - |  4419 | `/*` |
|        - |  4420 | ` * CVT_OBJ: * * *` |
|        - |  4421 | ` *` |
|        - |  4422 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4423 | ` */` |
|        8 |  4424 | `case PH7_OP_CVT_OBJ:` |
|        - |  4425 | `#ifdef UNTRUST` |
|        - |  4426 | `	if( pTos < pStack ){` |
|        - |  4427 | `		goto Abort;` |
|        - |  4428 | `	}` |
|        - |  4429 | `#endif` |
|       17 |  4430 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4431 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4432 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4433 | `	}` |
|       17 |  4434 | `	break;` |
|        - |  4435 | `/*` |
|        - |  4436 | ` * ERR_CTRL * * *` |
|        - |  4437 | ` *` |
|        - |  4438 | ` * Error control operator.` |
|        - |  4439 | ` */` |
|    16032 |  4440 | `case PH7_OP_ERR_CTRL:` |
|        - |  4441 | `	/*` |
|        - |  4442 | `	 * TICKET 1433-038:` |
|        - |  4443 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4444 | `	 * use the public API,to control error output.` |
|        - |  4445 | `	 */` |
|    32064 |  4446 | `	break;` |
|        - |  4447 | `/*` |
|        - |  4448 | ` * IS_A * * *` |
|        - |  4449 | ` *` |
|        - |  4450 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4451 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4452 | ` * holding a class name or an object).` |
|        - |  4453 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4454 | ` */` |
|       66 |  4455 | `case PH7_OP_IS_A:{` |
|      134 |  4456 | `	ph7_value *pNos = &pTos[-1];` |
|      134 |  4457 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4458 | `#ifdef UNTRUST` |
|        - |  4459 | `	if( pNos < pStack ){` |
|        - |  4460 | `		goto Abort;` |
|        - |  4461 | `	}` |
|        - |  4462 | `#endif` |
|      134 |  4463 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      132 |  4464 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      132 |  4465 | `		ph7_class *pClass = 0;` |
|        - |  4466 | `		/* Extract the target class */` |
|      132 |  4467 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4468 | `			/* Instance already loaded */` |
|      ! 0 |  4469 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      132 |  4470 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      132 |  4471 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      132 |  4472 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4473 | `			/* Handle self/static/parent keywords */` |
|      132 |  4474 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4475 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      130 |  4476 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4477 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      129 |  4478 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4479 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4480 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4481 | `					pClass = pSelf->pBase;` |
|        2 |  4482 | `				}` |
|        3 |  4483 | `			}else{` |
|      122 |  4484 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4485 | `			}` |
|       65 |  4486 | `		}` |
|      132 |  4487 | `		if( pClass ){` |
|        - |  4488 | `			/* Perform the query */` |
|      132 |  4489 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       65 |  4490 | `		}` |
|       65 |  4491 | `	}` |
|        - |  4492 | `	/* Push result */` |
|      134 |  4493 | `	VmPopOperand(&pTos,1);` |
|      134 |  4494 | `	PH7_MemObjRelease(pTos);` |
|      134 |  4495 | `	pTos->x.iVal = iRes;` |
|      134 |  4496 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      134 |  4497 | `	break;` |
|        - |  4498 | `				 }` |
|        - |  4499 |  |
|        - |  4500 | `/*` |
|        - |  4501 | ` * LOADC P1 P2 *` |
|        - |  4502 | ` *` |
|        - |  4503 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4504 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4505 | ` */` |
|  1013639 |  4506 | `case PH7_OP_LOADC: {` |
|        - |  4507 | `	ph7_value *pObj;` |
|        - |  4508 | `	/* Reserve a room */` |
|  2027324 |  4509 | `	pTos++;` |
|  3031158 |  4510 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2027324 |  4511 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4512 | `			SyHashEntry *pEntry;` |
|        - |  4513 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4514 | `			{` |
|        - |  4515 | `				SyHashEntry *pConstImport;` |
|    29555 |  4516 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19702 |  4517 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19704 |  4518 | `				if( pConstImport ){` |
|       11 |  4519 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4520 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4521 | `					if( pEntry ){` |
|       11 |  4522 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4523 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4524 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4525 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4526 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4527 | `						break;` |
|        - |  4528 | `					}` |
|        - |  4529 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4530 | `				}` |
|        - |  4531 | `			}` |
|        - |  4532 | `			/* Candidate for expansion via user defined callbacks */` |
|    19694 |  4533 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19694 |  4534 | `			if( pEntry ){` |
|    19688 |  4535 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4536 | `				/* Set a NULL default value */` |
|    19688 |  4537 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19688 |  4538 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4539 | `				/* Invoke the callback and deal with the expanded value */` |
|    19688 |  4540 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4541 | `				/* Mark as constant */` |
|    19688 |  4542 | `				pTos->nIdx = SXU32_HIGH;` |
|    19688 |  4543 | `				break;` |
|        - |  4544 | `			}` |
|        - |  4545 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4546 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4547 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4548 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4549 | `			{` |
|        8 |  4550 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4551 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4552 | `				sxu32 j;` |
|        8 |  4553 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4554 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4555 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4556 | `				}` |
|        8 |  4557 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4558 | `					/* Try current_namespace\name */` |
|      ! 0 |  4559 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4560 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4561 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4562 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4563 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4564 | `					if( pEntry ){` |
|      ! 0 |  4565 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4566 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4567 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4568 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4569 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4570 | `						break;` |
|        - |  4571 | `					}` |
|        - |  4572 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4573 | `				}` |
|        8 |  4574 | `				if( isQualified ){` |
|        - |  4575 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4576 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4577 | `					SyBlob sErr;` |
|        3 |  4578 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4579 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4580 | `					if( pErrFile ){` |
|        3 |  4581 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4582 | `					}` |
|        3 |  4583 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4584 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4585 | `					SyBlobRelease(&sErr);` |
|        3 |  4586 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4587 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4588 | `					goto LoadC_Done;` |
|        - |  4589 | `				}` |
|        - |  4590 | `			}` |
|        2 |  4591 | `		}` |
|  2007626 |  4592 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1003836 |  4593 | `	}else{` |
|        - |  4594 | `		/* Set a NULL value */` |
|      ! 0 |  4595 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4596 | `	}` |
|  1003791 |  4597 | `LoadC_Done:` |
|        - |  4598 | `	/* Mark as constant */` |
|  2007628 |  4599 | `	pTos->nIdx = SXU32_HIGH;` |
|  2007628 |  4600 | `	break;` |
|        - |  4601 | `				  }` |
|        - |  4602 | `/*` |
|        - |  4603 | ` * LOAD: P1 * P3` |
|        - |  4604 | ` *` |
|        - |  4605 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4606 | ` * from the P3 operand.` |
|        - |  4607 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4608 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4609 | ` */` |
|  1574940 |  4610 | `case PH7_OP_LOAD:{` |
|        - |  4611 | `	ph7_value *pObj;` |
|        - |  4612 | `	SyString sName;` |
|  3150102 |  4613 | `	if( pInstr->p3 == 0 ){` |
|        - |  4614 | `		/* Take the variable name from the top of the stack */` |
|        - |  4615 | `#ifdef UNTRUST` |
|        - |  4616 | `		if( pTos < pStack ){` |
|        - |  4617 | `			goto Abort;` |
|        - |  4618 | `		}` |
|        - |  4619 | `#endif` |
|        - |  4620 | `		/* Force a string cast */` |
|       19 |  4621 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4622 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4623 | `		}` |
|       19 |  4624 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4625 | `	}else{` |
|  3150084 |  4626 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4627 | `		/* Reserve a room for the target object */` |
|  3150084 |  4628 | `		pTos++;` |
|        - |  4629 | `	}` |
|        - |  4630 | `	/* Extract the requested memory object */` |
|  3150102 |  4631 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3150102 |  4632 | `	if( pObj == 0 ){` |
|      836 |  4633 | `		if( pInstr->iP1 ){` |
|        - |  4634 | `			/* Variable not found,load NULL */` |
|      836 |  4635 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4636 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4637 | `			}else{` |
|      836 |  4638 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4639 | `			}` |
|      836 |  4640 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1575359 |  4641 | `			break;` |
|      ! 0 |  4642 | `		}else{` |
|        - |  4643 | `			/* Fatal error */` |
|      ! 0 |  4644 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4645 | `			goto Abort;` |
|        - |  4646 | `		}` |
|        - |  4647 | `	}` |
|        - |  4648 | `	/* Load variable contents */` |
|  3149268 |  4649 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3149268 |  4650 | `	pTos->nIdx = pObj->nIdx;` |
|  3149268 |  4651 | `	break;` |
|        - |  4652 | `				   }` |
|        - |  4653 | `/*` |
|        - |  4654 | ` * LOAD_MAP P1 * *` |
|        - |  4655 | ` *` |
|        - |  4656 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4657 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4658 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4659 | ` */` |
|    22772 |  4660 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4661 | `	ph7_hashmap *pMap;` |
|        - |  4662 | `	/* Allocate a new hashmap instance */` |
|    45546 |  4663 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    45546 |  4664 | `	if( pMap == 0 ){` |
|      ! 0 |  4665 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4666 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4667 | `		goto Abort;` |
|        - |  4668 | `	}` |
|    45546 |  4669 | `	if( pInstr->iP1 > 0 ){` |
|     2780 |  4670 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2780 |  4671 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4672 | `		/* Perform the insertion */` |
|     8446 |  4673 | `		while( pEntry < pTos ){` |
|     5684 |  4674 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4675 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4676 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4677 | `				 * renumbered. Same routine that backs array_merge. */` |
|       70 |  4678 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  4679 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  4680 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4681 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4682 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4683 | `						 * map dangling. */` |
|      ! 0 |  4684 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4685 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4686 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4687 | `						break;` |
|        - |  4688 | `					}` |
|       27 |  4689 | `				}else{` |
|        - |  4690 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  4691 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  4692 | `					break;` |
|        1 |  4693 | `				}` |
|     5642 |  4694 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4695 | `				/* Insertion by reference */` |
|      151 |  4696 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  4697 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  4698 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4699 | `					);` |
|       51 |  4700 | `			}else{` |
|        - |  4701 | `				/* Standard insertion */` |
|     8273 |  4702 | `				PH7_HashmapInsert(pMap,` |
|     5514 |  4703 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2757 |  4704 | `					&pEntry[1]` |
|        - |  4705 | `				);` |
|        - |  4706 | `			}` |
|        - |  4707 | `			/* Next pair on the stack */` |
|     5668 |  4708 | `			pEntry += 2;` |
|        2 |  4709 | `		}` |
|        - |  4710 | `		/* Pop P1 elements */` |
|     2780 |  4711 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2780 |  4712 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4713 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  4714 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  4715 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4716 | `				goto Abort;` |
|        - |  4717 | `			}` |
|        - |  4718 | `			{` |
|       17 |  4719 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  4720 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  4721 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  4722 | `					break;` |
|        - |  4723 | `				}` |
|        - |  4724 | `			}` |
|       15 |  4725 | `			goto Exception;` |
|        - |  4726 | `		}` |
|     1381 |  4727 | `	}` |
|        - |  4728 | `	/* Push the hashmap */` |
|    45530 |  4729 | `	pTos++;` |
|    45530 |  4730 | `	pTos->nIdx = SXU32_HIGH;` |
|    45530 |  4731 | `	pTos->x.pOther = pMap;` |
|    45530 |  4732 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    45530 |  4733 | `	break;` |
|        - |  4734 | `					  }` |
|        - |  4735 | `/*` |
|        - |  4736 | ` * LOAD_LIST: P1 * *` |
|        - |  4737 | ` *` |
|        - |  4738 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4739 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4740 | ` * Caveats:` |
|        - |  4741 | ` *  This implementation support only a single nesting level.` |
|        - |  4742 | ` */` |
|       48 |  4743 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4744 | `	ph7_value *pEntry;` |
|       98 |  4745 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4746 | `		/* Empty list,break immediately */` |
|      ! 0 |  4747 | `		break;` |
|        - |  4748 | `	}` |
|       98 |  4749 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4750 | `#ifdef UNTRUST` |
|        - |  4751 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4752 | `		goto Abort;` |
|        - |  4753 | `	}` |
|        - |  4754 | `#endif` |
|       98 |  4755 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4756 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4757 | `		ph7_hashmap_node *pNode;` |
|        - |  4758 | `		ph7_value sKey,*pObj;` |
|        - |  4759 | `		/* Start Copying */` |
|       91 |  4760 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4761 | `		while( pEntry <= pTos ){` |
|      193 |  4762 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4763 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4764 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4765 | `					if( rc == SXRET_OK ){` |
|        - |  4766 | `						/* Store node value */` |
|      165 |  4767 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4768 | `					}else{` |
|        - |  4769 | `						/* Undefined array key */` |
|        - |  4770 | `						char zMsg[128];` |
|      ! 0 |  4771 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4772 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4773 | `						PH7_MemObjRelease(pObj);` |
|        - |  4774 | `					}` |
|       82 |  4775 | `				}` |
|       82 |  4776 | `			}` |
|      193 |  4777 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4778 | `			pEntry++;` |
|        1 |  4779 | `		}` |
|       46 |  4780 | `	}else{` |
|        - |  4781 | `		/* Source is not an array */` |
|        - |  4782 | `		ph7_value *pObj;` |
|       18 |  4783 | `		while( pEntry <= pTos ){` |
|       12 |  4784 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4785 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4786 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4787 | `				}` |
|        5 |  4788 | `			}` |
|       12 |  4789 | `			pEntry++;` |
|        2 |  4790 | `		}` |
|        8 |  4791 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4792 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4793 | `			const char *zType = "unknown";` |
|        3 |  4794 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4795 | `			char zMsg[256];` |
|        3 |  4796 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4797 | `				zType = "string";` |
|        1 |  4798 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4799 | `				zType = "int";` |
|      ! 0 |  4800 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4801 | `				zType = "float";` |
|      ! 0 |  4802 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4803 | `				zType = "object";` |
|      ! 0 |  4804 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4805 | `				zType = "resource";` |
|      ! 0 |  4806 | `			}` |
|        3 |  4807 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4808 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4809 | `		}` |
|        - |  4810 | `	}` |
|       98 |  4811 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4812 | `	break;` |
|        - |  4813 | `					   }` |
|        - |  4814 | `/*` |
|        - |  4815 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4816 | ` *` |
|        - |  4817 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4818 | ` * from the stack.` |
|        - |  4819 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4820 | ` * instead.` |
|        - |  4821 | ` */` |
|   250617 |  4822 | `case PH7_OP_LOAD_IDX: {` |
|   501280 |  4823 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   501280 |  4824 | `	ph7_hashmap *pMap = 0;` |
|        - |  4825 | `	ph7_value *pIdx;` |
|   501280 |  4826 | `	pIdx = 0;` |
|   501280 |  4827 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4828 | `		if( !pInstr->iP2){` |
|        - |  4829 | `			/* No available index,load NULL */` |
|      ! 0 |  4830 | `			if( pTos >= pStack ){` |
|      ! 0 |  4831 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4832 | `			}else{` |
|        - |  4833 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4834 | `				pTos++;` |
|      ! 0 |  4835 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4836 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4837 | `			}` |
|        - |  4838 | `			/* Emit a notice */` |
|      ! 0 |  4839 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4840 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4841 | `			break;` |
|        - |  4842 | `		}` |
|      ! 0 |  4843 | `	}else{` |
|   501280 |  4844 | `		pIdx = pTos;` |
|   501280 |  4845 | `		pTos--;` |
|        - |  4846 | `	}` |
|   501280 |  4847 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4848 | `		/* String access */` |
|   387716 |  4849 | `		if( pIdx ){` |
|        - |  4850 | `			sxu32 nOfft;` |
|   387716 |  4851 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4852 | `				/* Force an int cast */` |
|      ! 0 |  4853 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4854 | `			}` |
|   387716 |  4855 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  4856 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4857 | `				/* Invalid offset,load null */` |
|      ! 0 |  4858 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4859 | `			}else{` |
|   387716 |  4860 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  4861 | `				int c = zData[nOfft];` |
|   387716 |  4862 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  4863 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  4864 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4865 | `			}` |
|   193881 |  4866 | `		}else{` |
|        - |  4867 | `			/* No available index,load NULL */` |
|      ! 0 |  4868 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4869 | `		}` |
|   387716 |  4870 | `		break;` |
|        - |  4871 | `	}` |
|   113566 |  4872 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4873 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  4874 | `		 * iP2 codes:` |
|        - |  4875 | `		 *   0 = read       → offsetGet` |
|        - |  4876 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  4877 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  4878 | `		 *   4 = isset()    → offsetExists` |
|        - |  4879 | `		 *   5 = unset()    → offsetUnset` |
|        - |  4880 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  4881 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  4882 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  4883 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  4884 | `			ph7_class_method *pMeth;` |
|        - |  4885 | `			ph7_value sResult;` |
|        - |  4886 | `			ph7_value *apArg[1];` |
|      124 |  4887 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  4888 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  4889 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4890 | `					"Cannot use [] for reading");` |
|      ! 0 |  4891 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4892 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4893 | `				break;` |
|        - |  4894 | `			}` |
|      124 |  4895 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  4896 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  4897 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  4898 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4899 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  4900 | `				apArg[0] = pIdx;` |
|       51 |  4901 | `				if( pMeth ){` |
|       51 |  4902 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  4903 | `				}` |
|       99 |  4904 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  4905 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4906 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  4907 | `				apArg[0] = pIdx;` |
|        9 |  4908 | `				if( pMeth ){` |
|        9 |  4909 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  4910 | `				}` |
|        5 |  4911 | `			}else{` |
|       66 |  4912 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4913 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  4914 | `				apArg[0] = pIdx;` |
|       66 |  4915 | `				if( pMeth ){` |
|       66 |  4916 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  4917 | `				}` |
|        - |  4918 | `			}` |
|      124 |  4919 | `			if( pInstr->iP2 == 4 ){` |
|        - |  4920 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  4921 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  4922 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  4923 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  4924 | `				PH7_MemObjRelease(pTos);` |
|       33 |  4925 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  4926 | `				if( bExists ){` |
|       17 |  4927 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  4928 | `					pTos->x.iVal = 1;` |
|        9 |  4929 | `				}else{` |
|       17 |  4930 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  4931 | `				}` |
|      108 |  4932 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  4933 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  4934 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  4935 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4936 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4937 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  4938 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  4939 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  4940 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  4941 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  4942 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  4943 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  4944 | `				PH7_MemObjRelease(pTos);` |
|       11 |  4945 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  4946 | `				if( !bExists ){` |
|        3 |  4947 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  4948 | `				}else{` |
|        9 |  4949 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4950 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  4951 | `					ph7_value sValue;` |
|        9 |  4952 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4953 | `					apArg[0] = pIdx;` |
|        9 |  4954 | `					if( pGet ){` |
|        9 |  4955 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  4956 | `					}` |
|        9 |  4957 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  4958 | `					PH7_MemObjRelease(&sValue);` |
|        - |  4959 | `				}` |
|       11 |  4960 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  4961 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  4962 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  4963 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  4964 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  4965 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  4966 | `				 *     and push NULL.` |
|        - |  4967 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  4968 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  4969 | `				int bShouldArm = !bExists;` |
|        - |  4970 | `				ph7_value sValue;` |
|        9 |  4971 | `				PH7_MemObjRelease(&sResult);` |
|        - |  4972 | `				/* Reset any prior arming defensively */` |
|        9 |  4973 | `				VmCoalesceDisarm(pVm);` |
|        9 |  4974 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4975 | `				if( bExists ){` |
|        5 |  4976 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4977 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  4978 | `					apArg[0] = pIdx;` |
|        5 |  4979 | `					if( pGet ){` |
|        5 |  4980 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  4981 | `					}` |
|        5 |  4982 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  4983 | `						bShouldArm = 1;` |
|        1 |  4984 | `					}` |
|        2 |  4985 | `				}` |
|        9 |  4986 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4987 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4988 | `				if( bShouldArm ){` |
|        - |  4989 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  4990 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  4991 | `					 * intervening expression evaluation. */` |
|        7 |  4992 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  4993 | `					if( pIdx ){` |
|        7 |  4994 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  4995 | `					}` |
|        7 |  4996 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  4997 | `					pInst->iRef++;` |
|        7 |  4998 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  4999 | `				}else{` |
|        3 |  5000 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5001 | `				}` |
|        9 |  5002 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  5003 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  5004 | `				break;` |
|      ! 0 |  5005 | `			}else{` |
|        - |  5006 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  5007 | `				PH7_MemObjRelease(pTos);` |
|       66 |  5008 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  5009 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5010 | `			}` |
|      106 |  5011 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  5012 | `			if( pIdx ){` |
|      106 |  5013 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5014 | `			}` |
|      106 |  5015 | `			break;` |
|        - |  5016 | `		}` |
|        - |  5017 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5018 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5019 | `		if( pInst ){` |
|        - |  5020 | `			char zMsg[256];` |
|        3 |  5021 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5022 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5023 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5024 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5025 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5026 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5027 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5028 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5029 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5030 | `			break;` |
|        - |  5031 | `		}` |
|      ! 0 |  5032 | `	}` |
|   113442 |  5033 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5034 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5035 | `			ph7_value *pObj;` |
|        3 |  5036 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5037 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5038 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5039 | `			}` |
|        1 |  5040 | `		}` |
|        1 |  5041 | `	}` |
|   113442 |  5042 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   113442 |  5043 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   113442 |  5044 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5045 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5046 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5047 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5048 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5049 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5050 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  5051 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  5052 | `		}` |
|        - |  5053 | `		/* Point to the hashmap */` |
|   113442 |  5054 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   113442 |  5055 | `		if( pIdx ){` |
|        - |  5056 | `			/* Load the desired entry */` |
|   113442 |  5057 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    56720 |  5058 | `		}` |
|   113442 |  5059 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5060 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5061 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5062 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5063 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5064 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5065 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5066 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5067 | `			 * correct for the outermost write. */` |
|       19 |  5068 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5069 | `			if( !needWrite && pNode ){` |
|       13 |  5070 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5071 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5072 | `					needWrite = 1;` |
|        3 |  5073 | `				}` |
|        6 |  5074 | `			}` |
|       19 |  5075 | `			if( needWrite ){` |
|       13 |  5076 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5077 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5078 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5079 | `					 * into the new map's storage. */` |
|        7 |  5080 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5081 | `					if( pIdx ){` |
|        7 |  5082 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5083 | `					}` |
|        3 |  5084 | `				}` |
|        6 |  5085 | `			}` |
|        9 |  5086 | `		}` |
|   113442 |  5087 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5088 | `			/* Create a new empty entry */` |
|      273 |  5089 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5090 | `			if( rc == SXRET_OK ){` |
|        - |  5091 | `				/* Point to the last inserted entry */` |
|      273 |  5092 | `				pNode = pMap->pLast;` |
|      136 |  5093 | `			}` |
|      136 |  5094 | `		}` |
|    56720 |  5095 | `	}` |
|   113442 |  5096 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5097 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5098 | `		char zMsg[128];` |
|      ! 0 |  5099 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5100 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5101 | `		}` |
|      ! 0 |  5102 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5103 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5104 | `	}` |
|   113442 |  5105 | `	if( pIdx ){` |
|   113442 |  5106 | `		PH7_MemObjRelease(pIdx);` |
|    56720 |  5107 | `	}` |
|   113442 |  5108 | `	if( rc == SXRET_OK ){` |
|        - |  5109 | `		/* Load entry contents */` |
|    50304 |  5110 | `		if( pMap->iRef < 2 ){` |
|        - |  5111 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5112 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5113 | `			 */` |
|       28 |  5114 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5115 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5116 | `		}else{` |
|    50278 |  5117 | `			pTos->nIdx = pNode->nValIdx;` |
|    50278 |  5118 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50278 |  5119 | `			PH7_HashmapUnref(pMap);` |
|        - |  5120 | `		}` |
|    25153 |  5121 | `	}else{` |
|        - |  5122 | `		/* No such entry,load NULL */` |
|    63140 |  5123 | `		PH7_MemObjRelease(pTos);` |
|    63140 |  5124 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5125 | `	}` |
|   113442 |  5126 | `	break;` |
|        - |  5127 | `					  }` |
|        - |  5128 | `/*` |
|        - |  5129 | ` * LOAD_CLOSURE * * P3` |
|        - |  5130 | ` *` |
|        - |  5131 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5132 | ` * name in the stack.` |
|        - |  5133 | ` */` |
|       61 |  5134 | `case PH7_OP_LOAD_CLOSURE:{` |
|      124 |  5135 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      124 |  5136 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5137 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5138 | `		ph7_vm_func *pClosure;` |
|        - |  5139 | `		char *zName;` |
|        - |  5140 | `		sxu32 mLen;` |
|        - |  5141 | `		sxu32 n;` |
|        - |  5142 | `		/* Create a new VM function */` |
|      124 |  5143 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5144 | `		/* Generate an unique closure name */` |
|      124 |  5145 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      124 |  5146 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5147 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5148 | `			goto Abort;` |
|        - |  5149 | `		}` |
|      124 |  5150 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      124 |  5151 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5152 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5153 | `		}` |
|        - |  5154 | `		/* Zero the stucture */` |
|      124 |  5155 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5156 | `		/* Perform a structure assignment on read-only items */` |
|      124 |  5157 | `		pClosure->aArgs = pFunc->aArgs;` |
|      124 |  5158 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      124 |  5159 | `		pClosure->aStatic = pFunc->aStatic;` |
|      124 |  5160 | `		pClosure->iFlags = pFunc->iFlags;` |
|      124 |  5161 | `		pClosure->pUserData = pFunc->pUserData;` |
|      124 |  5162 | `		pClosure->sSignature = pFunc->sSignature;` |
|      124 |  5163 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      124 |  5164 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      124 |  5165 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      124 |  5166 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      124 |  5167 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5168 | `		/* Register the closure */` |
|      124 |  5169 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5170 | `		/* Set up closure environment */` |
|      124 |  5171 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      124 |  5172 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      312 |  5173 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5174 | `			ph7_value *pValue;` |
|      190 |  5175 | `			pEnv = &aEnv[n];` |
|      190 |  5176 | `			sEnv.sName  = pEnv->sName;` |
|      190 |  5177 | `			sEnv.iFlags = pEnv->iFlags;` |
|      190 |  5178 | `			sEnv.nIdx = SXU32_HIGH;` |
|      190 |  5179 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      190 |  5180 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5181 | `				/* Pass by reference */` |
|      ! 0 |  5182 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5183 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5184 | `					);` |
|      ! 0 |  5185 | `			}` |
|        - |  5186 | `			/* Standard pass by value */` |
|      190 |  5187 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      190 |  5188 | `			if( pValue ){` |
|        - |  5189 | `				/* Copy imported value */` |
|       72 |  5190 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5191 | `			}` |
|        - |  5192 | `			/* Insert the imported variable */` |
|      190 |  5193 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       96 |  5194 | `		}` |
|        - |  5195 | `		/* Finally,load the closure name on the stack */` |
|      124 |  5196 | `		pTos++;` |
|      124 |  5197 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       61 |  5198 | `	}` |
|      124 |  5199 | `	break;` |
|        - |  5200 | `						 }` |
|        - |  5201 | `/*` |
|        - |  5202 | ` * STORE * P2 P3` |
|        - |  5203 | ` *` |
|        - |  5204 | ` * Perform a store (Assignment) operation.` |
|        - |  5205 | ` */` |
|   145880 |  5206 | `case PH7_OP_STORE: {` |
|        - |  5207 | `	ph7_value *pObj;` |
|        - |  5208 | `	SyString sName;` |
|        - |  5209 | `#ifdef UNTRUST` |
|        - |  5210 | `	if( pTos < pStack ){` |
|        - |  5211 | `		goto Abort;` |
|        - |  5212 | `	}` |
|        - |  5213 | `#endif` |
|   291762 |  5214 | `	if( pInstr->iP2 ){` |
|        - |  5215 | `		sxu32 nIdx;` |
|        - |  5216 | `		sxi32 rcT;` |
|        - |  5217 | `		/* Member store operation */` |
|     5168 |  5218 | `		nIdx = pTos->nIdx;` |
|     5168 |  5219 | `		VmPopOperand(&pTos,1);` |
|     5168 |  5220 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5221 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5222 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5223 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5224 | `		}else{` |
|        - |  5225 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5226 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5164 |  5227 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5164 |  5228 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5229 | `				goto Abort;` |
|        - |  5230 | `			}` |
|     5164 |  5231 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5232 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5233 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5234 | `				 * propagate out of the VM loop. */` |
|       37 |  5235 | `				VmPopOperand(&pTos,1);` |
|        - |  5236 | `				{` |
|       37 |  5237 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5238 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5239 | `						pc = pFrm2->iExceptionJump - 1;` |
|   145899 |  5240 | `						break;` |
|        - |  5241 | `					}` |
|        - |  5242 | `				}` |
|      ! 0 |  5243 | `				goto Exception;` |
|        - |  5244 | `			}` |
|        - |  5245 | `			/* Point to the desired memory object */` |
|     5128 |  5246 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5128 |  5247 | `			if( pObj ){` |
|        - |  5248 | `				/* Perform the store operation */` |
|     5128 |  5249 | `				PH7_MemObjStore(pTos,pObj);` |
|     2563 |  5250 | `			}` |
|        - |  5251 | `		}` |
|     5132 |  5252 | `		break;` |
|   286596 |  5253 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5254 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5255 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5256 | `			/* Force a string cast */` |
|      ! 0 |  5257 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5258 | `		}` |
|        7 |  5259 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5260 | `		pTos--;` |
|        - |  5261 | `#ifdef UNTRUST` |
|        - |  5262 | `		if( pTos < pStack  ){` |
|        - |  5263 | `			goto Abort;` |
|        - |  5264 | `		}` |
|        - |  5265 | `#endif` |
|        4 |  5266 | `	}else{` |
|   286590 |  5267 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5268 | `	}` |
|        - |  5269 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   286596 |  5270 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   286596 |  5271 | `	if( pObj == 0 ){` |
|      ! 0 |  5272 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5273 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5274 | `		goto Abort;` |
|        - |  5275 | `	}` |
|   286596 |  5276 | `	if( !pInstr->p3 ){` |
|        7 |  5277 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5278 | `	}` |
|        - |  5279 | `	/* Perform the store operation */` |
|   286596 |  5280 | `	PH7_MemObjStore(pTos,pObj);` |
|   286596 |  5281 | `	break;` |
|        - |  5282 | `				   }` |
|        - |  5283 | `/*` |
|        - |  5284 | ` * STORE_IDX:   P1 * P3` |
|        - |  5285 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5286 | ` *` |
|        - |  5287 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5288 | ` */` |
|    96879 |  5289 | `case PH7_OP_STORE_IDX:` |
|        - |  5290 | `case PH7_OP_STORE_IDX_REF: {` |
|   193760 |  5291 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5292 | `	ph7_value *pKey;` |
|        - |  5293 | `	sxu32 nIdx;` |
|   193760 |  5294 | `	if( pInstr->iP1 ){` |
|        - |  5295 | `		/* Key is next on stack */` |
|    63288 |  5296 | `		pKey = pTos;` |
|    63288 |  5297 | `		pTos--;` |
|    31645 |  5298 | `	}else{` |
|   130474 |  5299 | `		pKey = 0;` |
|        - |  5300 | `	}` |
|   193760 |  5301 | `	nIdx = pTos->nIdx;` |
|        - |  5302 | `	{` |
|        - |  5303 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5304 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5305 | `		 * the backing variable slot at nIdx. */` |
|   193760 |  5306 | `		ph7_class_instance *pInst = 0;` |
|   193760 |  5307 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5308 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   193744 |  5309 | `		}else if( nIdx != SXU32_HIGH ){` |
|   193728 |  5310 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   193728 |  5311 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5312 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5313 | `			}` |
|    96863 |  5314 | `		}` |
|   193760 |  5315 | `		if( pInst ){` |
|       34 |  5316 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5317 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5318 | `				ph7_class_method *pMeth;` |
|        - |  5319 | `				ph7_value sNullKey;` |
|        - |  5320 | `				ph7_value *apArg[2];` |
|       32 |  5321 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5322 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5323 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5324 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5325 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5326 | `					break;` |
|        - |  5327 | `				}` |
|       32 |  5328 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5329 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5330 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5331 | `				VmPopOperand(&pTos,1);` |
|       32 |  5332 | `				if( pKey == 0 ){` |
|        7 |  5333 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5334 | `					apArg[0] = &sNullKey;` |
|        4 |  5335 | `				}else{` |
|       26 |  5336 | `					apArg[0] = pKey;` |
|        - |  5337 | `				}` |
|       32 |  5338 | `				apArg[1] = pTos;` |
|       32 |  5339 | `				if( pMeth ){` |
|       32 |  5340 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5341 | `				}` |
|       32 |  5342 | `				if( pKey ){` |
|       26 |  5343 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5344 | `				}else{` |
|        7 |  5345 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5346 | `				}` |
|        - |  5347 | `				/* Pop the value */` |
|       32 |  5348 | `				VmPopOperand(&pTos,1);` |
|       32 |  5349 | `				break;` |
|        - |  5350 | `			}` |
|        - |  5351 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5352 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5353 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5354 | `			 * a few lines below). Match PHP. */` |
|        - |  5355 | `			{` |
|        - |  5356 | `				char zMsg[256];` |
|        3 |  5357 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5358 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5359 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5360 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5361 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5362 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5363 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5364 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5365 | `				break;` |
|        - |  5366 | `			}` |
|        - |  5367 | `		}` |
|        - |  5368 | `	}` |
|   193728 |  5369 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5370 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5371 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5372 | `		 * checking true sharing count, then re-add after separation. */` |
|   193676 |  5373 | `		if( nIdx != SXU32_HIGH ){` |
|   193676 |  5374 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   290513 |  5375 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   193676 |  5376 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5377 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5378 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5379 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5380 | `				 * refcounts if the backing array was already separated. */` |
|   193676 |  5381 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   193676 |  5382 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   193676 |  5383 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   193676 |  5384 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   193676 |  5385 | `					pTos->x.pOther = pMap;` |
|    96839 |  5386 | `				}else{` |
|        - |  5387 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5388 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5389 | `					pMap = pCur;` |
|        - |  5390 | `				}` |
|    96839 |  5391 | `			}else{` |
|      ! 0 |  5392 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5393 | `			}` |
|    96839 |  5394 | `		}else{` |
|      ! 0 |  5395 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5396 | `		}` |
|   193676 |  5397 | `		if( pMap->iRef < 2 ){` |
|        - |  5398 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5399 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5400 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5401 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5402 | `			pMap->iRef = 2;` |
|      ! 0 |  5403 | `		}` |
|    96839 |  5404 | `	}else{` |
|        - |  5405 | `		ph7_value *pObj;` |
|       53 |  5406 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5407 | `		if( pObj == 0 ){` |
|      ! 0 |  5408 | `			if( pKey ){` |
|      ! 0 |  5409 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5410 | `			}` |
|      ! 0 |  5411 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5412 | `			break;` |
|        - |  5413 | `		}` |
|        - |  5414 | `		/* Phase#1: Load the array */` |
|       53 |  5415 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5416 | `			VmPopOperand(&pTos,1);` |
|       53 |  5417 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5418 | `				/* Force a string cast */` |
|      ! 0 |  5419 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5420 | `			}` |
|       53 |  5421 | `			if( pKey == 0 ){` |
|        - |  5422 | `				/* Append string */` |
|        3 |  5423 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5424 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5425 | `				}` |
|        2 |  5426 | `			}else{` |
|        - |  5427 | `				sxu32 nOfft;` |
|       51 |  5428 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5429 | `					/* Force an int cast */` |
|       51 |  5430 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5431 | `				}` |
|       51 |  5432 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5433 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5434 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5435 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5436 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5437 | `				}else{` |
|      ! 0 |  5438 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5439 | `						/* Perform an append operation */` |
|      ! 0 |  5440 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5441 | `					}` |
|        - |  5442 | `				}` |
|        - |  5443 | `			}` |
|       53 |  5444 | `			if( pKey ){` |
|       51 |  5445 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5446 | `			}` |
|       53 |  5447 | `			break;` |
|      ! 0 |  5448 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5449 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5450 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5451 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5452 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5453 | `				goto Abort;` |
|        - |  5454 | `			}` |
|      ! 0 |  5455 | `		}` |
|        - |  5456 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5457 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5458 | `	}` |
|   193676 |  5459 | `	VmPopOperand(&pTos,1);` |
|        - |  5460 | `	/* Phase#2: Perform the insertion */` |
|   193676 |  5461 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5462 | `		/* Insertion by reference */` |
|       15 |  5463 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5464 | `	}else{` |
|   193662 |  5465 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5466 | `	}` |
|   193676 |  5467 | `	if( pKey ){` |
|    63212 |  5468 | `		PH7_MemObjRelease(pKey);` |
|    31605 |  5469 | `	}` |
|   193676 |  5470 | `	break;` |
|        - |  5471 | `					   }` |
|        - |  5472 | `/*` |
|        - |  5473 | ` * INCR: P1 * *` |
|        - |  5474 | ` *` |
|        - |  5475 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5476 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5477 | ` * the stack and increment after that.` |
|        - |  5478 | ` */` |
|   167681 |  5479 | `case PH7_OP_INCR:` |
|        - |  5480 | `#ifdef UNTRUST` |
|        - |  5481 | `	if( pTos < pStack ){` |
|        - |  5482 | `		goto Abort;` |
|        - |  5483 | `	}` |
|        - |  5484 | `#endif` |
|   335408 |  5485 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335408 |  5486 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5487 | `			ph7_value *pObj;` |
|   335408 |  5488 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335408 |  5489 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5490 | `					/* Perl-style string increment.` |
|        - |  5491 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5492 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5493 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5494 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5495 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5496 | `					}` |
|       49 |  5497 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5498 | `					if( pInstr->iP1 ){` |
|        - |  5499 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5500 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5501 | `					}` |
|       25 |  5502 | `				}else{` |
|        - |  5503 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5504 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5505 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5506 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5507 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5508 | `					 * so its old-value view survives the coercion. */` |
|   335360 |  5509 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5510 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5511 | `					}` |
|        - |  5512 | `					/* Force a numeric cast on the variable */` |
|   335360 |  5513 | `					PH7_MemObjToNumeric(pObj);` |
|   335360 |  5514 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5515 | `						pObj->rVal++;` |
|        - |  5516 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5517 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5518 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5519 | `						 * integer-valued real. */` |
|        9 |  5520 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5521 | `					}else{` |
|   335352 |  5522 | `						pObj->x.iVal++;` |
|        - |  5523 | `					}` |
|   335360 |  5524 | `					if( pInstr->iP1 ){` |
|        - |  5525 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5526 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5527 | `					}` |
|        - |  5528 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5529 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5530 | `				}` |
|   167725 |  5531 | `			}` |
|   167727 |  5532 | `		}else{` |
|      ! 0 |  5533 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5534 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5535 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5536 | `				}else{` |
|        - |  5537 | `					/* Force a numeric cast */` |
|      ! 0 |  5538 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5539 | `					/* Pre-increment */` |
|      ! 0 |  5540 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5541 | `						pTos->rVal++;` |
|        - |  5542 | `						/* Try to get an integer representation */` |
|      ! 0 |  5543 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5544 | `					}else{` |
|      ! 0 |  5545 | `						pTos->x.iVal++;` |
|      ! 0 |  5546 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5547 | `					}` |
|        - |  5548 | `				}` |
|      ! 0 |  5549 | `			}` |
|        - |  5550 | `		}` |
|   167725 |  5551 | `	}` |
|   335408 |  5552 | `	break;` |
|        - |  5553 | `/*` |
|        - |  5554 | ` * DECR: P1 * *` |
|        - |  5555 | ` *` |
|        - |  5556 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5557 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5558 | ` * and decrement after that.` |
|        - |  5559 | ` */` |
|       14 |  5560 | `case PH7_OP_DECR:` |
|        - |  5561 | `#ifdef UNTRUST` |
|        - |  5562 | `	if( pTos < pStack ){` |
|        - |  5563 | `		goto Abort;` |
|        - |  5564 | `	}` |
|        - |  5565 | `#endif` |
|        - |  5566 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5567 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5568 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5569 | `			ph7_value *pObj;` |
|       27 |  5570 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5571 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5572 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5573 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5574 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5575 | `					if( pInstr->iP1 ){` |
|        - |  5576 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5577 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5578 | `					}` |
|        - |  5579 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5580 | `				}else{` |
|        - |  5581 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5582 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5583 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5584 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5585 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5586 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5587 | `					}` |
|       21 |  5588 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5589 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5590 | `						pObj->rVal--;` |
|        - |  5591 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5592 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5593 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5594 | `						 * integer-valued real. */` |
|        9 |  5595 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5596 | `					}else{` |
|       13 |  5597 | `						pObj->x.iVal--;` |
|        - |  5598 | `					}` |
|       21 |  5599 | `					if( pInstr->iP1 ){` |
|        - |  5600 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5601 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5602 | `					}` |
|        - |  5603 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5604 | `				}` |
|       13 |  5605 | `			}` |
|       14 |  5606 | `		}else{` |
|      ! 0 |  5607 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5608 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5609 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5610 | `				}else{` |
|        - |  5611 | `					/* Force a numeric cast */` |
|      ! 0 |  5612 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5613 | `					/* Pre-decrement */` |
|      ! 0 |  5614 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5615 | `						pTos->rVal--;` |
|        - |  5616 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5617 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5618 | `					}else{` |
|      ! 0 |  5619 | `						pTos->x.iVal--;` |
|      ! 0 |  5620 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5621 | `					}` |
|        - |  5622 | `				}` |
|      ! 0 |  5623 | `			}` |
|        - |  5624 | `		}` |
|       13 |  5625 | `	}` |
|       29 |  5626 | `	break;` |
|        - |  5627 | `/*` |
|        - |  5628 | ` * UMINUS: * * *` |
|        - |  5629 | ` *` |
|        - |  5630 | ` * Perform a unary minus operation.` |
|        - |  5631 | ` */` |
|    29683 |  5632 | `case PH7_OP_UMINUS:` |
|        - |  5633 | `#ifdef UNTRUST` |
|        - |  5634 | `	if( pTos < pStack ){` |
|        - |  5635 | `		goto Abort;` |
|        - |  5636 | `	}` |
|        - |  5637 | `#endif` |
|        - |  5638 | `	/* Force a numeric (integer,real or both) cast */` |
|    59368 |  5639 | `	PH7_MemObjToNumeric(pTos);` |
|    59368 |  5640 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5641 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5642 | `	}` |
|    59368 |  5643 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    59338 |  5644 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29668 |  5645 | `	}` |
|    59368 |  5646 | `	break;` |
|        - |  5647 | `/*` |
|        - |  5648 | ` * UPLUS: * * *` |
|        - |  5649 | ` *` |
|        - |  5650 | ` * Perform a unary plus operation.` |
|        - |  5651 | ` */` |
|       18 |  5652 | `case PH7_OP_UPLUS:` |
|        - |  5653 | `#ifdef UNTRUST` |
|        - |  5654 | `	if( pTos < pStack ){` |
|        - |  5655 | `		goto Abort;` |
|        - |  5656 | `	}` |
|        - |  5657 | `#endif` |
|        - |  5658 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5659 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5660 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5661 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5662 | `	}` |
|       37 |  5663 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5664 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5665 | `	}` |
|       37 |  5666 | `	break;` |
|        - |  5667 | `/*` |
|        - |  5668 | ` * OP_LNOT: * * *` |
|        - |  5669 | ` *` |
|        - |  5670 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5671 | ` * with its complement.` |
|        - |  5672 | ` */` |
|    44826 |  5673 | `case PH7_OP_LNOT:` |
|        - |  5674 | `#ifdef UNTRUST` |
|        - |  5675 | `	if( pTos < pStack ){` |
|        - |  5676 | `		goto Abort;` |
|        - |  5677 | `	}` |
|        - |  5678 | `#endif` |
|        - |  5679 | `	/* Force a boolean cast */` |
|    89698 |  5680 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5681 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5682 | `	}` |
|    89698 |  5683 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89698 |  5684 | `	break;` |
|        - |  5685 | `/*` |
|        - |  5686 | ` * OP_BITNOT: * * *` |
|        - |  5687 | ` *` |
|        - |  5688 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5689 | ` * with its ones-complement.` |
|        - |  5690 | ` */` |
|       14 |  5691 | `case PH7_OP_BITNOT:` |
|        - |  5692 | `#ifdef UNTRUST` |
|        - |  5693 | `	if( pTos < pStack ){` |
|        - |  5694 | `		goto Abort;` |
|        - |  5695 | `	}` |
|        - |  5696 | `#endif` |
|        - |  5697 | `	/* Force an integer cast */` |
|       30 |  5698 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5699 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5700 | `	}` |
|       30 |  5701 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  5702 | `	break;` |
|        - |  5703 | `/* OP_MUL * * *` |
|        - |  5704 | ` * OP_MUL_STORE * * *` |
|        - |  5705 | ` *` |
|        - |  5706 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5707 | ` * and push the result back onto the stack.` |
|        - |  5708 | ` */` |
|     1290 |  5709 | `case PH7_OP_MUL:` |
|        - |  5710 | `case PH7_OP_MUL_STORE: {` |
|     2582 |  5711 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5712 | `	/* Force the operand to be numeric */` |
|        - |  5713 | `#ifdef UNTRUST` |
|        - |  5714 | `	if( pNos < pStack ){` |
|        - |  5715 | `		goto Abort;` |
|        - |  5716 | `	}` |
|        - |  5717 | `#endif` |
|     2582 |  5718 | `	PH7_MemObjToNumeric(pTos);` |
|     2582 |  5719 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5720 | `	/* Perform the requested operation */` |
|     2582 |  5721 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5722 | `		/* Floating point arithemic */` |
|        - |  5723 | `		ph7_real a,b,r;` |
|       21 |  5724 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5725 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5726 | `		}` |
|       21 |  5727 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5728 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5729 | `		}` |
|       21 |  5730 | `		a = pNos->rVal;` |
|       21 |  5731 | `		b = pTos->rVal;` |
|       21 |  5732 | `		r = a * b;` |
|        - |  5733 | `		/* Push the result */` |
|       21 |  5734 | `		pNos->rVal = r;` |
|       21 |  5735 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5736 | `		/* Try to get an integer representation */` |
|       21 |  5737 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  5738 | `	}else{` |
|        - |  5739 | `		/* Integer arithmetic */` |
|        - |  5740 | `		sxi64 a,b,r;` |
|     2562 |  5741 | `		a = pNos->x.iVal;` |
|     2562 |  5742 | `		b = pTos->x.iVal;` |
|     2562 |  5743 | `		r = a * b;` |
|        - |  5744 | `		/* Push the result */` |
|     2562 |  5745 | `		pNos->x.iVal = r;` |
|     2562 |  5746 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5747 | `	}` |
|     2582 |  5748 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5749 | `		ph7_value *pObj;` |
|       32 |  5750 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5751 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5752 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5753 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5754 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5755 | `		}` |
|       15 |  5756 | `	}` |
|     2582 |  5757 | `	VmPopOperand(&pTos,1);` |
|     2582 |  5758 | `	break;` |
|        - |  5759 | `				 }` |
|        - |  5760 | `/* OP_POW * * *` |
|        - |  5761 | ` * OP_POW_STORE * * *` |
|        - |  5762 | ` *` |
|        - |  5763 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5764 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5765 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5766 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5767 | ` */` |
|       67 |  5768 | `case PH7_OP_POW:` |
|        - |  5769 | `case PH7_OP_POW_STORE: {` |
|      135 |  5770 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  5771 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5772 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5773 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5774 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5775 | `	 */` |
|      135 |  5776 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  5777 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5778 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5779 | `	int bBothInt;` |
|      135 |  5780 | `	int usedInt = 0;` |
|        - |  5781 | `	ph7_real a, b, r;` |
|        - |  5782 | `#endif` |
|      135 |  5783 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5784 | `#ifdef UNTRUST` |
|        - |  5785 | `	if( pNos < pStack ){` |
|        - |  5786 | `		goto Abort;` |
|        - |  5787 | `	}` |
|        - |  5788 | `#endif` |
|      135 |  5789 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  5790 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5791 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  5792 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  5793 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  5794 | `	if( bBothInt ){` |
|      123 |  5795 | `		base_i = pBase->x.iVal;` |
|      123 |  5796 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5797 | `	}` |
|      135 |  5798 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5799 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5800 | `	}` |
|      135 |  5801 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  5802 | `		PH7_MemObjToReal(pExp);` |
|       66 |  5803 | `	}` |
|      135 |  5804 | `	a = pBase->rVal;` |
|      135 |  5805 | `	b = pExp->rVal;` |
|      135 |  5806 | `	r = pow(a, b);` |
|        - |  5807 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5808 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5809 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5810 | `	 * representable as double but not as signed int64. */` |
|      135 |  5811 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5812 | `		sxi64 result_i = 1;` |
|      117 |  5813 | `		sxi64 cur_base = base_i;` |
|      117 |  5814 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5815 | `		int overflow = 0;` |
|      401 |  5816 | `		while( cur_exp > 0 ){` |
|      289 |  5817 | `			if( cur_exp & 1 ){` |
|      189 |  5818 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5819 | `					overflow = 1;` |
|        3 |  5820 | `					break;` |
|        - |  5821 | `				}` |
|       93 |  5822 | `			}` |
|      287 |  5823 | `			cur_exp >>= 1;` |
|      287 |  5824 | `			if( cur_exp > 0 ){` |
|      181 |  5825 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5826 | `					overflow = 1;` |
|        3 |  5827 | `					break;` |
|        - |  5828 | `				}` |
|       89 |  5829 | `			}` |
|        1 |  5830 | `		}` |
|      117 |  5831 | `		if( !overflow ){` |
|      113 |  5832 | `			pNos->x.iVal = result_i;` |
|      113 |  5833 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5834 | `			usedInt = 1;` |
|       56 |  5835 | `		}` |
|       58 |  5836 | `	}` |
|      135 |  5837 | `	if( !usedInt ){` |
|       23 |  5838 | `		pNos->rVal = r;` |
|       23 |  5839 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  5840 | `	}` |
|        - |  5841 | `#else` |
|        - |  5842 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5843 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5844 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5845 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5846 | `	 * represented. */` |
|        - |  5847 | `	base_i = pBase->x.iVal;` |
|        - |  5848 | `	exp_i  = pExp->x.iVal;` |
|        - |  5849 | `	{` |
|        - |  5850 | `		sxi64 result_i = 1;` |
|        - |  5851 | `		sxi64 cur_base = base_i;` |
|        - |  5852 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5853 | `		if( cur_exp < 0 ){` |
|        - |  5854 | `			result_i = 0;` |
|        - |  5855 | `		}else{` |
|        - |  5856 | `			while( cur_exp > 0 ){` |
|        - |  5857 | `				if( cur_exp & 1 ){` |
|        - |  5858 | `					result_i *= cur_base;` |
|        - |  5859 | `				}` |
|        - |  5860 | `				cur_exp >>= 1;` |
|        - |  5861 | `				if( cur_exp > 0 ){` |
|        - |  5862 | `					cur_base *= cur_base;` |
|        - |  5863 | `				}` |
|        - |  5864 | `			}` |
|        - |  5865 | `		}` |
|        - |  5866 | `		pNos->x.iVal = result_i;` |
|        - |  5867 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5868 | `	}` |
|        - |  5869 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  5870 | `	if( bStore ){` |
|        - |  5871 | `		ph7_value *pObj;` |
|       23 |  5872 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5873 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5874 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5875 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5876 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5877 | `		}` |
|       11 |  5878 | `	}` |
|      135 |  5879 | `	VmPopOperand(&pTos,1);` |
|      135 |  5880 | `	break;` |
|        - |  5881 | `				 }` |
|        - |  5882 | `/* OP_ADD * * *` |
|        - |  5883 | ` *` |
|        - |  5884 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5885 | ` * and push the result back onto the stack.` |
|        - |  5886 | ` */` |
|      528 |  5887 | `case PH7_OP_ADD:{` |
|     1058 |  5888 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5889 | `#ifdef UNTRUST` |
|        - |  5890 | `	if( pNos < pStack ){` |
|        - |  5891 | `		goto Abort;` |
|        - |  5892 | `	}` |
|        - |  5893 | `#endif` |
|        - |  5894 | `	/* Perform the addition */` |
|     1058 |  5895 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1058 |  5896 | `	VmPopOperand(&pTos,1);` |
|     1058 |  5897 | `	break;` |
|        - |  5898 | `				}` |
|        - |  5899 | `/*` |
|        - |  5900 | ` * OP_ADD_STORE * * *` |
|        - |  5901 | ` *` |
|        - |  5902 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5903 | ` * and push the result back onto the stack.` |
|        - |  5904 | ` */` |
|      502 |  5905 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5906 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5907 | `	ph7_value *pObj;` |
|        - |  5908 | `	sxu32 nIdx;` |
|        - |  5909 | `#ifdef UNTRUST` |
|        - |  5910 | `	if( pNos < pStack ){` |
|        - |  5911 | `		goto Abort;` |
|        - |  5912 | `	}` |
|        - |  5913 | `#endif` |
|        - |  5914 | `	/* Perform the addition */` |
|     1006 |  5915 | `	nIdx = pTos->nIdx;` |
|     1006 |  5916 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5917 | `	/* Peform the store operation */` |
|     1006 |  5918 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5919 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5920 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5921 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5922 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5923 | `	}` |
|        - |  5924 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5925 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5926 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5927 | `	break;` |
|        - |  5928 | `				}` |
|        - |  5929 | `/* OP_SUB * * *` |
|        - |  5930 | ` *` |
|        - |  5931 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5932 | ` * first (what was next on the stack) from the second (the` |
|        - |  5933 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5934 | ` */` |
|      349 |  5935 | `case PH7_OP_SUB: {` |
|      700 |  5936 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5937 | `#ifdef UNTRUST` |
|        - |  5938 | `	if( pNos < pStack ){` |
|        - |  5939 | `		goto Abort;` |
|        - |  5940 | `	}` |
|        - |  5941 | `#endif` |
|      700 |  5942 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5943 | `		/* Floating point arithemic */` |
|        - |  5944 | `		ph7_real a,b,r;` |
|       97 |  5945 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5946 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5947 | `		}` |
|       97 |  5948 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5949 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5950 | `		}` |
|       97 |  5951 | `		a = pNos->rVal;` |
|       97 |  5952 | `		b = pTos->rVal;` |
|       97 |  5953 | `		r = a - b;` |
|        - |  5954 | `		/* Push the result */` |
|       97 |  5955 | `		pNos->rVal = r;` |
|       97 |  5956 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5957 | `		/* Try to get an integer representation */` |
|       97 |  5958 | `		PH7_MemObjTryInteger(pNos);` |
|       49 |  5959 | `	}else{` |
|        - |  5960 | `		/* Integer arithmetic */` |
|        - |  5961 | `		sxi64 a,b,r;` |
|      604 |  5962 | `		a = pNos->x.iVal;` |
|      604 |  5963 | `		b = pTos->x.iVal;` |
|      604 |  5964 | `		r = a - b;` |
|        - |  5965 | `		/* Push the result */` |
|      604 |  5966 | `		pNos->x.iVal = r;` |
|      604 |  5967 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5968 | `	}` |
|      700 |  5969 | `	VmPopOperand(&pTos,1);` |
|      700 |  5970 | `	break;` |
|        - |  5971 | `				 }` |
|        - |  5972 | `/* OP_SUB_STORE * * *` |
|        - |  5973 | ` *` |
|        - |  5974 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5975 | ` * first (what was next on the stack) from the second (the` |
|        - |  5976 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5977 | ` */` |
|        4 |  5978 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5979 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5980 | `	ph7_value *pObj;` |
|        - |  5981 | `#ifdef UNTRUST` |
|        - |  5982 | `	if( pNos < pStack ){` |
|        - |  5983 | `		goto Abort;` |
|        - |  5984 | `	}` |
|        - |  5985 | `#endif` |
|       10 |  5986 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5987 | `		/* Floating point arithemic */` |
|        - |  5988 | `		ph7_real a,b,r;` |
|      ! 0 |  5989 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5990 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5991 | `		}` |
|      ! 0 |  5992 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5993 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5994 | `		}` |
|      ! 0 |  5995 | `		a = pTos->rVal;` |
|      ! 0 |  5996 | `		b = pNos->rVal;` |
|      ! 0 |  5997 | `		r = a - b;` |
|        - |  5998 | `		/* Push the result */` |
|      ! 0 |  5999 | `		pNos->rVal = r;` |
|      ! 0 |  6000 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6001 | `		/* Try to get an integer representation */` |
|      ! 0 |  6002 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6003 | `	}else{` |
|        - |  6004 | `		/* Integer arithmetic */` |
|        - |  6005 | `		sxi64 a,b,r;` |
|       10 |  6006 | `		a = pTos->x.iVal;` |
|       10 |  6007 | `		b = pNos->x.iVal;` |
|       10 |  6008 | `		r = a - b;` |
|        - |  6009 | `		/* Push the result */` |
|       10 |  6010 | `		pNos->x.iVal = r;` |
|       10 |  6011 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6012 | `	}` |
|       10 |  6013 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6014 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6015 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6016 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6017 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6018 | `	}` |
|       10 |  6019 | `	VmPopOperand(&pTos,1);` |
|       10 |  6020 | `	break;` |
|        - |  6021 | `				 }` |
|        - |  6022 |  |
|        - |  6023 | `/*` |
|        - |  6024 | ` * OP_MOD * * *` |
|        - |  6025 | ` *` |
|        - |  6026 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6027 | ` * first (what was next on the stack) from the second (the` |
|        - |  6028 | ` * top of the stack) and push the remainder after division` |
|        - |  6029 | ` * onto the stack.` |
|        - |  6030 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6031 | ` */` |
|      308 |  6032 | `case PH7_OP_MOD:{` |
|      618 |  6033 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6034 | `	sxi64 a,b,r;` |
|        - |  6035 | `#ifdef UNTRUST` |
|        - |  6036 | `	if( pNos < pStack ){` |
|        - |  6037 | `		goto Abort;` |
|        - |  6038 | `	}` |
|        - |  6039 | `#endif` |
|        - |  6040 | `	/* Force the operands to be integer */` |
|      618 |  6041 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6042 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6043 | `	}` |
|      618 |  6044 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6045 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6046 | `	}` |
|        - |  6047 | `	/* Perform the requested operation */` |
|      618 |  6048 | `	a = pNos->x.iVal;` |
|      618 |  6049 | `	b = pTos->x.iVal;` |
|      618 |  6050 | `	if( b == 0 ){` |
|        3 |  6051 | `		r = 0;` |
|        3 |  6052 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6053 | `		/* goto Abort; */` |
|        2 |  6054 | `	}else{` |
|      615 |  6055 | `		r = a%b;` |
|        - |  6056 | `	}` |
|        - |  6057 | `	/* Push the result */` |
|      618 |  6058 | `	pNos->x.iVal = r;` |
|      618 |  6059 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  6060 | `	VmPopOperand(&pTos,1);` |
|      618 |  6061 | `	break;` |
|        - |  6062 | `				}` |
|        - |  6063 | `/*` |
|        - |  6064 | ` * OP_MOD_STORE * * *` |
|        - |  6065 | ` *` |
|        - |  6066 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6067 | ` * first (what was next on the stack) from the second (the` |
|        - |  6068 | ` * top of the stack) and push the remainder after division` |
|        - |  6069 | ` * onto the stack.` |
|        - |  6070 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6071 | ` */` |
|        1 |  6072 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6073 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6074 | `	ph7_value *pObj;` |
|        - |  6075 | `	sxi64 a,b,r;` |
|        - |  6076 | `#ifdef UNTRUST` |
|        - |  6077 | `	if( pNos < pStack ){` |
|        - |  6078 | `		goto Abort;` |
|        - |  6079 | `	}` |
|        - |  6080 | `#endif` |
|        - |  6081 | `	/* Force the operands to be integer */` |
|        3 |  6082 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6083 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6084 | `	}` |
|        3 |  6085 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6086 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6087 | `	}` |
|        - |  6088 | `	/* Perform the requested operation */` |
|        3 |  6089 | `	a = pTos->x.iVal;` |
|        3 |  6090 | `	b = pNos->x.iVal;` |
|        3 |  6091 | `	if( b == 0 ){` |
|      ! 0 |  6092 | `		r = 0;` |
|      ! 0 |  6093 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6094 | `		/* goto Abort; */` |
|      ! 0 |  6095 | `	}else{` |
|        3 |  6096 | `		r = a%b;` |
|        - |  6097 | `	}` |
|        - |  6098 | `	/* Push the result */` |
|        3 |  6099 | `	pNos->x.iVal = r;` |
|        3 |  6100 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6101 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6102 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6103 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6104 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6105 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6106 | `	}` |
|        3 |  6107 | `	VmPopOperand(&pTos,1);` |
|        3 |  6108 | `	break;` |
|        - |  6109 | `				}` |
|        - |  6110 | `/*` |
|        - |  6111 | ` * OP_DIV * * *` |
|        - |  6112 | ` *` |
|        - |  6113 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6114 | ` * first (what was next on the stack) from the second (the` |
|        - |  6115 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6116 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6117 | ` */` |
|       33 |  6118 | `case PH7_OP_DIV:{` |
|       68 |  6119 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6120 | `	ph7_real a,b,r;` |
|        - |  6121 | `#ifdef UNTRUST` |
|        - |  6122 | `	if( pNos < pStack ){` |
|        - |  6123 | `		goto Abort;` |
|        - |  6124 | `	}` |
|        - |  6125 | `#endif` |
|        - |  6126 | `	/* Force the operands to be real */` |
|       68 |  6127 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6128 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6129 | `	}` |
|       68 |  6130 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6131 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6132 | `	}` |
|        - |  6133 | `	/* Perform the requested operation */` |
|       68 |  6134 | `	a = pNos->rVal;` |
|       68 |  6135 | `	b = pTos->rVal;` |
|       68 |  6136 | `	if( b == 0 ){` |
|        - |  6137 | `		/* Division by zero */` |
|        3 |  6138 | `		pNos->rVal = 0;` |
|        3 |  6139 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6140 | `		/* goto Abort; */` |
|        2 |  6141 | `	}else{` |
|       65 |  6142 | `		r = a/b;` |
|        - |  6143 | `		/* Push the result */` |
|       65 |  6144 | `		pNos->rVal = r;` |
|       65 |  6145 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6146 | `		/* Try to get an integer representation */` |
|       65 |  6147 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6148 | `	}` |
|       68 |  6149 | `	VmPopOperand(&pTos,1);` |
|       68 |  6150 | `	break;` |
|        - |  6151 | `				}` |
|        - |  6152 | `/*` |
|        - |  6153 | ` * OP_DIV_STORE * * *` |
|        - |  6154 | ` *` |
|        - |  6155 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6156 | ` * first (what was next on the stack) from the second (the` |
|        - |  6157 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6158 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6159 | ` */` |
|        2 |  6160 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6161 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6162 | `	ph7_value *pObj;` |
|        - |  6163 | `	ph7_real a,b,r;` |
|        - |  6164 | `#ifdef UNTRUST` |
|        - |  6165 | `	if( pNos < pStack ){` |
|        - |  6166 | `		goto Abort;` |
|        - |  6167 | `	}` |
|        - |  6168 | `#endif` |
|        - |  6169 | `	/* Force the operands to be real */` |
|        5 |  6170 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6171 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6172 | `	}` |
|        5 |  6173 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6174 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6175 | `	}` |
|        - |  6176 | `	/* Perform the requested operation */` |
|        5 |  6177 | `	a = pTos->rVal;` |
|        5 |  6178 | `	b = pNos->rVal;` |
|        5 |  6179 | `	if( b == 0 ){` |
|        - |  6180 | `		/* Division by zero */` |
|      ! 0 |  6181 | `		r = 0;` |
|      ! 0 |  6182 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6183 | `		/* goto Abort; */` |
|      ! 0 |  6184 | `	}else{` |
|        5 |  6185 | `		r = a/b;` |
|        - |  6186 | `		/* Push the result */` |
|        5 |  6187 | `		pNos->rVal = r;` |
|        5 |  6188 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6189 | `		/* Try to get an integer representation */` |
|        5 |  6190 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6191 | `	}` |
|        5 |  6192 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6193 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6194 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6195 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6196 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6197 | `	}` |
|        5 |  6198 | `	VmPopOperand(&pTos,1);` |
|        5 |  6199 | `	break;` |
|        - |  6200 | `				}` |
|        - |  6201 | `/* OP_BAND * * *` |
|        - |  6202 | ` *` |
|        - |  6203 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6204 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6205 | ` * two elements.` |
|        - |  6206 | `*/` |
|        - |  6207 | `/* OP_BOR * * *` |
|        - |  6208 | ` *` |
|        - |  6209 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6210 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6211 | ` * two elements.` |
|        - |  6212 | ` */` |
|        - |  6213 | `/* OP_BXOR * * *` |
|        - |  6214 | ` *` |
|        - |  6215 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6216 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6217 | ` * two elements.` |
|        - |  6218 | ` */` |
|       43 |  6219 | `case PH7_OP_BAND:` |
|        - |  6220 | `case PH7_OP_BOR:` |
|        - |  6221 | `case PH7_OP_BXOR:{` |
|       88 |  6222 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6223 | `	sxi64 a,b,r;` |
|        - |  6224 | `#ifdef UNTRUST` |
|        - |  6225 | `	if( pNos < pStack ){` |
|        - |  6226 | `		goto Abort;` |
|        - |  6227 | `	}` |
|        - |  6228 | `#endif` |
|        - |  6229 | `	/* Force the operands to be integer */` |
|       88 |  6230 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6231 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6232 | `	}` |
|       88 |  6233 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6234 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6235 | `	}` |
|        - |  6236 | `	/* Perform the requested operation */` |
|       88 |  6237 | `	a = pNos->x.iVal;` |
|       88 |  6238 | `	b = pTos->x.iVal;` |
|       88 |  6239 | `	switch(pInstr->iOp){` |
|        7 |  6240 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6241 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6242 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6243 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6244 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6245 | `	case PH7_OP_BAND:` |
|       60 |  6246 | `	default:          r = a&b; break;` |
|        - |  6247 | `	}` |
|        - |  6248 | `	/* Push the result */` |
|       88 |  6249 | `	pNos->x.iVal = r;` |
|       88 |  6250 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       88 |  6251 | `	VmPopOperand(&pTos,1);` |
|       88 |  6252 | `	break;` |
|        - |  6253 | `				 }` |
|        - |  6254 | `/* OP_BAND_STORE * * *` |
|        - |  6255 | ` *` |
|        - |  6256 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6257 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6258 | ` * two elements.` |
|        - |  6259 | `*/` |
|        - |  6260 | `/* OP_BOR_STORE * * *` |
|        - |  6261 | ` *` |
|        - |  6262 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6263 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6264 | ` * two elements.` |
|        - |  6265 | ` */` |
|        - |  6266 | `/* OP_BXOR_STORE * * *` |
|        - |  6267 | ` *` |
|        - |  6268 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6269 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6270 | ` * two elements.` |
|        - |  6271 | ` */` |
|       10 |  6272 | `case PH7_OP_BAND_STORE:` |
|        - |  6273 | `case PH7_OP_BOR_STORE:` |
|        - |  6274 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6275 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6276 | `	ph7_value *pObj;` |
|        - |  6277 | `	sxi64 a,b,r;` |
|        - |  6278 | `#ifdef UNTRUST` |
|        - |  6279 | `	if( pNos < pStack ){` |
|        - |  6280 | `		goto Abort;` |
|        - |  6281 | `	}` |
|        - |  6282 | `#endif` |
|        - |  6283 | `	/* Force the operands to be integer */` |
|       21 |  6284 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6285 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6286 | `	}` |
|       21 |  6287 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6288 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6289 | `	}` |
|        - |  6290 | `	/* Perform the requested operation */` |
|       21 |  6291 | `	a = pTos->x.iVal;` |
|       21 |  6292 | `	b = pNos->x.iVal;` |
|       21 |  6293 | `	switch(pInstr->iOp){` |
|        3 |  6294 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6295 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6296 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6297 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6298 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6299 | `	case PH7_OP_BAND:` |
|        7 |  6300 | `	default:          r = a&b; break;` |
|        - |  6301 | `	}` |
|        - |  6302 | `	/* Push the result */` |
|       21 |  6303 | `	pNos->x.iVal = r;` |
|       21 |  6304 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6305 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6306 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6307 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6308 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6309 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6310 | `	}` |
|       21 |  6311 | `	VmPopOperand(&pTos,1);` |
|       21 |  6312 | `	break;` |
|        - |  6313 | `				 }` |
|        - |  6314 | `/* OP_SHL * * *` |
|        - |  6315 | ` *` |
|        - |  6316 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6317 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6318 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6319 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6320 | ` */` |
|        - |  6321 | `/* OP_SHR * * *` |
|        - |  6322 | ` *` |
|        - |  6323 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6324 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6325 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6326 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6327 | ` */` |
|       12 |  6328 | `case PH7_OP_SHL:` |
|        - |  6329 | `case PH7_OP_SHR: {` |
|       25 |  6330 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6331 | `	sxi64 a,r;` |
|        - |  6332 | `	sxi32 b;` |
|        - |  6333 | `#ifdef UNTRUST` |
|        - |  6334 | `	if( pNos < pStack ){` |
|        - |  6335 | `		goto Abort;` |
|        - |  6336 | `	}` |
|        - |  6337 | `#endif` |
|        - |  6338 | `	/* Force the operands to be integer */` |
|       25 |  6339 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6340 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6341 | `	}` |
|       25 |  6342 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6343 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6344 | `	}` |
|        - |  6345 | `	/* Perform the requested operation */` |
|       25 |  6346 | `	a = pNos->x.iVal;` |
|       25 |  6347 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6348 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6349 | `		r = a << b;` |
|        8 |  6350 | `	}else{` |
|       11 |  6351 | `		r = a >> b;` |
|        - |  6352 | `	}` |
|        - |  6353 | `	/* Push the result */` |
|       25 |  6354 | `	pNos->x.iVal = r;` |
|       25 |  6355 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6356 | `	VmPopOperand(&pTos,1);` |
|       25 |  6357 | `	break;` |
|        - |  6358 | `				 }` |
|        - |  6359 | `/*  OP_SHL_STORE * * *` |
|        - |  6360 | ` *` |
|        - |  6361 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6362 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6363 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6364 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6365 | ` */` |
|        - |  6366 | `/* OP_SHR_STORE * * *` |
|        - |  6367 | ` *` |
|        - |  6368 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6369 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6370 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6371 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6372 | ` */` |
|        9 |  6373 | `case PH7_OP_SHL_STORE:` |
|        - |  6374 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6375 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6376 | `	ph7_value *pObj;` |
|        - |  6377 | `	sxi64 a,r;` |
|        - |  6378 | `	sxi32 b;` |
|        - |  6379 | `#ifdef UNTRUST` |
|        - |  6380 | `	if( pNos < pStack ){` |
|        - |  6381 | `		goto Abort;` |
|        - |  6382 | `	}` |
|        - |  6383 | `#endif` |
|        - |  6384 | `	/* Force the operands to be integer */` |
|       19 |  6385 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6386 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6387 | `	}` |
|       19 |  6388 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6389 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6390 | `	}` |
|        - |  6391 | `	/* Perform the requested operation */` |
|       19 |  6392 | `	a = pTos->x.iVal;` |
|       19 |  6393 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6394 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6395 | `		r = a << b;` |
|        5 |  6396 | `	}else{` |
|       11 |  6397 | `		r = a >> b;` |
|        - |  6398 | `	}` |
|        - |  6399 | `	/* Push the result */` |
|       19 |  6400 | `	pNos->x.iVal = r;` |
|       19 |  6401 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6402 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6403 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6404 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6405 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6406 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6407 | `	}` |
|       19 |  6408 | `	VmPopOperand(&pTos,1);` |
|       19 |  6409 | `	break;` |
|        - |  6410 | `				 }` |
|        - |  6411 | `/* CAT:  P1 * *` |
|        - |  6412 | ` *` |
|        - |  6413 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6414 | ` * back.` |
|        - |  6415 | ` */` |
|    71743 |  6416 | `case PH7_OP_CAT:{` |
|        - |  6417 | `	ph7_value *pNos,*pCur;` |
|   143488 |  6418 | `	if( pInstr->iP1 < 1 ){` |
|   116008 |  6419 | `		pNos = &pTos[-1];` |
|    58005 |  6420 | `	}else{` |
|    27482 |  6421 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6422 | `	}` |
|        - |  6423 | `#ifdef UNTRUST` |
|        - |  6424 | `	if( pNos < pStack ){` |
|        - |  6425 | `		goto Abort;` |
|        - |  6426 | `	}` |
|        - |  6427 | `#endif` |
|        - |  6428 | `	/* Force a string cast */` |
|   143488 |  6429 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1672 |  6430 | `		PH7_MemObjToString(pNos);` |
|      835 |  6431 | `	}` |
|   143488 |  6432 | `	pCur = &pNos[1];` |
|   289698 |  6433 | `	while( pCur <= pTos ){` |
|   146212 |  6434 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50918 |  6435 | `			PH7_MemObjToString(pCur);` |
|    25458 |  6436 | `		}` |
|        - |  6437 | `		/* Perform the concatenation */` |
|   146212 |  6438 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   146168 |  6439 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6440 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6441 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6442 | `				goto Abort;` |
|        - |  6443 | `			}` |
|    73083 |  6444 | `		}` |
|   146212 |  6445 | `		SyBlobRelease(&pCur->sBlob);` |
|   146212 |  6446 | `		pCur++;` |
|        2 |  6447 | `	}` |
|   143488 |  6448 | `	pTos = pNos;` |
|   143488 |  6449 | `	break;` |
|        - |  6450 | `				}` |
|        - |  6451 | `/*  CAT_STORE: * * *` |
|        - |  6452 | ` *` |
|        - |  6453 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6454 | ` * back.` |
|        - |  6455 | ` */` |
|     4112 |  6456 | `case PH7_OP_CAT_STORE:{` |
|     8226 |  6457 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6458 | `	ph7_value *pObj;` |
|        - |  6459 | `#ifdef UNTRUST` |
|        - |  6460 | `	if( pNos < pStack ){` |
|        - |  6461 | `		goto Abort;` |
|        - |  6462 | `	}` |
|        - |  6463 | `#endif` |
|     8226 |  6464 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6465 | `		/* Force a string cast */` |
|        3 |  6466 | `		PH7_MemObjToString(pTos);` |
|        1 |  6467 | `	}` |
|     8226 |  6468 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6469 | `		/* Force a string cast */` |
|      ! 0 |  6470 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6471 | `	}` |
|        - |  6472 | `	/* Perform the concatenation (Reverse order) */` |
|     8226 |  6473 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8226 |  6474 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  6475 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  6476 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  6477 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6478 | `			goto Abort;` |
|        - |  6479 | `		}` |
|     4112 |  6480 | `	}` |
|        - |  6481 | `	/* Perform the store operation */` |
|     8226 |  6482 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6483 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8226 |  6484 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8226 |  6485 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8224 |  6486 | `		PH7_MemObjStore(pTos,pObj);` |
|     4111 |  6487 | `	}` |
|     8224 |  6488 | `	PH7_MemObjStore(pTos,pNos);` |
|     8224 |  6489 | `	VmPopOperand(&pTos,1);` |
|     8224 |  6490 | `	break;` |
|        - |  6491 | `				}` |
|        - |  6492 | `/* OP_AND: * * *` |
|        - |  6493 | ` *` |
|        - |  6494 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6495 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6496 | ` * stack.` |
|        - |  6497 | ` */` |
|        - |  6498 | `/* OP_OR: * * *` |
|        - |  6499 | ` *` |
|        - |  6500 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6501 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6502 | ` * stack.` |
|        - |  6503 | ` */` |
|   108208 |  6504 | `case PH7_OP_LAND:` |
|        - |  6505 | `case PH7_OP_LOR: {` |
|   216462 |  6506 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6507 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6508 | `#ifdef UNTRUST` |
|        - |  6509 | `	if( pNos < pStack ){` |
|        - |  6510 | `		goto Abort;` |
|        - |  6511 | `	}` |
|        - |  6512 | `#endif` |
|        - |  6513 | `	/* Force a boolean cast */` |
|   216462 |  6514 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6515 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6516 | `	}` |
|   216462 |  6517 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6518 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6519 | `	}` |
|   216462 |  6520 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   216462 |  6521 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   216462 |  6522 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6523 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    99336 |  6524 | `		v1 = and_logic[v1*3+v2];` |
|    49691 |  6525 | `	}else{` |
|        - |  6526 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117128 |  6527 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6528 | `	}` |
|   216462 |  6529 | `	if( v1 == 2 ){` |
|      ! 0 |  6530 | `		v1 = 1;` |
|      ! 0 |  6531 | `	}` |
|   216462 |  6532 | `	VmPopOperand(&pTos,1);` |
|   216462 |  6533 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   216462 |  6534 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   216462 |  6535 | `	break;` |
|        - |  6536 | `				 }` |
|        - |  6537 | `/*` |
|        - |  6538 | ` * OP_NULLC: * * *` |
|        - |  6539 | ` * Null coalescing operator '??'.` |
|        - |  6540 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6541 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6542 | ` */` |
|        - |  6543 | `/*` |
|        - |  6544 | ` * OP_NULLC: * P2 *` |
|        - |  6545 | ` * Short-circuit null coalescing '??'.` |
|        - |  6546 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6547 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6548 | ` */` |
|       93 |  6549 | `case PH7_OP_NULLC: {` |
|        - |  6550 | `#ifdef UNTRUST` |
|        - |  6551 | `	if( pTos < pStack ){` |
|        - |  6552 | `		goto Abort;` |
|        - |  6553 | `	}` |
|        - |  6554 | `#endif` |
|      188 |  6555 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6556 | `		/* Left is not null — keep it and skip the RHS */` |
|      114 |  6557 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       58 |  6558 | `	}else{` |
|        - |  6559 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       76 |  6560 | `		VmPopOperand(&pTos, 1);` |
|        - |  6561 | `	}` |
|      188 |  6562 | `	break;` |
|        - |  6563 |  |
|        - |  6564 | `/*` |
|        - |  6565 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6566 | ` * Null coalescing assignment short-circuit.` |
|        - |  6567 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6568 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6569 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6570 | ` */` |
|       28 |  6571 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6572 | `#ifdef UNTRUST` |
|        - |  6573 | `	if( pTos < pStack ){` |
|        - |  6574 | `		goto Abort;` |
|        - |  6575 | `	}` |
|        - |  6576 | `#endif` |
|       58 |  6577 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6578 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6579 | `	}` |
|       58 |  6580 | `	break;` |
|        - |  6581 |  |
|        - |  6582 | `/*` |
|        - |  6583 | ` * OP_NULLC_STORE: * * *` |
|        - |  6584 | ` * Null coalescing assignment store.` |
|        - |  6585 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6586 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6587 | ` * expression result.` |
|        - |  6588 | ` */` |
|        - |  6589 | `/*` |
|        - |  6590 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6591 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6592 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6593 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6594 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6595 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6596 | ` */` |
|       51 |  6597 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6598 | `#ifdef UNTRUST` |
|        - |  6599 | `	if( pTos < pStack ){` |
|        - |  6600 | `		goto Abort;` |
|        - |  6601 | `	}` |
|        - |  6602 | `#endif` |
|      104 |  6603 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6604 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6605 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6606 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6607 | `	}` |
|      104 |  6608 | `	break;` |
|        - |  6609 |  |
|       17 |  6610 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6611 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6612 | `	ph7_value *pObj;` |
|        - |  6613 | `	sxu32 nIdx;` |
|        - |  6614 | `#ifdef UNTRUST` |
|        - |  6615 | `	if( pNos < pStack ){` |
|        - |  6616 | `		goto Abort;` |
|        - |  6617 | `	}` |
|        - |  6618 | `#endif` |
|        - |  6619 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6620 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6621 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6622 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6623 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6624 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6625 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6626 | `		ph7_value *apArg[2];` |
|        5 |  6627 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6628 | `		apArg[1] = pTos;` |
|        5 |  6629 | `		if( pSet ){` |
|        5 |  6630 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6631 | `		}` |
|        - |  6632 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6633 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6634 | `		VmPopOperand(&pTos,1);` |
|        - |  6635 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6636 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6637 | `		break;` |
|        - |  6638 | `	}` |
|       32 |  6639 | `	nIdx = pNos->nIdx;` |
|       32 |  6640 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6641 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6642 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6643 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6644 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6645 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6646 | `	}` |
|       32 |  6647 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6648 | `	VmPopOperand(&pTos,1);` |
|       32 |  6649 | `	break;` |
|        - |  6650 |  |
|        - |  6651 | `/*` |
|        - |  6652 | ` * OP_SPREAD: * * *` |
|        - |  6653 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6654 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6655 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6656 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6657 | ` */` |
|        9 |  6658 | `case PH7_OP_SPREAD: {` |
|        - |  6659 | `#ifdef UNTRUST` |
|        - |  6660 | `	if( pTos < pStack ){` |
|        - |  6661 | `		goto Abort;` |
|        - |  6662 | `	}` |
|        - |  6663 | `#endif` |
|       20 |  6664 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6665 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6666 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6667 | `		if( nEntry == 0 ){` |
|        - |  6668 | `			/* Empty array — remove from stack */` |
|        3 |  6669 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6670 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6671 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6672 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6673 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6674 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6675 | `				VM_STACK_GUARD);` |
|      ! 0 |  6676 | `		}else{` |
|        - |  6677 | `			ph7_hashmap_node *pNode2;` |
|        - |  6678 | `			ph7_value *pElem;` |
|        - |  6679 | `			sxu32 i;` |
|        - |  6680 | `			/* Overwrite TOS with first element */` |
|       18 |  6681 | `			pNode2 = pMap->pFirst;` |
|       18 |  6682 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6683 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6684 | `			if( pElem ){` |
|       18 |  6685 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6686 | `			}` |
|       18 |  6687 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6688 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6689 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6690 | `			pNode2 = pNode2->pPrev;` |
|        - |  6691 | `			/* Push remaining elements */` |
|       44 |  6692 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6693 | `				pTos++;` |
|       28 |  6694 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6695 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6696 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6697 | `				if( pElem ){` |
|       28 |  6698 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6699 | `				}` |
|       28 |  6700 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6701 | `			}` |
|       18 |  6702 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6703 | `		}` |
|        9 |  6704 | `	}` |
|        - |  6705 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6706 | `	break;` |
|        - |  6707 |  |
|        - |  6708 | `/*` |
|        - |  6709 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6710 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6711 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6712 | ` */` |
|       34 |  6713 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6714 | `#ifdef UNTRUST` |
|        - |  6715 | `	if( pTos < pStack ){` |
|        - |  6716 | `		goto Abort;` |
|        - |  6717 | `	}` |
|        - |  6718 | `#endif` |
|       70 |  6719 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       70 |  6720 | `	break;` |
|        - |  6721 |  |
|        - |  6722 | `/* OP_LXOR: * * *` |
|        - |  6723 | ` *` |
|        - |  6724 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6725 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6726 | ` * stack.` |
|        - |  6727 | ` * According to the PHP language reference manual:` |
|        - |  6728 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6729 | ` *  TRUE,but not both.` |
|        - |  6730 | ` */` |
|        5 |  6731 | `case PH7_OP_LXOR:{` |
|       11 |  6732 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6733 | `	sxi32 v = 0;` |
|        - |  6734 | `#ifdef UNTRUST` |
|        - |  6735 | `	if( pNos < pStack ){` |
|        - |  6736 | `		goto Abort;` |
|        - |  6737 | `	}` |
|        - |  6738 | `#endif` |
|        - |  6739 | `	/* Force a boolean cast */` |
|       11 |  6740 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6741 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6742 | `	}` |
|       11 |  6743 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6744 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6745 | `	}` |
|       11 |  6746 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6747 | `		v = 1;` |
|        3 |  6748 | `	}` |
|       11 |  6749 | `	VmPopOperand(&pTos,1);` |
|       11 |  6750 | `	pTos->x.iVal = v;` |
|       11 |  6751 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6752 | `	break;` |
|        - |  6753 | `				 }` |
|        - |  6754 | `/* OP_EQ P1 P2 P3` |
|        - |  6755 | ` *` |
|        - |  6756 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6757 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6758 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6759 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6760 | ` */` |
|        - |  6761 | `/* OP_NEQ P1 P2 P3` |
|        - |  6762 | ` *` |
|        - |  6763 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6764 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6765 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6766 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6767 | ` */` |
|     4577 |  6768 | `case PH7_OP_EQ:` |
|        - |  6769 | `case PH7_OP_NEQ: {` |
|     9156 |  6770 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6771 | `	/* Perform the comparison and act accordingly */` |
|        - |  6772 | `#ifdef UNTRUST` |
|        - |  6773 | `	if( pNos < pStack ){` |
|        - |  6774 | `		goto Abort;` |
|        - |  6775 | `	}` |
|        - |  6776 | `#endif` |
|     9156 |  6777 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9156 |  6778 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6779 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9147 |  6780 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9112 |  6781 | `		rc = rc == 0;` |
|     4557 |  6782 | `	}else{` |
|       28 |  6783 | `		rc = rc != 0;` |
|        - |  6784 | `	}` |
|     9156 |  6785 | `	VmPopOperand(&pTos,1);` |
|     9156 |  6786 | `	if( !pInstr->iP2 ){` |
|        - |  6787 | `		/* Push comparison result without taking the jump */` |
|     9156 |  6788 | `		PH7_MemObjRelease(pTos);` |
|     9156 |  6789 | `		pTos->x.iVal = rc;` |
|        - |  6790 | `		/* Invalidate any prior representation */` |
|     9156 |  6791 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4579 |  6792 | `	}else{` |
|      ! 0 |  6793 | `		if( rc ){` |
|        - |  6794 | `			/* Jump to the desired location */` |
|      ! 0 |  6795 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6796 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6797 | `		}` |
|        - |  6798 | `	}` |
|     9156 |  6799 | `	break;` |
|        - |  6800 | `				 }` |
|        - |  6801 | `/* OP_TEQ P1 P2 *` |
|        - |  6802 | ` *` |
|        - |  6803 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6804 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6805 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6806 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6807 | ` */` |
|   161309 |  6808 | `case PH7_OP_TEQ: {` |
|   322620 |  6809 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6810 | `	/* Perform the comparison and act accordingly */` |
|        - |  6811 | `#ifdef UNTRUST` |
|        - |  6812 | `	if( pNos < pStack ){` |
|        - |  6813 | `		goto Abort;` |
|        - |  6814 | `	}` |
|        - |  6815 | `#endif` |
|   322620 |  6816 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   322620 |  6817 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6818 | `		rc = 0;` |
|        2 |  6819 | `	}else{` |
|   322618 |  6820 | `		rc = rc == 0;` |
|        - |  6821 | `	}` |
|   322620 |  6822 | `	VmPopOperand(&pTos,1);` |
|   322620 |  6823 | `	if( !pInstr->iP2 ){` |
|        - |  6824 | `		/* Push comparison result without taking the jump */` |
|   322620 |  6825 | `		PH7_MemObjRelease(pTos);` |
|   322620 |  6826 | `		pTos->x.iVal = rc;` |
|        - |  6827 | `		/* Invalidate any prior representation */` |
|   322620 |  6828 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   161311 |  6829 | `	}else{` |
|      ! 0 |  6830 | `		if( rc ){` |
|        - |  6831 | `			/* Jump to the desired location */` |
|      ! 0 |  6832 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6833 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6834 | `		}` |
|        - |  6835 | `	}` |
|   322620 |  6836 | `	break;` |
|        - |  6837 | `				 }` |
|        - |  6838 | `/* OP_TNE P1 P2 *` |
|        - |  6839 | ` *` |
|        - |  6840 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6841 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6842 | ` * instruction.` |
|        - |  6843 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6844 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6845 | ` *` |
|        - |  6846 | ` */` |
|   124094 |  6847 | `case PH7_OP_TNE: {` |
|   248190 |  6848 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6849 | `	/* Perform the comparison and act accordingly */` |
|        - |  6850 | `#ifdef UNTRUST` |
|        - |  6851 | `	if( pNos < pStack ){` |
|        - |  6852 | `		goto Abort;` |
|        - |  6853 | `	}` |
|        - |  6854 | `#endif` |
|   248190 |  6855 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   248190 |  6856 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6857 | `		rc = 1;` |
|        2 |  6858 | `	}else{` |
|   248188 |  6859 | `		rc = rc != 0;` |
|        - |  6860 | `	}` |
|   248190 |  6861 | `	VmPopOperand(&pTos,1);` |
|   248190 |  6862 | `	if( !pInstr->iP2 ){` |
|        - |  6863 | `		/* Push comparison result without taking the jump */` |
|   248190 |  6864 | `		PH7_MemObjRelease(pTos);` |
|   248190 |  6865 | `		pTos->x.iVal = rc;` |
|        - |  6866 | `		/* Invalidate any prior representation */` |
|   248190 |  6867 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124096 |  6868 | `	}else{` |
|      ! 0 |  6869 | `		if( rc ){` |
|        - |  6870 | `			/* Jump to the desired location */` |
|      ! 0 |  6871 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6872 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6873 | `		}` |
|        - |  6874 | `	}` |
|   248190 |  6875 | `	break;` |
|        - |  6876 | `				 }` |
|        - |  6877 | `/* OP_LT P1 P2 P3` |
|        - |  6878 | ` *` |
|        - |  6879 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6880 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6881 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6882 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6883 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6884 | ` *` |
|        - |  6885 | ` */` |
|        - |  6886 | `/* OP_LE P1 P2 P3` |
|        - |  6887 | ` *` |
|        - |  6888 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6889 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6890 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6891 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6892 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6893 | ` *` |
|        - |  6894 | ` */` |
|   112423 |  6895 | `case PH7_OP_LT:` |
|        - |  6896 | `case PH7_OP_LE: {` |
|   224892 |  6897 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6898 | `	/* Perform the comparison and act accordingly */` |
|        - |  6899 | `#ifdef UNTRUST` |
|        - |  6900 | `	if( pNos < pStack ){` |
|        - |  6901 | `		goto Abort;` |
|        - |  6902 | `	}` |
|        - |  6903 | `#endif` |
|   224892 |  6904 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224892 |  6905 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6906 | `		rc = 0;` |
|   224888 |  6907 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  6908 | `		rc = rc < 1;` |
|      805 |  6909 | `	}else{` |
|   223278 |  6910 | `		rc = rc < 0;` |
|        - |  6911 | `	}` |
|   224892 |  6912 | `	VmPopOperand(&pTos,1);` |
|   224892 |  6913 | `	if( !pInstr->iP2 ){` |
|        - |  6914 | `		/* Push comparison result without taking the jump */` |
|   224892 |  6915 | `		PH7_MemObjRelease(pTos);` |
|   224892 |  6916 | `		pTos->x.iVal = rc;` |
|        - |  6917 | `		/* Invalidate any prior representation */` |
|   224892 |  6918 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112469 |  6919 | `	}else{` |
|      ! 0 |  6920 | `		if( rc ){` |
|        - |  6921 | `			/* Jump to the desired location */` |
|      ! 0 |  6922 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6923 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6924 | `		}` |
|        - |  6925 | `	}` |
|   224892 |  6926 | `	break;` |
|        - |  6927 | `				}` |
|        - |  6928 | `/* OP_GT P1 P2 P3` |
|        - |  6929 | ` *` |
|        - |  6930 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6931 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6932 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6933 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6934 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6935 | ` *` |
|        - |  6936 | ` */` |
|        - |  6937 | `/* OP_GE P1 P2 P3` |
|        - |  6938 | ` *` |
|        - |  6939 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6940 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6941 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6942 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6943 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6944 | ` *` |
|        - |  6945 | ` */` |
|    55654 |  6946 | `case PH7_OP_GT:` |
|        - |  6947 | `case PH7_OP_GE: {` |
|   111310 |  6948 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6949 | `	/* Perform the comparison and act accordingly */` |
|        - |  6950 | `#ifdef UNTRUST` |
|        - |  6951 | `	if( pNos < pStack ){` |
|        - |  6952 | `		goto Abort;` |
|        - |  6953 | `	}` |
|        - |  6954 | `#endif` |
|   111310 |  6955 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111310 |  6956 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6957 | `		rc = 0;` |
|   111306 |  6958 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110878 |  6959 | `		rc = rc >= 0;` |
|    55440 |  6960 | `	}else{` |
|      426 |  6961 | `		rc = rc > 0;` |
|        - |  6962 | `	}` |
|   111310 |  6963 | `	VmPopOperand(&pTos,1);` |
|   111310 |  6964 | `	if( !pInstr->iP2 ){` |
|        - |  6965 | `		/* Push comparison result without taking the jump */` |
|   111310 |  6966 | `		PH7_MemObjRelease(pTos);` |
|   111310 |  6967 | `		pTos->x.iVal = rc;` |
|        - |  6968 | `		/* Invalidate any prior representation */` |
|   111310 |  6969 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55656 |  6970 | `	}else{` |
|      ! 0 |  6971 | `		if( rc ){` |
|        - |  6972 | `			/* Jump to the desired location */` |
|      ! 0 |  6973 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6974 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6975 | `		}` |
|        - |  6976 | `	}` |
|   111310 |  6977 | `	break;` |
|        - |  6978 | `				}` |
|        - |  6979 | `/* OP_SPACESHIP * * *` |
|        - |  6980 | ` *` |
|        - |  6981 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6982 | ` *   -1 if left < right` |
|        - |  6983 | ` *    0 if left == right` |
|        - |  6984 | ` *    1 if left > right` |
|        - |  6985 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6986 | ` */` |
|       25 |  6987 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6988 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6989 | `#ifdef UNTRUST` |
|        - |  6990 | `	if( pNos < pStack ){` |
|        - |  6991 | `		goto Abort;` |
|        - |  6992 | `	}` |
|        - |  6993 | `#endif` |
|       51 |  6994 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6995 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6996 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6997 | `		rc = 1;` |
|        4 |  6998 | `	}else{` |
|        - |  6999 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7000 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7001 | `	}` |
|       51 |  7002 | `	VmPopOperand(&pTos,1);` |
|       51 |  7003 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7004 | `	pTos->x.iVal = rc;` |
|       51 |  7005 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7006 | `	break;` |
|        - |  7007 | `				}` |
|        - |  7008 | `/* OP_SEQ P1 P2 *` |
|        - |  7009 | ` * Strict string comparison.` |
|        - |  7010 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7011 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7012 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7013 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7014 | ` * use PH7_OP_EQ.` |
|        - |  7015 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7016 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7017 | ` */` |
|        - |  7018 | `/* OP_SNE P1 P2 *` |
|        - |  7019 | ` * Strict string comparison.` |
|        - |  7020 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7021 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7022 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7023 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7024 | ` * use PH7_OP_EQ.` |
|        - |  7025 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7026 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7027 | ` */` |
|       18 |  7028 | `case PH7_OP_SEQ:` |
|        - |  7029 | `case PH7_OP_SNE: {` |
|       38 |  7030 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7031 | `	SyString s1,s2;` |
|        - |  7032 | `	/* Perform the comparison and act accordingly */` |
|        - |  7033 | `#ifdef UNTRUST` |
|        - |  7034 | `	if( pNos < pStack ){` |
|        - |  7035 | `		goto Abort;` |
|        - |  7036 | `	}` |
|        - |  7037 | `#endif` |
|        - |  7038 | `	/* Force a string cast */` |
|       38 |  7039 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7040 | `		PH7_MemObjToString(pTos);` |
|        2 |  7041 | `	}` |
|       38 |  7042 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7043 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7044 | `	}` |
|       38 |  7045 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7046 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7047 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7048 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7049 | `		rc = rc != 0;` |
|      ! 0 |  7050 | `	}else{` |
|       38 |  7051 | `		rc = rc == 0;` |
|        - |  7052 | `	}` |
|       38 |  7053 | `	VmPopOperand(&pTos,1);` |
|       38 |  7054 | `	if( !pInstr->iP2 ){` |
|        - |  7055 | `		/* Push comparison result without taking the jump */` |
|       38 |  7056 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7057 | `		pTos->x.iVal = rc;` |
|        - |  7058 | `		/* Invalidate any prior representation */` |
|       38 |  7059 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7060 | `	}else{` |
|      ! 0 |  7061 | `		if( rc ){` |
|        - |  7062 | `			/* Jump to the desired location */` |
|      ! 0 |  7063 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7064 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7065 | `		}` |
|        - |  7066 | `	}` |
|       38 |  7067 | `	break;` |
|        - |  7068 | `				 }` |
|        - |  7069 | `/*` |
|        - |  7070 | ` * OP_LOAD_REF * * *` |
|        - |  7071 | ` * Push the index of a referenced object on the stack.` |
|        - |  7072 | ` */` |
|       60 |  7073 | `case PH7_OP_LOAD_REF: {` |
|        - |  7074 | `	sxu32 nIdx;` |
|        - |  7075 | `#ifdef UNTRUST` |
|        - |  7076 | `	if( pTos < pStack ){` |
|        - |  7077 | `		goto Abort;` |
|        - |  7078 | `	}` |
|        - |  7079 | `#endif` |
|        - |  7080 | `	/* Extract memory object index */` |
|      121 |  7081 | `	nIdx = pTos->nIdx;` |
|      121 |  7082 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7083 | `		/* Nullify the object */` |
|      101 |  7084 | `		PH7_MemObjRelease(pTos);` |
|        - |  7085 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7086 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7087 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7088 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7089 | `	}` |
|      121 |  7090 | `	break;` |
|        - |  7091 | `					  }` |
|        - |  7092 | `/*` |
|        - |  7093 | ` * OP_STORE_REF * * P3` |
|        - |  7094 | ` * Perform an assignment operation by reference.` |
|        - |  7095 | ` */` |
|       16 |  7096 | ` case PH7_OP_STORE_REF: {` |
|       34 |  7097 | `	 SyString sName = { 0 , 0 };` |
|        - |  7098 | `	 VmFrame *pFrameLocal;` |
|        - |  7099 | `	SyHashEntry *pEntry;` |
|        - |  7100 | `	sxu32 nIdx;` |
|        - |  7101 | `#ifdef UNTRUST` |
|        - |  7102 | `	if( pTos < pStack ){` |
|        - |  7103 | `		goto Abort;` |
|        - |  7104 | `	}` |
|        - |  7105 | `#endif` |
|       34 |  7106 | `	if( pInstr->p3 == 0 ){` |
|        - |  7107 | `		char *zName;` |
|        - |  7108 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7109 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7110 | `			/* Force a string cast */` |
|      ! 0 |  7111 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7112 | `		}` |
|      ! 0 |  7113 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7114 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7115 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7116 | `			if( zName ){` |
|      ! 0 |  7117 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7118 | `			}` |
|      ! 0 |  7119 | `		}` |
|      ! 0 |  7120 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7121 | `		pTos--;` |
|      ! 0 |  7122 | `	}else{` |
|       34 |  7123 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7124 | `	}` |
|       34 |  7125 | `	nIdx = pTos->nIdx;` |
|       34 |  7126 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7127 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7128 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7129 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7130 | `		}else{` |
|        - |  7131 | `			ph7_value *pObj;` |
|        - |  7132 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7133 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7134 | `			if( pObj == 0 ){` |
|      ! 0 |  7135 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7136 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7137 | `				goto Abort;` |
|        - |  7138 | `			}` |
|        - |  7139 | `			/* Perform the store operation */` |
|      ! 0 |  7140 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7141 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7142 | `		}` |
|       34 |  7143 | `	}else if( sName.nByte > 0){` |
|       34 |  7144 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7145 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7146 | `		}else{` |
|       34 |  7147 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7148 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7149 | `			/* Query the local frame */` |
|       34 |  7150 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7151 | `			if( pEntry ){` |
|      ! 0 |  7152 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7153 | `			}else{` |
|       34 |  7154 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7155 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7156 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7157 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7158 | `				}` |
|       34 |  7159 | `				if( rc == SXRET_OK ){` |
|       34 |  7160 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7161 | `				}` |
|        - |  7162 | `			}` |
|        - |  7163 | `		}` |
|       16 |  7164 | `	}` |
|       34 |  7165 | `	break;` |
|        - |  7166 | `				 }` |
|        - |  7167 | `/*` |
|        - |  7168 | ` * OP_UPLINK P1 * *` |
|        - |  7169 | ` * Link a variable to the top active VM frame.` |
|        - |  7170 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7171 | ` */` |
|       30 |  7172 | `case PH7_OP_UPLINK: {` |
|       62 |  7173 | `	if( pVm->pFrame->pParent ){` |
|       62 |  7174 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7175 | `		SyString sName;` |
|        - |  7176 | `		/* Perform the link */` |
|      132 |  7177 | `		while( pLink <= pTos ){` |
|       72 |  7178 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7179 | `				/* Force a string cast */` |
|      ! 0 |  7180 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7181 | `			}` |
|       72 |  7182 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       72 |  7183 | `			if( sName.nByte > 0 ){` |
|       72 |  7184 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7185 | `			}` |
|       72 |  7186 | `			pLink++;` |
|        2 |  7187 | `		}` |
|       30 |  7188 | `	}` |
|       62 |  7189 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       62 |  7190 | `	break;` |
|        - |  7191 | `					}` |
|        - |  7192 | `/*` |
|        - |  7193 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7194 | ` * Push an exception in the corresponding container so that` |
|        - |  7195 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7196 | ` */` |
|      180 |  7197 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      362 |  7198 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7199 | `	VmFrame *pFrameLocal;` |
|        - |  7200 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      362 |  7201 | `	pException->iFinallyDone = 0;` |
|      362 |  7202 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7203 | `	/* Create the exception frame */` |
|      362 |  7204 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      362 |  7205 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7206 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7207 | `		goto Abort;` |
|        - |  7208 | `	}` |
|        - |  7209 | `	/* Mark the special frame */` |
|      362 |  7210 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      362 |  7211 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7212 | `	/* Point to the frame that trigger the exception */` |
|      362 |  7213 | `	pFrameLocal = pFrameLocal->pParent;` |
|      362 |  7214 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      362 |  7215 | `	pException->pFrame = pFrameLocal;` |
|      362 |  7216 | `	break;` |
|        - |  7217 | `							}` |
|        - |  7218 | `/*` |
|        - |  7219 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7220 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7221 | ` */` |
|      179 |  7222 | `case PH7_OP_POP_EXCEPTION: {` |
|      360 |  7223 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      360 |  7224 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7225 | `		ph7_exception **apException;` |
|        - |  7226 | `		/* Pop the loaded exception */` |
|       32 |  7227 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7228 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7229 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7230 | `		}` |
|       15 |  7231 | `	}` |
|      360 |  7232 | `	pException->pFrame = 0;` |
|        - |  7233 | `	/* Leave the exception frame */` |
|      360 |  7234 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7235 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      360 |  7236 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7237 | `		sxi32 rcFinally;` |
|       20 |  7238 | `		pException->iFinallyDone = 1;` |
|       20 |  7239 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7240 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7241 | `			goto Abort;` |
|        - |  7242 | `		}` |
|        9 |  7243 | `	}` |
|      360 |  7244 | `	break;` |
|        - |  7245 | `							}` |
|        - |  7246 |  |
|        - |  7247 | `/*` |
|        - |  7248 | ` * OP_THROW * P2 *` |
|        - |  7249 | ` * Throw an user exception.` |
|        - |  7250 | ` */` |
|       75 |  7251 | `case PH7_OP_THROW: {` |
|      152 |  7252 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      152 |  7253 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7254 | `#ifdef UNTRUST` |
|        - |  7255 | `	if( pTos < pStack ){` |
|        - |  7256 | `		goto Abort;` |
|        - |  7257 | `	}` |
|        - |  7258 | `#endif` |
|      152 |  7259 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7260 | `	/* Tell the upper layer that an exception was thrown */` |
|      152 |  7261 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      152 |  7262 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      152 |  7263 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7264 | `		ph7_class *pThrowable;` |
|        - |  7265 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      152 |  7266 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      153 |  7267 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7268 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7269 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7270 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7271 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7272 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7273 | `			if( pErrorClass ){` |
|        3 |  7274 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7275 | `			}` |
|        3 |  7276 | `			if( pErrInst ){` |
|        - |  7277 | `				ph7_class_method *pCons;` |
|        3 |  7278 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7279 | `				if( pCons ){` |
|        - |  7280 | `					ph7_value sArg;` |
|        - |  7281 | `					ph7_value *apArg[1];` |
|        - |  7282 | `					SyString sMsgStr;` |
|        - |  7283 | `					static const char zErrMsg[] =` |
|        - |  7284 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7285 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7286 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7287 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7288 | `					apArg[0] = &sArg;` |
|        3 |  7289 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7290 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7291 | `				}` |
|        3 |  7292 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7293 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7294 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7295 | `					goto Abort;` |
|        - |  7296 | `				}` |
|        2 |  7297 | `			}else{` |
|        - |  7298 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7299 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7300 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7301 | `					goto Abort;` |
|        - |  7302 | `				}` |
|        - |  7303 | `			}` |
|        2 |  7304 | `		}else{` |
|        - |  7305 | `			/* Throw the exception */` |
|      150 |  7306 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      150 |  7307 | `			if( rc == SXERR_ABORT ){` |
|        - |  7308 | `				/* Abort processing immediately */` |
|       11 |  7309 | `				goto Abort;` |
|        - |  7310 | `			}` |
|        - |  7311 | `		}` |
|       72 |  7312 | `	}else{` |
|        - |  7313 | `		/* Expecting a class instance */` |
|      ! 0 |  7314 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7315 | `		if( rc == SXERR_ABORT ){` |
|        - |  7316 | `			/* Abort processing immediately */` |
|      ! 0 |  7317 | `			goto Abort;` |
|        - |  7318 | `		}` |
|        - |  7319 | `	}` |
|        - |  7320 | `	/* Pop the top entry */` |
|      142 |  7321 | `	VmPopOperand(&pTos,1);` |
|        - |  7322 | `	/* Perform an unconditional jump */` |
|      142 |  7323 | `	pc = nJump - 1;` |
|      142 |  7324 | `	break;` |
|        - |  7325 | `				   }` |
|        - |  7326 | `/*` |
|        - |  7327 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7328 | ` * Prepare a foreach step.` |
|        - |  7329 | ` */` |
|     6166 |  7330 | `case PH7_OP_FOREACH_INIT: {` |
|    12334 |  7331 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7332 | `	void *pName;` |
|        - |  7333 | `#ifdef UNTRUST` |
|        - |  7334 | `	if( pTos < pStack ){` |
|        - |  7335 | `		goto Abort;` |
|        - |  7336 | `	}` |
|        - |  7337 | `#endif` |
|    12334 |  7338 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7339 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7340 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7341 | `			/* Force a string cast */` |
|      ! 0 |  7342 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7343 | `		}` |
|        - |  7344 | `		/* Duplicate name */` |
|      ! 0 |  7345 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7346 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7347 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7348 | `		}` |
|      ! 0 |  7349 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7350 | `	}` |
|    12334 |  7351 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7352 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7353 | `			/* Force a string cast */` |
|      ! 0 |  7354 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7355 | `		}` |
|        - |  7356 | `		/* Duplicate name */` |
|      ! 0 |  7357 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7358 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7359 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7360 | `		}` |
|      ! 0 |  7361 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7362 | `	}` |
|        - |  7363 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12334 |  7364 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7365 | `		/* Jump out of the loop */` |
|      ! 0 |  7366 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7367 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7368 | `		}` |
|      ! 0 |  7369 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7370 | `	}else{` |
|        - |  7371 | `		ph7_foreach_step *pStep;` |
|    12334 |  7372 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12334 |  7373 | `		if( pStep == 0 ){` |
|      ! 0 |  7374 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7375 | `			/* Jump out of the loop */` |
|      ! 0 |  7376 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7377 | `		}else{` |
|        - |  7378 | `			/* Zero the structure */` |
|    12334 |  7379 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7380 | `			/* Prepare the step */` |
|    12334 |  7381 | `			pStep->iFlags = pInfo->iFlags;` |
|    12334 |  7382 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7383 | `				ph7_hashmap *pMap;` |
|        - |  7384 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7385 | `				 * source array so mutations don't affect other sharers. */` |
|    12300 |  7386 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7387 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7388 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7389 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7390 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7391 | `						 * variable still points at the same hashmap as` |
|        - |  7392 | `						 * the stack value. */` |
|        9 |  7393 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7394 | `							pCur->iRef--;` |
|        9 |  7395 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7396 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7397 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7398 | `						}` |
|        4 |  7399 | `					}` |
|        4 |  7400 | `				}` |
|    12300 |  7401 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7402 | `				/* Reset the internal loop cursor */` |
|    12300 |  7403 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7404 | `				/* Mark the step */` |
|    12300 |  7405 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12300 |  7406 | `				pStep->xIter.pMap = pMap;` |
|    12300 |  7407 | `				pMap->iRef++;` |
|     6151 |  7408 | `			}else{` |
|       36 |  7409 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7410 | `				ph7_class *pIteratorClass;` |
|        - |  7411 | `				/* Check if the object implements Iterator */` |
|       36 |  7412 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7413 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7414 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7415 | `					ph7_class_method *pRewind;` |
|       24 |  7416 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7417 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7418 | `					pThis->iRef++;` |
|       24 |  7419 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7420 | `					if( pRewind ){` |
|       24 |  7421 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7422 | `					}` |
|       13 |  7423 | `				}else{` |
|        - |  7424 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7425 | `					ph7_class *pIterAggClass;` |
|       14 |  7426 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7427 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7428 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7429 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7430 | `						ph7_class_method *pGetIter;` |
|        3 |  7431 | `						int iterAggOk = 0;` |
|        3 |  7432 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7433 | `						if( pGetIter ){` |
|        - |  7434 | `							ph7_value sResult;` |
|        3 |  7435 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7436 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7437 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7438 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7439 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7440 | `									ph7_class_method *pRewind;` |
|        3 |  7441 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7442 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7443 | `									pIterObj->iRef++;` |
|        - |  7444 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7445 | `									pStep->pOwner = pThis;` |
|        3 |  7446 | `									pThis->iRef++;` |
|        3 |  7447 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7448 | `									if( pRewind ){` |
|        3 |  7449 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7450 | `									}` |
|        3 |  7451 | `									iterAggOk = 1;` |
|        1 |  7452 | `								}` |
|        1 |  7453 | `							}` |
|        3 |  7454 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7455 | `						}` |
|        3 |  7456 | `						if( !iterAggOk ){` |
|        - |  7457 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7458 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7459 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7460 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7461 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7462 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7463 | `						}` |
|        2 |  7464 | `					}else{` |
|        - |  7465 | `						/* Plain object iteration via hAttr */` |
|       12 |  7466 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7467 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7468 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7469 | `						pThis->iRef++;` |
|        - |  7470 | `					}` |
|        - |  7471 | `				}` |
|        - |  7472 | `			}` |
|        - |  7473 | `		}` |
|    12334 |  7474 | `		if( pStep ){` |
|    12334 |  7475 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7476 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7477 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7478 | `				/* Jump out of the loop */` |
|      ! 0 |  7479 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7480 | `			}` |
|     6166 |  7481 | `		}` |
|        - |  7482 | `	}` |
|    12334 |  7483 | `	VmPopOperand(&pTos,1);` |
|    12334 |  7484 | `	break;` |
|        - |  7485 | `						  }` |
|        - |  7486 | `/*` |
|        - |  7487 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7488 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7489 | ` */` |
|   101183 |  7490 | `case PH7_OP_FOREACH_STEP: {` |
|   202368 |  7491 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7492 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7493 | `	ph7_value *pValue;` |
|        - |  7494 | `	VmFrame *pFrameLocal;` |
|        - |  7495 | `	/* Peek the last step */` |
|   202368 |  7496 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   202368 |  7497 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   202368 |  7498 | `	pFrameLocal = pVm->pFrame;` |
|   202368 |  7499 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   202368 |  7500 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   202234 |  7501 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7502 | `		ph7_hashmap_node *pNode;` |
|        - |  7503 | `		/* Extract the current node value */` |
|   202234 |  7504 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   202234 |  7505 | `		if( pNode == 0 ){` |
|        - |  7506 | `			/* No more entry to process */` |
|    12298 |  7507 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12298 |  7508 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7509 | `				/* Break the reference with the last element */` |
|        7 |  7510 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7511 | `			}` |
|        - |  7512 | `			/* Automatically reset the loop cursor */` |
|    12298 |  7513 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7514 | `			/* Cleanup the mess left behind */` |
|    12298 |  7515 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12298 |  7516 | `			SySetPop(&pInfo->aStep);` |
|    12298 |  7517 | `			PH7_HashmapUnref(pMap);` |
|     6150 |  7518 | `		}else{` |
|   189938 |  7519 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7520 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7521 | `				if( pKey ){` |
|      528 |  7522 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7523 | `				}` |
|      263 |  7524 | `			}` |
|   189938 |  7525 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7526 | `				SyHashEntry *pEntry;` |
|        - |  7527 | `				/* Pass by reference */` |
|       23 |  7528 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7529 | `				if( pEntry ){` |
|       21 |  7530 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7531 | `				}else{` |
|        4 |  7532 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7533 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7534 | `				}` |
|       12 |  7535 | `			}else{` |
|        - |  7536 | `				/* Make a copy of the entry value */` |
|   189916 |  7537 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   189916 |  7538 | `				if( pValue ){` |
|   189916 |  7539 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    94957 |  7540 | `				}` |
|        - |  7541 | `			}` |
|        2 |  7542 | `		}` |
|   101252 |  7543 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7544 | `		/* Iterator-based iteration.` |
|        - |  7545 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7546 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7547 | `		 */` |
|      106 |  7548 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7549 | `		ph7_class_method *pMethod;` |
|        - |  7550 | `		ph7_value sResult;` |
|      106 |  7551 | `		int isValid = 0;` |
|        - |  7552 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7553 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7554 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7555 | `		}else{` |
|       82 |  7556 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7557 | `			if( pMethod ){` |
|       82 |  7558 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7559 | `			}` |
|        - |  7560 | `		}` |
|        - |  7561 | `		/* Call valid() */` |
|      106 |  7562 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7563 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7564 | `		if( pMethod ){` |
|      106 |  7565 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7566 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7567 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7568 | `		}` |
|      106 |  7569 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7570 | `		if( !isValid ){` |
|        - |  7571 | `			/* Iterator exhausted */` |
|       24 |  7572 | `			pc = pInstr->iP2 - 1;` |
|        - |  7573 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7574 | `			if( pStep->pOwner ){` |
|        3 |  7575 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7576 | `			}` |
|       24 |  7577 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7578 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7579 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7580 | `		}else{` |
|        - |  7581 | `			/* Call current() to get value */` |
|       84 |  7582 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7583 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7584 | `			if( pMethod ){` |
|       84 |  7585 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7586 | `			}` |
|       84 |  7587 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7588 | `			if( pValue ){` |
|       84 |  7589 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7590 | `			}` |
|       84 |  7591 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7592 | `			/* Call key() if needed */` |
|       84 |  7593 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7594 | `				ph7_value sKey;` |
|       35 |  7595 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7596 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7597 | `				if( pMethod ){` |
|       35 |  7598 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7599 | `				}` |
|       35 |  7600 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7601 | `				if( pValue ){` |
|       35 |  7602 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7603 | `				}` |
|       35 |  7604 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7605 | `			}` |
|        - |  7606 | `		}` |
|       54 |  7607 | `	}else{` |
|       32 |  7608 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7609 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7610 | `		SyHashEntry *pEntry;` |
|        - |  7611 | `		/* Point to the next attribute */` |
|       36 |  7612 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7613 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7614 | `			/* Check access permission */` |
|       38 |  7615 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7616 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7617 | `					break; /* Access is granted */` |
|        - |  7618 | `			}` |
|        1 |  7619 | `		}` |
|       32 |  7620 | `		if( pEntry == 0 ){` |
|        - |  7621 | `			/* Clean up the mess left behind */` |
|       12 |  7622 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7623 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7624 | `				/* Break the reference with the last element */` |
|        3 |  7625 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7626 | `			}` |
|       12 |  7627 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7628 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7629 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7630 | `		}else{` |
|       22 |  7631 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7632 | `			ph7_value *pAttrValue;` |
|       22 |  7633 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7634 | `				/* Fill with the current attribute name */` |
|       22 |  7635 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7636 | `				if( pKey ){` |
|       22 |  7637 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7638 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7639 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7640 | `				}` |
|       10 |  7641 | `			}` |
|        - |  7642 | `			/* Extract attribute value */` |
|       22 |  7643 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7644 | `			if( pAttrValue ){` |
|       22 |  7645 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7646 | `					/* Pass by reference */` |
|        3 |  7647 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7648 | `					if( pEntry ){` |
|        3 |  7649 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7650 | `					}else{` |
|      ! 0 |  7651 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7652 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7653 | `					}` |
|        2 |  7654 | `				}else{` |
|        - |  7655 | `					/* Make a copy of the attribute value */` |
|       20 |  7656 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7657 | `					if( pValue ){` |
|       20 |  7658 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7659 | `					}` |
|        - |  7660 | `				}` |
|       10 |  7661 | `			}` |
|        - |  7662 | `		}` |
|        - |  7663 | `	}` |
|   202368 |  7664 | `	break;` |
|        - |  7665 | `						  }` |
|        - |  7666 | `/*` |
|        - |  7667 | ` * OP_MEMBER P1 P2` |
|        - |  7668 | ` * Load class attribute/method on the stack.` |
|        - |  7669 | ` */` |
|     3980 |  7670 | `case PH7_OP_MEMBER: {` |
|        - |  7671 | `	ph7_class_instance *pThis;` |
|        - |  7672 | `	ph7_value *pNos;` |
|        - |  7673 | `	SyString sName;` |
|     7962 |  7674 | `	if( !pInstr->iP1 ){` |
|     7734 |  7675 | `		pNos = &pTos[-1];` |
|        - |  7676 | `#ifdef UNTRUST` |
|        - |  7677 | `		if( pNos < pStack ){` |
|        - |  7678 | `			goto Abort;` |
|        - |  7679 | `		}` |
|        - |  7680 | `#endif` |
|     7734 |  7681 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7682 | `			ph7_class *pClass;` |
|        - |  7683 | `			/* Class already instantiated */` |
|     7732 |  7684 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7685 | `			/* Point to the instantiated class */` |
|     7732 |  7686 | `			pClass = pThis->pClass;` |
|        - |  7687 | `			/* Extract attribute name first */` |
|     7732 |  7688 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7732 |  7689 | `			if( pInstr->iP2 ){` |
|        - |  7690 | `				/* Method call */` |
|      782 |  7691 | `				ph7_class_method *pMeth = 0;` |
|      782 |  7692 | `				if( sName.nByte > 0 ){` |
|        - |  7693 | `					/* Extract the target method */` |
|      782 |  7694 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      390 |  7695 | `				}` |
|      782 |  7696 | `				if( pMeth == 0 ){` |
|      ! 0 |  7697 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7698 | `						&pClass->sName,&sName` |
|        - |  7699 | `						);` |
|        - |  7700 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7701 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7702 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7703 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7704 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7705 | `				}else{` |
|        - |  7706 | `					/* Push method name on the stack */` |
|      782 |  7707 | `					PH7_MemObjRelease(pTos);` |
|      782 |  7708 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      782 |  7709 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7710 | `				}` |
|      782 |  7711 | `				pTos->nIdx = SXU32_HIGH;` |
|      392 |  7712 | `			}else{` |
|        - |  7713 | `				/* Attribute access */` |
|     6952 |  7714 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7715 | `				SyHashEntry *pEntry;` |
|        - |  7716 | `				/* Extract the target attribute */` |
|     6952 |  7717 | `				if( sName.nByte > 0 ){` |
|     6952 |  7718 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6952 |  7719 | `					if( pEntry ){` |
|        - |  7720 | `						/* Point to the attribute value */` |
|     6950 |  7721 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3474 |  7722 | `					}` |
|     3475 |  7723 | `				}` |
|     6952 |  7724 | `				if( pObjAttr == 0 ){` |
|        - |  7725 | `					/* No such attribute,load null */` |
|        4 |  7726 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7727 | `						&pClass->sName,&sName);` |
|        - |  7728 | `					/* Call the __get magic method if available */` |
|        3 |  7729 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7730 | `				}` |
|     6952 |  7731 | `				VmPopOperand(&pTos,1);` |
|        - |  7732 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7733 | `				 * This is due to the following case:` |
|        - |  7734 | `				 *     (new TestClass())->foo;` |
|        - |  7735 | `				 */` |
|     6952 |  7736 | `				pThis->iRef++;` |
|     6952 |  7737 | `				PH7_MemObjRelease(pTos);` |
|     6952 |  7738 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6952 |  7739 | `				if( pObjAttr ){` |
|     6950 |  7740 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7741 | `					/* Check attribute access */` |
|     6950 |  7742 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7743 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7744 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7745 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7746 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7747 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6948 |  7748 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3516 |  7749 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  7750 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  7751 | `							int bIsLhs = 0;` |
|       82 |  7752 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  7753 | `								bIsLhs = 1;` |
|       39 |  7754 | `							}` |
|       82 |  7755 | `							if( !bIsLhs ){` |
|        3 |  7756 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7757 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7758 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7759 | `									goto Abort;` |
|        - |  7760 | `								}` |
|        - |  7761 | `								{` |
|        3 |  7762 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7763 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7764 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3980 |  7765 | `										break;` |
|        - |  7766 | `									}` |
|        - |  7767 | `								}` |
|      ! 0 |  7768 | `								goto Exception;` |
|        - |  7769 | `							}` |
|       39 |  7770 | `						}` |
|        - |  7771 | `						/* Load attribute */` |
|     6948 |  7772 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6948 |  7773 | `						if( pValue ){` |
|     6948 |  7774 | `							if( pThis->iRef < 2 ){` |
|        - |  7775 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7776 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7777 | `								 */` |
|        7 |  7778 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7779 | `							}else{` |
|        - |  7780 | `								/* Simple load */` |
|     6942 |  7781 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7782 | `							}` |
|     6948 |  7783 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6946 |  7784 | `								if( pThis->iRef > 1 ){` |
|        - |  7785 | `									/* Load attribute index */` |
|     6940 |  7786 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3469 |  7787 | `								}` |
|     3472 |  7788 | `							}` |
|     3473 |  7789 | `						}` |
|     3475 |  7790 | `					}else{` |
|        - |  7791 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7792 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7793 | `						char zMsg[256];` |
|      ! 0 |  7794 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7795 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7796 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7797 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7798 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7799 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7800 | `						goto Abort;` |
|        - |  7801 | `					}` |
|     3473 |  7802 | `				}` |
|        - |  7803 | `				/* Safely unreference the object */` |
|     6950 |  7804 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7805 | `			}` |
|     3866 |  7806 | `		}else{` |
|        3 |  7807 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7808 | `			VmPopOperand(&pTos,1);` |
|        3 |  7809 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7810 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7811 | `		}` |
|     3867 |  7812 | `	}else{` |
|        - |  7813 | `		/* Static member access using class name */` |
|      230 |  7814 | `		pNos = pTos;` |
|      230 |  7815 | `		pThis = 0;` |
|      230 |  7816 | `		if( !pInstr->p3 ){` |
|      192 |  7817 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  7818 | `			pNos--;` |
|        - |  7819 | `#ifdef UNTRUST` |
|        - |  7820 | `			if( pNos < pStack ){` |
|        - |  7821 | `				goto Abort;` |
|        - |  7822 | `			}` |
|        - |  7823 | `#endif` |
|       97 |  7824 | `		}else{` |
|        - |  7825 | `			/* Attribute name already computed */` |
|       40 |  7826 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7827 | `		}` |
|      230 |  7828 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      230 |  7829 | `			ph7_class *pClass = 0;` |
|      230 |  7830 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7831 | `				/* Class already instantiated */` |
|        5 |  7832 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7833 | `				pClass = pThis->pClass;` |
|        5 |  7834 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7835 | `			}else{` |
|        - |  7836 | `				/* Try to extract the target class */` |
|      226 |  7837 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      226 |  7838 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      226 |  7839 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7840 | `					/* Handle self/static/parent keywords */` |
|      226 |  7841 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7842 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7843 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7844 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7845 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7846 | `						}` |
|      196 |  7847 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7848 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      166 |  7849 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7850 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7851 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7852 | `							pClass = pSelf->pBase;` |
|       13 |  7853 | `						}` |
|       15 |  7854 | `					}else{` |
|      114 |  7855 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7856 | `					}` |
|      112 |  7857 | `				}` |
|        - |  7858 | `			}` |
|      230 |  7859 | `			if( pClass == 0 ){` |
|        - |  7860 | `				/* Undefined class */` |
|      ! 0 |  7861 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7862 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7863 | `					);` |
|      ! 0 |  7864 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7865 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7866 | `				}` |
|      ! 0 |  7867 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7868 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7869 | `			}else{` |
|      230 |  7870 | `				if( pInstr->iP2 ){` |
|        - |  7871 | `					/* Method call */` |
|       86 |  7872 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7873 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7874 | `						/* Extract the target method */` |
|       86 |  7875 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7876 | `					}` |
|       86 |  7877 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7878 | `						if( pMeth ){` |
|      ! 0 |  7879 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7880 | `								&pClass->sName,&sName` |
|        - |  7881 | `								);` |
|      ! 0 |  7882 | `						}else{` |
|      ! 0 |  7883 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7884 | `								&pClass->sName,&sName` |
|        - |  7885 | `								);` |
|        - |  7886 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7887 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7888 | `						}` |
|        - |  7889 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7890 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7891 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7892 | `						}` |
|      ! 0 |  7893 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7894 | `					}else{` |
|        - |  7895 | `						/* Push method name on the stack */` |
|       86 |  7896 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7897 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7898 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7899 | `					}` |
|       86 |  7900 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7901 | `				}else{` |
|        - |  7902 | `					/* Attribute access */` |
|      146 |  7903 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7904 | `					/* Check for special ::class pseudo-constant */` |
|      192 |  7905 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7906 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7907 | `						/* ::class returns the fully qualified class name */` |
|        - |  7908 | `						/* Pop the attribute name from the stack */` |
|       60 |  7909 | `						if( !pInstr->p3 ){` |
|       60 |  7910 | `							VmPopOperand(&pTos,1);` |
|       29 |  7911 | `						}` |
|       60 |  7912 | `						PH7_MemObjRelease(pTos);` |
|        - |  7913 | `						/* Load the class name */` |
|       60 |  7914 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7915 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7916 | `					}else{` |
|        - |  7917 | `						/* Extract the target attribute */` |
|       88 |  7918 | `						if( sName.nByte > 0 ){` |
|       88 |  7919 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       43 |  7920 | `						}` |
|       88 |  7921 | `						if( pAttr == 0 ){` |
|        - |  7922 | `							/* No such attribute,load null */` |
|      ! 0 |  7923 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7924 | `								&pClass->sName,&sName);` |
|        - |  7925 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7926 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7927 | `						}` |
|        - |  7928 | `						/* Pop the attribute name from the stack */` |
|       88 |  7929 | `						if( !pInstr->p3 ){` |
|       50 |  7930 | `							VmPopOperand(&pTos,1);` |
|       24 |  7931 | `						}` |
|       88 |  7932 | `						PH7_MemObjRelease(pTos);` |
|       88 |  7933 | `						pTos->nIdx = SXU32_HIGH;` |
|       88 |  7934 | `						if( pAttr ){` |
|       88 |  7935 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7936 | `								/* Access to a non static attribute */` |
|      ! 0 |  7937 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7938 | `									&pClass->sName,&pAttr->sName` |
|        - |  7939 | `									);` |
|      ! 0 |  7940 | `							}else{` |
|        - |  7941 | `								ph7_value *pValue;` |
|        - |  7942 | `								/* Check if the access to the attribute is allowed */` |
|       88 |  7943 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7944 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7945 | `									 * Same LHS-of-store peek as the instance path. */` |
|       82 |  7946 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       56 |  7947 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7948 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7949 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7950 | `										if( pS ){` |
|       28 |  7951 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7952 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7953 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7954 | `												int bIsLhs = 0;` |
|        8 |  7955 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7956 | `													bIsLhs = 1;` |
|        2 |  7957 | `												}` |
|        8 |  7958 | `												if( !bIsLhs ){` |
|        3 |  7959 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7960 | `													if( pThis ){` |
|      ! 0 |  7961 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7962 | `													}` |
|        3 |  7963 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7964 | `														goto Abort;` |
|        - |  7965 | `													}` |
|        - |  7966 | `													{` |
|        3 |  7967 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7968 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7969 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7970 | `															break;` |
|        - |  7971 | `														}` |
|        - |  7972 | `													}` |
|      ! 0 |  7973 | `													goto Exception;` |
|        - |  7974 | `												}` |
|        2 |  7975 | `											}` |
|       12 |  7976 | `										}` |
|       12 |  7977 | `									}` |
|        - |  7978 | `									/* Load the desired attribute */` |
|       82 |  7979 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       82 |  7980 | `									if( pValue ){` |
|       82 |  7981 | `										PH7_MemObjLoad(pValue,pTos);` |
|       82 |  7982 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7983 | `											/* Load index number */` |
|       38 |  7984 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7985 | `										}` |
|       40 |  7986 | `									}` |
|       42 |  7987 | `								}else{` |
|        - |  7988 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7989 | `									char zMsg[256];` |
|        5 |  7990 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7991 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7992 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7993 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7994 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7995 | `									}else{` |
|      ! 0 |  7996 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7997 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7998 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7999 | `									}` |
|        5 |  8000 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8001 | `									goto Abort;` |
|        - |  8002 | `								}` |
|        - |  8003 | `							}` |
|       40 |  8004 | `						}` |
|        - |  8005 | `					}` |
|        - |  8006 | `				}` |
|      224 |  8007 | `				if( pThis ){` |
|        - |  8008 | `					/* Safely unreference the object */` |
|        5 |  8009 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8010 | `				}` |
|        - |  8011 | `			}` |
|      113 |  8012 | `		}else{` |
|        - |  8013 | `			/* Pop operands */` |
|      ! 0 |  8014 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8015 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8016 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8017 | `			}` |
|      ! 0 |  8018 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8019 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8020 | `		}` |
|        - |  8021 | `	}` |
|     7954 |  8022 | `	break;` |
|        - |  8023 | `					}` |
|        - |  8024 | `/*` |
|        - |  8025 | ` * OP_NEW P1 * * *` |
|        - |  8026 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8027 | ` */` |
|      649 |  8028 | `case PH7_OP_NEW: {` |
|     1300 |  8029 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1300 |  8030 | `	ph7_class *pClass = 0;` |
|        - |  8031 | `	ph7_class_instance *pNew;` |
|     1300 |  8032 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8033 | `		/* Try to extract the desired class */` |
|     1949 |  8034 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1298 |  8035 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      649 |  8036 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8037 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8038 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8039 | `	}` |
|     1300 |  8040 | `	if( pClass == 0 ){` |
|        - |  8041 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8042 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8043 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8044 | `			);` |
|        - |  8045 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8046 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8047 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8048 | `			/* Pop given arguments */` |
|      ! 0 |  8049 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8050 | `		}` |
|      ! 0 |  8051 | `		goto Abort;` |
|      ! 0 |  8052 | `	}else{` |
|        - |  8053 | `		ph7_class_method *pCons;` |
|        - |  8054 | `		/* Create a new class instance */` |
|     1300 |  8055 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1300 |  8056 | `		if( pNew == 0 ){` |
|      ! 0 |  8057 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8058 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8059 | `				&pClass->sName` |
|        - |  8060 | `			);` |
|      ! 0 |  8061 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8062 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8063 | `				/* Pop given arguments */` |
|      ! 0 |  8064 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8065 | `			}` |
|      ! 0 |  8066 | `			break;` |
|        - |  8067 | `		}` |
|        - |  8068 | `		/* Check if a constructor is available */` |
|     1300 |  8069 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1300 |  8070 | `		if( pCons == 0 ){` |
|      928 |  8071 | `			SyString *pName = &pClass->sName;` |
|        - |  8072 | `			/* Check for a constructor with the same base class name */` |
|      928 |  8073 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      463 |  8074 | `		}` |
|     1300 |  8075 | `		if( pCons ){` |
|        - |  8076 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8077 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8078 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8079 | `			 * (including variadic string-key packing). */` |
|      374 |  8080 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8081 | `			sxi32 rcCons;` |
|      374 |  8082 | `			SySetReset(&aArg);` |
|      746 |  8083 | `			while( pArg < pTos ){` |
|      374 |  8084 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      374 |  8085 | `				pArg++;` |
|        2 |  8086 | `			}` |
|      374 |  8087 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8088 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8089 | `				sxu32 n;` |
|      114 |  8090 | `				n = SySetUsed(&aArg);` |
|        - |  8091 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8092 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8093 | `				 * after resolution). */` |
|      222 |  8094 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8095 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8096 | `					if( pFuncArg ){` |
|      110 |  8097 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8098 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8099 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8100 | `						}` |
|       54 |  8101 | `					}` |
|      110 |  8102 | `					n++;` |
|        2 |  8103 | `				}` |
|       56 |  8104 | `			}` |
|      374 |  8105 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8106 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      374 |  8107 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8108 | `				pNew->iRef = 1;` |
|      ! 0 |  8109 | `			}` |
|      374 |  8110 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8111 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8112 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8113 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8114 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8115 | `				sxi32 iResumePc;` |
|        5 |  8116 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8117 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8118 | `					goto Abort;` |
|        - |  8119 | `				}` |
|        5 |  8120 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8121 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8122 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8123 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8124 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8125 | `					}` |
|        5 |  8126 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8127 | `					pc = iResumePc;` |
|        5 |  8128 | `					break;` |
|        - |  8129 | `				}` |
|      ! 0 |  8130 | `				goto Exception;` |
|        - |  8131 | `			}` |
|      184 |  8132 | `		}` |
|     1296 |  8133 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8134 | `			/* Pop given arguments */` |
|      306 |  8135 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      152 |  8136 | `		}` |
|     1296 |  8137 | `		PH7_MemObjRelease(pTos);` |
|     1296 |  8138 | `		pTos->x.pOther = pNew;` |
|     1296 |  8139 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8140 | `	}` |
|     1296 |  8141 | `	break;` |
|        - |  8142 | `				 }` |
|        - |  8143 | `/*` |
|        - |  8144 | ` * OP_CLONE * * *` |
|        - |  8145 | ` * Perfome a clone operation.` |
|        - |  8146 | ` */` |
|       24 |  8147 | `case PH7_OP_CLONE: {` |
|        - |  8148 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8149 | `#ifdef UNTRUST` |
|        - |  8150 | `	if( pTos < pStack ){` |
|        - |  8151 | `		goto Abort;` |
|        - |  8152 | `	}` |
|        - |  8153 | `#endif` |
|        - |  8154 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8155 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8156 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8157 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8158 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8159 | `		break;` |
|        - |  8160 | `	}` |
|        - |  8161 | `	/* Point to the source */` |
|       46 |  8162 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8163 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8164 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8165 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8166 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8167 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8168 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8169 | `		break;` |
|        - |  8170 | `	}` |
|        - |  8171 | `	/* Perform the clone operation */` |
|       46 |  8172 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8173 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8174 | `	if( pClone == 0 ){` |
|      ! 0 |  8175 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8176 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8177 | `	}else{` |
|        - |  8178 | `		/* Load the cloned object */` |
|       46 |  8179 | `		pTos->x.pOther = pClone;` |
|       46 |  8180 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8181 | `	}` |
|       46 |  8182 | `	break;` |
|        - |  8183 | `				   }` |
|        - |  8184 | `/*` |
|        - |  8185 | ` * OP_SWITCH * * P3` |
|        - |  8186 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8187 | ` */` |
|       26 |  8188 | `case PH7_OP_SWITCH: {` |
|       54 |  8189 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8190 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8191 | `	ph7_value sValue,sCaseValue;` |
|        - |  8192 | `	sxu32 n,nEntry;` |
|        - |  8193 | `#ifdef UNTRUST` |
|        - |  8194 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8195 | `		goto Abort;` |
|        - |  8196 | `	}` |
|        - |  8197 | `#endif` |
|        - |  8198 | `	/* Point to the case table  */` |
|       54 |  8199 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8200 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8201 | `	/* Select the appropriate case block to execute */` |
|       54 |  8202 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8203 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8204 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8205 | `		pCase = &aCase[n];` |
|      130 |  8206 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8207 | `		/* Execute the case expression first */` |
|      130 |  8208 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8209 | `		/* Compare the two expression */` |
|      130 |  8210 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8211 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8212 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8213 | `		if( rc == 0 ){` |
|        - |  8214 | `			/* Value match,jump to this block */` |
|       52 |  8215 | `			pc = pCase->nStart - 1;` |
|       52 |  8216 | `			break;` |
|        - |  8217 | `		}` |
|       41 |  8218 | `	}` |
|       54 |  8219 | `	VmPopOperand(&pTos,1);` |
|       54 |  8220 | `	if( n >= nEntry ){` |
|        - |  8221 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8222 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8223 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8224 | `		}else{` |
|        - |  8225 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8226 | `			pc = pSwitch->nOut - 1;` |
|        - |  8227 | `		}` |
|        1 |  8228 | `	}` |
|       54 |  8229 | `	break;` |
|        - |  8230 | `					}` |
|        - |  8231 | `/*` |
|        - |  8232 | ` * OP_MATCH * * P3` |
|        - |  8233 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8234 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8235 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8236 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8237 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8238 | ` */` |
|       54 |  8239 | `case PH7_OP_MATCH: {` |
|      110 |  8240 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8241 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8242 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8243 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8244 | `	int matched = 0;` |
|        - |  8245 | `#ifdef UNTRUST` |
|        - |  8246 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8247 | `		goto Abort;` |
|        - |  8248 | `	}` |
|        - |  8249 | `#endif` |
|      110 |  8250 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8251 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8252 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8253 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8254 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8255 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8256 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8257 | `		pArm = &aArm[i];` |
|      240 |  8258 | `		if( pArm->bDefault ){` |
|       13 |  8259 | `			pDefault = pArm;` |
|       13 |  8260 | `			continue;` |
|        - |  8261 | `		}` |
|      228 |  8262 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8263 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8264 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8265 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8266 | `				continue;` |
|        - |  8267 | `			}` |
|      260 |  8268 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8269 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8270 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8271 | `			if( rc == 0 ){` |
|       93 |  8272 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8273 | `				matched = 1;` |
|       93 |  8274 | `				break;` |
|        - |  8275 | `			}` |
|       85 |  8276 | `		}` |
|      115 |  8277 | `	}` |
|      110 |  8278 | `	if( !matched && pDefault ){` |
|       13 |  8279 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8280 | `		matched = 1;` |
|        6 |  8281 | `	}` |
|      110 |  8282 | `	if( !matched ){` |
|        5 |  8283 | `		const char *zType = "unknown";` |
|        - |  8284 | `		char zMsg[128];` |
|        - |  8285 | `		sxu32 nMsg;` |
|        5 |  8286 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8287 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8288 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8289 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8290 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8291 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8292 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8293 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8294 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8295 | `		default: break;` |
|        - |  8296 | `		}` |
|        7 |  8297 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8298 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8299 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8300 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8301 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8302 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8303 | `		goto Abort;` |
|        - |  8304 | `	}` |
|      105 |  8305 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8306 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8307 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8308 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8309 | `	break;` |
|        - |  8310 | `					}` |
|        - |  8311 | `/*` |
|        - |  8312 | ` * OP_YIELD P1 P2 *` |
|        - |  8313 | ` *  Yield a value from a generator function.` |
|        - |  8314 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8315 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8316 | ` */` |
|       34 |  8317 | `case PH7_OP_YIELD: {` |
|        - |  8318 | `	ph7_generator *pGen;` |
|       70 |  8319 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8320 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8321 | `		goto Abort;` |
|        - |  8322 | `	}` |
|       70 |  8323 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8324 | `	if( pInstr->iP2 ){` |
|        - |  8325 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8326 | `#ifdef UNTRUST` |
|        - |  8327 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8328 | `#endif` |
|        7 |  8329 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8330 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8331 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8332 | `		VmPopOperand(&pTos, 1);` |
|        - |  8333 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8334 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8335 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8336 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8337 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8338 | `			}` |
|        1 |  8339 | `		}` |
|       67 |  8340 | `	}else if( pInstr->iP1 ){` |
|        - |  8341 | `		/* yield $value */` |
|        - |  8342 | `#ifdef UNTRUST` |
|        - |  8343 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8344 | `#endif` |
|       64 |  8345 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8346 | `		VmPopOperand(&pTos, 1);` |
|        - |  8347 | `		/* Auto-increment key */` |
|       64 |  8348 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8349 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8350 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8351 | `	}else{` |
|        - |  8352 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8353 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8354 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8355 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8356 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8357 | `	}` |
|        - |  8358 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8359 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8360 | `	goto Suspend;` |
|        - |  8361 |  |
|        - |  8362 | `/*` |
|        - |  8363 | ` * OP_CALL P1 * *` |
|        - |  8364 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8365 | ` *  function on the stack.` |
|        - |  8366 | ` */` |
|   356871 |  8367 | `case PH7_OP_CALL: {` |
|   713788 |  8368 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8369 | `	ph7_value *pArg;` |
|   713788 |  8370 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   713788 |  8371 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8372 | `	SyHashEntry *pEntry;` |
|        - |  8373 | `	SyString sName;` |
|        - |  8374 | `	/* Extract function name */` |
|   713788 |  8375 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8376 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8377 | `			ph7_value sResult;` |
|        - |  8378 | `			sxi32 rcArr;` |
|        3 |  8379 | `			SySetReset(&aArg);` |
|        3 |  8380 | `			while( pArg < pTos ){` |
|      ! 0 |  8381 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8382 | `				pArg++;` |
|      ! 0 |  8383 | `			}` |
|        3 |  8384 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8385 | `			/* May be a class instance and it's static method */` |
|        3 |  8386 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8387 | `			SySetReset(&aArg);` |
|        - |  8388 | `			/* Pop given arguments */` |
|        3 |  8389 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8390 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8391 | `			}` |
|        3 |  8392 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8393 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8394 | `				goto Abort;` |
|        - |  8395 | `			}` |
|        3 |  8396 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8397 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8398 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8399 | `				sxi32 iResumePc;` |
|        3 |  8400 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8401 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8402 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8403 | `					pc = iResumePc;` |
|        3 |  8404 | `					break;` |
|        - |  8405 | `				}` |
|      ! 0 |  8406 | `				goto Exception;` |
|        - |  8407 | `			}` |
|        - |  8408 | `			/* Copy result */` |
|      ! 0 |  8409 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8410 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8411 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8412 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8413 | `			ph7_value sResult;` |
|        - |  8414 | `			sxi32 rcInv;` |
|       84 |  8415 | `			SySetReset(&aArg);` |
|      200 |  8416 | `			while( pArg < pTos ){` |
|      118 |  8417 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8418 | `				pArg++;` |
|        2 |  8419 | `			}` |
|       84 |  8420 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8421 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8422 | `				(int)SySetUsed(&aArg),` |
|       82 |  8423 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8424 | `				&sResult,` |
|       82 |  8425 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8426 | `			SySetReset(&aArg);` |
|       84 |  8427 | `			if( nCallArgs > 0 ){` |
|       76 |  8428 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8429 | `			}` |
|       84 |  8430 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8431 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8432 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8433 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8434 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8435 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8436 | `				pThis->iRef++;` |
|       13 |  8437 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8438 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8439 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8440 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8441 | `					goto Abort;` |
|        - |  8442 | `				}` |
|        - |  8443 | `				{` |
|       13 |  8444 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8445 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8446 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8447 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8448 | `						break;` |
|        - |  8449 | `					}` |
|        - |  8450 | `				}` |
|      ! 0 |  8451 | `				goto Exception;` |
|        - |  8452 | `			}` |
|       72 |  8453 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8454 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8455 | `				goto Abort;` |
|        - |  8456 | `			}` |
|       72 |  8457 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8458 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8459 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8460 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8461 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8462 | `				sxi32 iResumePc;` |
|        7 |  8463 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8464 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8465 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8466 | `					pc = iResumePc;` |
|        5 |  8467 | `					break;` |
|        - |  8468 | `				}` |
|        3 |  8469 | `				goto Exception;` |
|        - |  8470 | `			}` |
|       66 |  8471 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8472 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8473 | `		}else{` |
|        - |  8474 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8475 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8476 | `			/* Pop given arguments */` |
|      ! 0 |  8477 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8478 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8479 | `			}` |
|        - |  8480 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8481 | `			PH7_MemObjRelease(pTos);` |
|        - |  8482 | `		}` |
|       66 |  8483 | `		break;` |
|        - |  8484 | `	}` |
|   713704 |  8485 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8486 | `	/* Check for a compiled function first.` |
|        - |  8487 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8488 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   713704 |  8489 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8490 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8491 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8492 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8493 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8494 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8495 | `	{` |
|   713704 |  8496 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   713704 |  8497 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8498 | `		const char *zFunc;` |
|        - |  8499 | `		const char *zEnd;` |
|        - |  8500 | `		const char *z;` |
|        - |  8501 | `		SyString sGlobal;` |
|       22 |  8502 | `		zFunc = sName.zString;` |
|       22 |  8503 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8504 | `		z = zEnd;` |
|        - |  8505 | `		/* Find last namespace separator */` |
|      194 |  8506 | `		while( z > zFunc ){` |
|      194 |  8507 | `			if( z[-1] == '\\' ){` |
|       22 |  8508 | `				break;` |
|        - |  8509 | `			}` |
|      174 |  8510 | `			z--;` |
|        2 |  8511 | `		}` |
|       22 |  8512 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8513 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8514 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8515 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8516 | `		}` |
|       10 |  8517 | `	}` |
|        - |  8518 | `	} /* end VmCallArgMap namespace scope */` |
|   713704 |  8519 | `	if( pEntry ){` |
|        - |  8520 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8521 | `		ph7_class_instance *pThis;` |
|        - |  8522 | `		ph7_value *pFrameStack;` |
|        - |  8523 | `		ph7_vm_func *pVmFunc;` |
|        - |  8524 | `		ph7_class *pSelf;` |
|        - |  8525 | `		VmFrame *pFrame;` |
|        - |  8526 | `		ph7_value *pObj;` |
|        - |  8527 | `		VmSlot sArg;` |
|        - |  8528 | `		sxu32 n;` |
|        - |  8529 | `		/* initialize fields */` |
|    18458 |  8530 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18458 |  8531 | `		pThis = 0;` |
|    18458 |  8532 | `		pSelf = 0;` |
|    18458 |  8533 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8534 | `			ph7_class_method *pMeth;` |
|        - |  8535 | `			/* Class method call */` |
|     3322 |  8536 | `			ph7_value *pTarget = &pTos[-1];` |
|     3322 |  8537 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8538 | `				/* Extract the 'this' pointer */` |
|     3322 |  8539 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8540 | `					/* Instance already loaded */` |
|     3232 |  8541 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3232 |  8542 | `					pThis->iRef++;` |
|     3232 |  8543 | `					pSelf = pThis->pClass;` |
|     1615 |  8544 | `				}` |
|     3322 |  8545 | `				if( pSelf == 0 ){` |
|       92 |  8546 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8547 | `						/* "Late Static Binding" class name */` |
|      128 |  8548 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8549 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8550 | `					}` |
|       92 |  8551 | `					if( pSelf == 0 ){` |
|       21 |  8552 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8553 | `					}` |
|       45 |  8554 | `				}` |
|     3322 |  8555 | `				if( pThis == 0  ){` |
|       92 |  8556 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8557 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8558 | `					if( pFrameLocal->pParent ){` |
|        - |  8559 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8560 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8561 | `						if( pThis ){` |
|       21 |  8562 | `							pThis->iRef++;` |
|       10 |  8563 | `						}` |
|       32 |  8564 | `					}` |
|       45 |  8565 | `				}` |
|     3322 |  8566 | `				VmPopOperand(&pTos,1);` |
|     3322 |  8567 | `				PH7_MemObjRelease(pTos);` |
|        - |  8568 | `				/* Synchronize pointers */` |
|     3322 |  8569 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8570 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8571 | `				 * user have already computed the random generated unique class method name` |
|        - |  8572 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8573 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8574 | `				 */` |
|     3322 |  8575 | `				while( pArg < pStack ){` |
|      ! 0 |  8576 | `					pArg++;` |
|      ! 0 |  8577 | `				}` |
|     3322 |  8578 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8579 | `					/* Check if the call is allowed */` |
|     3322 |  8580 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3322 |  8581 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8582 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8583 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8584 | `							char zMsg[256];` |
|      ! 0 |  8585 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8586 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8587 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8588 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8589 | `							/* Pop given arguments */` |
|      ! 0 |  8590 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8591 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8592 | `							}` |
|      ! 0 |  8593 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8594 | `							goto Abort;` |
|        - |  8595 | `						}` |
|        6 |  8596 | `					}` |
|     1660 |  8597 | `				}` |
|     1660 |  8598 | `			}` |
|     1660 |  8599 | `		}` |
|        - |  8600 | `		/* Check The recursion limit */` |
|    18458 |  8601 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8602 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8603 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8604 | `				&pVmFunc->sName);` |
|        - |  8605 | `			/* Pop given arguments */` |
|        3 |  8606 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8607 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8608 | `			}` |
|        - |  8609 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8610 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8611 | `			break;` |
|        - |  8612 | `		}` |
|    18456 |  8613 | `		if( pVmFunc->pNextName ){` |
|        - |  8614 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8615 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8616 | `		}` |
|    18456 |  8617 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8618 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8619 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8620 | `			ph7_generator *pGenerator;` |
|        - |  8621 | `			ph7_class_instance *pGenObj;` |
|        - |  8622 | `			ph7_value *pCtxAttr;` |
|        - |  8623 | `			SyString sAttrName;` |
|        - |  8624 | `			ph7_value **apCallArgs;` |
|        - |  8625 | `			int nGenArgs, iArg;` |
|        - |  8626 | `			/* Collect arguments from the operand stack */` |
|       24 |  8627 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8628 | `			apCallArgs = 0;` |
|       24 |  8629 | `			if( nGenArgs > 0 ){` |
|       14 |  8630 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8631 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8632 | `				if( apCallArgs == 0 ){` |
|        - |  8633 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8634 | `					nGenArgs = 0;` |
|      ! 0 |  8635 | `				}else{` |
|       10 |  8636 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8637 | `					int didReorder = 0;` |
|       10 |  8638 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8639 | `						/* Named-argument reordering for generator */` |
|        5 |  8640 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8641 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8642 | `						sxu32 nNV = nF;` |
|        5 |  8643 | `						sxi32 iVIdx = -1;` |
|        - |  8644 | `						sxi32 *aGSlot;` |
|        - |  8645 | `						sxu8 *aGUsed;` |
|        - |  8646 | `						sxu32 gi;` |
|       13 |  8647 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8648 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8649 | `						}` |
|        7 |  8650 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8651 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8652 | `						if( aGSlot ){` |
|        5 |  8653 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8654 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8655 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8656 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8657 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8658 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8659 | `								goto Abort;` |
|        - |  8660 | `							}` |
|        - |  8661 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8662 | `							 * append overflow (variadic / positional beyond` |
|        - |  8663 | `							 * formals) so downstream sees every argument. */` |
|        - |  8664 | `							{` |
|        5 |  8665 | `								int nOut = 0;` |
|       13 |  8666 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8667 | `									sxu32 gj;` |
|       13 |  8668 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8669 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8670 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8671 | `											break;` |
|        - |  8672 | `										}` |
|        3 |  8673 | `									}` |
|        5 |  8674 | `								}` |
|       13 |  8675 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8676 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8677 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8678 | `									}` |
|        5 |  8679 | `								}` |
|        5 |  8680 | `								nGenArgs = nOut;` |
|        - |  8681 | `							}` |
|        5 |  8682 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8683 | `							didReorder = 1;` |
|        2 |  8684 | `						}` |
|        - |  8685 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8686 | `						 * positional fill below — preserves arg order rather` |
|        - |  8687 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8688 | `					}` |
|       10 |  8689 | `					if( !didReorder ){` |
|       12 |  8690 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8691 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8692 | `						}` |
|        2 |  8693 | `					}` |
|        - |  8694 | `				}` |
|        4 |  8695 | `			}` |
|        - |  8696 | `			/* Create execution context and generator wrapper */` |
|       24 |  8697 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8698 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8699 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8700 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8701 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8702 | `				break;` |
|        - |  8703 | `			}` |
|       24 |  8704 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8705 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8706 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8707 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8708 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8709 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8710 | `				break;` |
|        - |  8711 | `			}` |
|        - |  8712 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8713 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8714 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8715 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8716 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8717 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8718 | `			if( apCallArgs ){` |
|       10 |  8719 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8720 | `			}` |
|       24 |  8721 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8722 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8723 | `				if( pThis ){` |
|      ! 0 |  8724 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8725 | `				}` |
|      ! 0 |  8726 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8727 | `					goto Abort;` |
|        - |  8728 | `				}` |
|      ! 0 |  8729 | `				break;` |
|        - |  8730 | `			}` |
|        - |  8731 | `			/* Create Generator class instance */` |
|       24 |  8732 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8733 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8734 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8735 | `				break;` |
|        - |  8736 | `			}` |
|        - |  8737 | `			/* Store generator in __ctx attribute */` |
|       24 |  8738 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8739 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8740 | `			if( pCtxAttr ){` |
|       24 |  8741 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8742 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8743 | `			}` |
|        - |  8744 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8745 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8746 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8747 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8748 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8749 | `			pGenObj->iRef++;` |
|       24 |  8750 | `			if( pThis ){` |
|      ! 0 |  8751 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8752 | `			}` |
|       24 |  8753 | `			break;` |
|        - |  8754 | `		}` |
|        - |  8755 | `		/* Extract the formal argument set */` |
|    18434 |  8756 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8757 | `		/* Create a new VM frame  */` |
|    18434 |  8758 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18434 |  8759 | `		if( rc != SXRET_OK ){` |
|        - |  8760 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8761 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8762 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8763 | `				&pVmFunc->sName);` |
|        - |  8764 | `			/* Pop given arguments */` |
|      ! 0 |  8765 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8766 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8767 | `			}` |
|        - |  8768 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8769 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8770 | `			break;` |
|        - |  8771 | `		}` |
|    18434 |  8772 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8773 | `			/* Install the '$this' variable */` |
|        - |  8774 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3250 |  8775 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3250 |  8776 | `			if( pObj ){` |
|        - |  8777 | `				/* Reflect the change */` |
|     3250 |  8778 | `				pObj->x.pOther = pThis;` |
|     3250 |  8779 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1624 |  8780 | `			}` |
|     1624 |  8781 | `		}` |
|    18434 |  8782 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8783 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8784 | `			/* Install static variables */` |
|      ! 0 |  8785 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8786 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8787 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8788 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8789 | `					/* Initialize the static variables */` |
|      ! 0 |  8790 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8791 | `					if( pObj ){` |
|        - |  8792 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8793 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8794 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8795 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8796 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8797 | `						}` |
|      ! 0 |  8798 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8799 | `					}else{` |
|      ! 0 |  8800 | `						continue;` |
|        - |  8801 | `					}` |
|      ! 0 |  8802 | `				}` |
|        - |  8803 | `				/* Install in the current frame */` |
|      ! 0 |  8804 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8805 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8806 | `			}` |
|      ! 0 |  8807 | `		}` |
|        - |  8808 | `		/* Push arguments in the local frame */` |
|        - |  8809 | `		{` |
|    18434 |  8810 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8811 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8812 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18434 |  8813 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18434 |  8814 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8815 | `			/* ============================================================` |
|        - |  8816 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8817 | `			 *` |
|        - |  8818 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8819 | `			 * or position, then install them in the frame.` |
|        - |  8820 | `			 * ============================================================ */` |
|       96 |  8821 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8822 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8823 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8824 | `			sxu32 nNonVariadic;` |
|        - |  8825 | `			sxi32 *aSlot;` |
|        - |  8826 | `			sxu8  *aUsed;` |
|        - |  8827 | `			sxu32 i;` |
|        - |  8828 | `			/* Find variadic parameter index */` |
|      292 |  8829 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8830 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8831 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8832 | `					break;` |
|        - |  8833 | `				}` |
|      100 |  8834 | `			}` |
|       96 |  8835 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8836 | `			/* Allocate mapping arrays */` |
|      143 |  8837 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8838 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8839 | `			if( aSlot == 0 ){` |
|      ! 0 |  8840 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8841 | `				goto Abort;` |
|        - |  8842 | `			}` |
|       96 |  8843 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8844 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8845 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8846 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8847 | `			if( rc == PH7_ABORT ){` |
|        7 |  8848 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8849 | `				goto Abort;` |
|        - |  8850 | `			}` |
|        - |  8851 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8852 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8853 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8854 | `				sxi32 iSrc = -1;` |
|      309 |  8855 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8856 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8857 | `						iSrc = (sxi32)i;` |
|      169 |  8858 | `						break;` |
|        - |  8859 | `					}` |
|       62 |  8860 | `				}` |
|      187 |  8861 | `				if( iSrc >= 0 ){` |
|        - |  8862 | `					/* Argument was provided — install with type checking */` |
|      169 |  8863 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8864 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8865 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8866 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8867 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8868 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8869 | `					}` |
|        - |  8870 | `					/* Type checking: union types */` |
|      169 |  8871 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8872 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8873 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8874 | `							bCallIsStrict);` |
|       13 |  8875 | `						if( rcU != SXRET_OK ){` |
|        - |  8876 | `							const char *zGiven;` |
|      ! 0 |  8877 | `							const char *zExpected = "union";` |
|        - |  8878 | `							char zBuf[128];` |
|        - |  8879 | `							char zTypeBuf[128];` |
|      ! 0 |  8880 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8881 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8882 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8883 | `								zGiven = "null";` |
|      ! 0 |  8884 | `							}else{` |
|      ! 0 |  8885 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8886 | `							}` |
|      ! 0 |  8887 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8888 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8889 | `							}` |
|      ! 0 |  8890 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8891 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8892 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8893 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8894 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8895 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8896 | `							pFrameStack = 0;` |
|      ! 0 |  8897 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8898 | `							goto SkipFuncBody;` |
|        - |  8899 | `						}` |
|      171 |  8900 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8901 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8902 | `						/* Scalar/class type checking */` |
|       17 |  8903 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8904 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8905 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8906 | `							if( pClass ){` |
|      ! 0 |  8907 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8908 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8909 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8910 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8911 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8912 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8913 | `									}` |
|      ! 0 |  8914 | `								}else{` |
|      ! 0 |  8915 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8916 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8917 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8918 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8919 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8920 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8921 | `									}` |
|        - |  8922 | `								}` |
|      ! 0 |  8923 | `							}` |
|       17 |  8924 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8925 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8926 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8927 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8928 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8929 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8930 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8931 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8932 | `								pFrameStack = 0;` |
|      ! 0 |  8933 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8934 | `								goto SkipFuncBody;` |
|        7 |  8935 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8936 | `								char zTypeBuf[128];` |
|      ! 0 |  8937 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8938 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8939 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8940 | `									ph7_type_name(pVal));` |
|      ! 0 |  8941 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8942 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8943 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8944 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8945 | `								pFrameStack = 0;` |
|      ! 0 |  8946 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8947 | `								goto SkipFuncBody;` |
|        - |  8948 | `							}` |
|        3 |  8949 | `						}` |
|        8 |  8950 | `					}` |
|        - |  8951 | `					/* Install: by reference or by value */` |
|      169 |  8952 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8953 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8954 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8955 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8956 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8957 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8958 | `							}` |
|      ! 0 |  8959 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8960 | `						}else{` |
|        7 |  8961 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8962 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8963 | `							if( pRefEntry == 0 ){` |
|        7 |  8964 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8965 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8966 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8967 | `								sArg.pUserData = 0;` |
|        5 |  8968 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8969 | `							}` |
|        5 |  8970 | `							pObj = 0;` |
|        - |  8971 | `						}` |
|        3 |  8972 | `					}else{` |
|      165 |  8973 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8974 | `					}` |
|      169 |  8975 | `					if( pObj ){` |
|      165 |  8976 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8977 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8978 | `						sArg.pUserData = 0;` |
|      165 |  8979 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8980 | `					}` |
|       85 |  8981 | `				}else{` |
|        - |  8982 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8983 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8984 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8985 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8986 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8987 | `						if( pObj ){` |
|       19 |  8988 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8989 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8990 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8991 | `							sArg.pUserData = 0;` |
|       19 |  8992 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8993 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8994 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8995 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8996 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8997 | `							}` |
|        9 |  8998 | `						}` |
|        9 |  8999 | `					}` |
|        - |  9000 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9001 | `				}` |
|       94 |  9002 | `			}` |
|        - |  9003 | `			/* Handle variadic parameter */` |
|       89 |  9004 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9005 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9006 | `				if( pObj ){` |
|        9 |  9007 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9008 | `					{` |
|        9 |  9009 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9010 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9011 | `							if( aSlot[i] == -1 ){` |
|       16 |  9012 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9013 | `									/* Named variadic entry: insert with string key */` |
|        - |  9014 | `									ph7_value sKey;` |
|       11 |  9015 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9016 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9017 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9018 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9019 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9020 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9021 | `								}else{` |
|        - |  9022 | `									/* Positional variadic entry */` |
|      ! 0 |  9023 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9024 | `								}` |
|        5 |  9025 | `							}` |
|       12 |  9026 | `						}` |
|        - |  9027 | `					}` |
|        9 |  9028 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9029 | `					sArg.pUserData = 0;` |
|        9 |  9030 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9031 | `				}` |
|        5 |  9032 | `			}else{` |
|        - |  9033 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9034 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9035 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9036 | `				 * the positional-only path's behavior. */` |
|       81 |  9037 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9038 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9039 | `					if( aSlot[i] == -2 ){` |
|        - |  9040 | `						char zAnonBuf[32];` |
|        - |  9041 | `						SyString sAnonName;` |
|      ! 0 |  9042 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9043 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9044 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9045 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9046 | `						if( pObj ){` |
|      ! 0 |  9047 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9048 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9049 | `							sArg.pUserData = 0;` |
|      ! 0 |  9050 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9051 | `						}` |
|      ! 0 |  9052 | `						nAnon++;` |
|      ! 0 |  9053 | `					}` |
|       79 |  9054 | `				}` |
|        - |  9055 | `			}` |
|        - |  9056 | `			/* Release all stack arguments */` |
|      267 |  9057 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9058 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9059 | `			}` |
|       89 |  9060 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9061 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9062 | `			n = nFormal;` |
|       45 |  9063 | `		}else{` |
|        - |  9064 | `		/* ============================================================` |
|        - |  9065 | `		 * Positional-only matching path (original)` |
|        - |  9066 | `		 * ============================================================ */` |
|    18340 |  9067 | `		n = 0;` |
|    48864 |  9068 | `		while( pArg < pTos ){` |
|    30598 |  9069 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9070 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9071 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9072 | `				if( pObj ){` |
|        - |  9073 | `					/* Initialize as empty array */` |
|       40 |  9074 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9075 | `					{` |
|       40 |  9076 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9077 | `						while( pArg < pTos ){` |
|        - |  9078 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9079 | `							 *` |
|        - |  9080 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9081 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9082 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9083 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9084 | `							 * fixing both wants a separate counter for elements` |
|        - |  9085 | `							 * already packed into the variadic array. */` |
|      114 |  9086 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9087 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9088 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9089 | `									bCallIsStrict);` |
|       16 |  9090 | `								if( rcU != SXRET_OK ){` |
|        - |  9091 | `									const char *zGiven;` |
|        3 |  9092 | `									const char *zExpected = "union";` |
|        - |  9093 | `									char zBuf[128];` |
|        - |  9094 | `									char zTypeBuf[128];` |
|        3 |  9095 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9096 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9097 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9098 | `										zGiven = "null";` |
|      ! 0 |  9099 | `									}else{` |
|        3 |  9100 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9101 | `									}` |
|        3 |  9102 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9103 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9104 | `									}` |
|        4 |  9105 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9106 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9107 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9108 | `										goto Abort;` |
|        - |  9109 | `									}` |
|        3 |  9110 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9111 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9112 | `									pFrameStack = 0;` |
|        3 |  9113 | `									rc = PH7_EXCEPTION;` |
|        3 |  9114 | `									goto SkipFuncBody;` |
|        - |  9115 | `								}` |
|       14 |  9116 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9117 | `								pArg++;` |
|       14 |  9118 | `								continue;` |
|        - |  9119 | `							}` |
|        - |  9120 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9121 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9122 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9123 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9124 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9125 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9126 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9127 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9128 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9129 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9130 | `										goto Abort;` |
|        - |  9131 | `									}` |
|        - |  9132 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9133 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9134 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9135 | `									pFrameStack = 0;` |
|      ! 0 |  9136 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9137 | `									goto SkipFuncBody;` |
|       13 |  9138 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9139 | `									char zTypeBuf[128];` |
|      ! 0 |  9140 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9141 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9142 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9143 | `										ph7_type_name(pArg));` |
|      ! 0 |  9144 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9145 | `										goto Abort;` |
|        - |  9146 | `									}` |
|      ! 0 |  9147 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9148 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9149 | `									pFrameStack = 0;` |
|      ! 0 |  9150 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9151 | `									goto SkipFuncBody;` |
|        - |  9152 | `								}` |
|        6 |  9153 | `							}` |
|      100 |  9154 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9155 | `							pArg++;` |
|        2 |  9156 | `						}` |
|        - |  9157 | `					}` |
|       38 |  9158 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9159 | `					sArg.pUserData = 0;` |
|       38 |  9160 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9161 | `				}` |
|       38 |  9162 | `				break; /* All remaining args consumed */` |
|        - |  9163 | `			}` |
|    30560 |  9164 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30342 |  9165 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9166 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9167 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9168 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9169 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9170 | `						goto Abort;` |
|        - |  9171 | `					}` |
|      ! 0 |  9172 | `				}` |
|        - |  9173 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30344 |  9174 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9175 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9176 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9177 | `						bCallIsStrict);` |
|       60 |  9178 | `					if( rcU != SXRET_OK ){` |
|        - |  9179 | `						const char *zGiven;` |
|       19 |  9180 | `						const char *zExpected = "union";` |
|        - |  9181 | `						char zBuf[128];` |
|        - |  9182 | `						char zTypeBuf[128];` |
|       19 |  9183 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9184 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9185 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9186 | `							zGiven = "null";` |
|        5 |  9187 | `						}else{` |
|        5 |  9188 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9189 | `						}` |
|       19 |  9190 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9191 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9192 | `						}` |
|       28 |  9193 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9194 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9195 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9196 | `							goto Abort;` |
|        - |  9197 | `						}` |
|       19 |  9198 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9199 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9200 | `						pFrameStack = 0;` |
|       19 |  9201 | `						rc = PH7_EXCEPTION;` |
|       19 |  9202 | `						goto SkipFuncBody;` |
|        - |  9203 | `					}` |
|       21 |  9204 | `				}else` |
|        - |  9205 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9206 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30310 |  9207 | `				if( aFormalArg[n].nType > 0` |
|    15859 |  9208 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9209 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9210 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9211 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9212 | `						ph7_class *pClass;` |
|        - |  9213 | `						/* Try to extract the desired class */` |
|       26 |  9214 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9215 | `						if( pClass ){` |
|       22 |  9216 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9217 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9218 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9219 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9220 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9221 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9222 | `								}` |
|      ! 0 |  9223 | `							}else{` |
|        - |  9224 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9225 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9226 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9227 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9228 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9229 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9230 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9231 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9232 | `								}` |
|        - |  9233 | `							}` |
|       12 |  9234 | `						}` |
|     1394 |  9235 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9236 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9237 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9238 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9239 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9240 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9241 | `								goto Abort;` |
|        - |  9242 | `							}` |
|        - |  9243 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9244 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9245 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9246 | `							pFrameStack = 0;` |
|       11 |  9247 | `							rc = PH7_EXCEPTION;` |
|       11 |  9248 | `							goto SkipFuncBody;` |
|       16 |  9249 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9250 | `							char zTypeBuf[128];` |
|       11 |  9251 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9252 | `								&aFormalArg[n].sName,` |
|        6 |  9253 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9254 | `								ph7_type_name(pArg));` |
|        8 |  9255 | `							if( rc == PH7_ABORT ){` |
|        5 |  9256 | `								goto Abort;` |
|        - |  9257 | `							}` |
|        3 |  9258 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9259 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9260 | `							pFrameStack = 0;` |
|        3 |  9261 | `							rc = PH7_EXCEPTION;` |
|        3 |  9262 | `							goto SkipFuncBody;` |
|        - |  9263 | `						}` |
|        4 |  9264 | `					}` |
|      694 |  9265 | `				}` |
|    30310 |  9266 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9267 | `					/* Pass by reference */` |
|       58 |  9268 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9269 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9270 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9271 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9272 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9273 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9274 | `						}` |
|        - |  9275 | `						/* Switch to pass by value */` |
|      ! 0 |  9276 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9277 | `					}else{` |
|        - |  9278 | `						SyHashEntry *pRefEntry;` |
|        - |  9279 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9280 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9281 | `						if( pRefEntry == 0 ){` |
|       86 |  9282 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9283 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9284 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9285 | `							sArg.pUserData = 0;` |
|       58 |  9286 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9287 | `						}` |
|       58 |  9288 | `						pObj = 0;` |
|        - |  9289 | `					}` |
|       30 |  9290 | `				}else{` |
|        - |  9291 | `					/* Pass by value,make a copy of the given argument */` |
|    30254 |  9292 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9293 | `				}` |
|    15156 |  9294 | `			}else{` |
|        - |  9295 | `				char zName[32];` |
|        - |  9296 | `				SyString sArgName;` |
|        - |  9297 | `				/* Set a dummy name */` |
|      218 |  9298 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9299 | `				sArgName.zString = zName;` |
|        - |  9300 | `				/* Annonymous argument */` |
|      218 |  9301 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9302 | `			}` |
|    30526 |  9303 | `			if( pObj ){` |
|    30470 |  9304 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9305 | `				/* Insert argument index  */` |
|    30470 |  9306 | `				sArg.nIdx = pObj->nIdx;` |
|    30470 |  9307 | `				sArg.pUserData = 0;` |
|    30470 |  9308 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15234 |  9309 | `			}` |
|    30526 |  9310 | `			PH7_MemObjRelease(pArg);` |
|    30526 |  9311 | `			pArg++;` |
|    30526 |  9312 | `			++n;` |
|        2 |  9313 | `		}` |
|        - |  9314 | `		} /* end named vs positional branch */` |
|        - |  9315 | `		/* Set up closure environment */` |
|    18392 |  9316 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9317 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9318 | `			ph7_value *pValue;` |
|        - |  9319 | `			sxu32 iEnv;` |
|      178 |  9320 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      422 |  9321 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      246 |  9322 | `				pEnv = &aEnv[iEnv];` |
|      246 |  9323 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9324 | `					/* Do not install null value */` |
|      172 |  9325 | `					continue;` |
|        - |  9326 | `				}` |
|       76 |  9327 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9328 | `				if( pValue == 0 ){` |
|      ! 0 |  9329 | `					continue;` |
|        - |  9330 | `				}` |
|        - |  9331 | `				/* Invalidate any prior representation */` |
|       76 |  9332 | `				PH7_MemObjRelease(pValue);` |
|        - |  9333 | `				/* Duplicate bound variable value */` |
|       76 |  9334 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9335 | `			}` |
|       88 |  9336 | `		}` |
|        - |  9337 | `		/* Process default values for remaining formal parameters */` |
|    21216 |  9338 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2872 |  9339 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9340 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9341 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9342 | `				if( pObj ){` |
|       48 |  9343 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9344 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9345 | `					sArg.pUserData = 0;` |
|       48 |  9346 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9347 | `				}` |
|       48 |  9348 | `				n++;` |
|       48 |  9349 | `				break; /* Variadic is always last */` |
|        - |  9350 | `			}` |
|     2826 |  9351 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2820 |  9352 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2820 |  9353 | `				if( pObj ){` |
|        - |  9354 | `					/* Evaluate the default value and extract it's result */` |
|     2820 |  9355 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2820 |  9356 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9357 | `						goto Abort;` |
|        - |  9358 | `					}` |
|        - |  9359 | `					/* Insert argument index */` |
|     2820 |  9360 | `					sArg.nIdx = pObj->nIdx;` |
|     2820 |  9361 | `					sArg.pUserData = 0;` |
|     2820 |  9362 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9363 | `					/* Make sure the default argument is of the correct type */` |
|     2818 |  9364 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1840 |  9365 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9366 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9367 | `						/* Cast to the desired type */` |
|        3 |  9368 | `						xCast(pObj);` |
|        1 |  9369 | `					}` |
|     1409 |  9370 | `				}` |
|     1409 |  9371 | `			}` |
|     2826 |  9372 | `			++n;` |
|        2 |  9373 | `		}` |
|        - |  9374 | `		} /* end VmCallArgMap scope */` |
|        - |  9375 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9376 | `		 * does not return anything.` |
|        - |  9377 | `		 */` |
|    18392 |  9378 | `		PH7_MemObjRelease(pTos);` |
|    18392 |  9379 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9380 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18392 |  9381 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18392 |  9382 | `		if( pFrameStack == 0 ){` |
|        - |  9383 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9384 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9385 | `				&pVmFunc->sName);` |
|      ! 0 |  9386 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9387 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9388 | `			}` |
|      ! 0 |  9389 | `			break;` |
|        - |  9390 | `		}` |
|     9195 |  9391 | `SkipFuncBody:` |
|    18424 |  9392 | `		if( pSelf ){` |
|        - |  9393 | `			/* Push class name */` |
|     3320 |  9394 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1659 |  9395 | `		}` |
|        - |  9396 | `		/* Increment nesting level */` |
|    18424 |  9397 | `		pVm->nRecursionDepth++;` |
|    18424 |  9398 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9399 | `			/* Execute function body */` |
|    27587 |  9400 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18390 |  9401 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9195 |  9402 | `		}` |
|        - |  9403 | `		/* Decrement nesting level */` |
|    18424 |  9404 | `		pVm->nRecursionDepth--;` |
|    18424 |  9405 | `		if( pSelf ){` |
|        - |  9406 | `			/* Pop class name */` |
|     3320 |  9407 | `			(void)SySetPop(&pVm->aSelf);` |
|     1659 |  9408 | `		}` |
|        - |  9409 | `		/* Cleanup the mess left behind */` |
|    18424 |  9410 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9411 | `			/* Return by reference,reflect that */` |
|        9 |  9412 | `			if( n != SXU32_HIGH ){` |
|        9 |  9413 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9414 | `				sxu32 i;` |
|        - |  9415 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9416 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9417 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9418 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9419 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9420 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9421 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9422 | `								&pVmFunc->sName);` |
|      ! 0 |  9423 | `						}` |
|      ! 0 |  9424 | `						n = SXU32_HIGH;` |
|      ! 0 |  9425 | `						break;` |
|        - |  9426 | `					}` |
|        3 |  9427 | `				}` |
|        5 |  9428 | `			}else{` |
|      ! 0 |  9429 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9430 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9431 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9432 | `						&pVmFunc->sName);` |
|      ! 0 |  9433 | `				}` |
|        - |  9434 | `			}` |
|        9 |  9435 | `			pTos->nIdx = n;` |
|        4 |  9436 | `		}` |
|        - |  9437 | `		/* Cleanup the mess left behind */` |
|    18424 |  9438 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9439 | `			/* An exception was throw in this frame */` |
|      100 |  9440 | `			pFrame = pFrame->pParent;` |
|      100 |  9441 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9442 | `				/* Pop the resutlt */` |
|       62 |  9443 | `				VmPopOperand(&pTos,1);` |
|        - |  9444 | `				/* Jump to this destination */` |
|       62 |  9445 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9446 | `				rc = PH7_OK;` |
|       32 |  9447 | `			}else{` |
|       39 |  9448 | `				if( pFrame->pParent ){` |
|       39 |  9449 | `					rc = PH7_EXCEPTION;` |
|       20 |  9450 | `				}else{` |
|        - |  9451 | `					/* Continue normal execution */` |
|      ! 0 |  9452 | `					rc = PH7_OK;` |
|        - |  9453 | `				}` |
|        - |  9454 | `			}` |
|       49 |  9455 | `		}` |
|        - |  9456 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18424 |  9457 | `		if( pFrameStack ){` |
|    18392 |  9458 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9195 |  9459 | `		}` |
|        - |  9460 | `		/* Leave the frame */` |
|    18424 |  9461 | `		VmLeaveFrame(&(*pVm));` |
|    18424 |  9462 | `		if( rc == PH7_ABORT ){` |
|        - |  9463 | `			/* Abort processing immeditaley */` |
|       17 |  9464 | `			goto Abort;` |
|    18408 |  9465 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9466 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9467 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9468 | `			 * overwriting the state saved by the inner level.` |
|        - |  9469 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9470 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9471 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9472 | `			goto Suspend;` |
|    18370 |  9473 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9474 | `			goto Exception;` |
|        - |  9475 | `		}` |
|     9167 |  9476 | `	}else{` |
|        - |  9477 | `		ph7_user_func *pFunc;` |
|        - |  9478 | `		ph7_context sCtx;` |
|        - |  9479 | `		ph7_value sRet;` |
|        - |  9480 | `		/* Look for an installed foreign function.` |
|        - |  9481 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9482 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9483 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9484 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   695248 |  9485 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9486 | `		{` |
|   695248 |  9487 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   695248 |  9488 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9489 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9490 | `			const char *zShort = sName.zString;` |
|        - |  9491 | `			sxu32 i;` |
|      334 |  9492 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9493 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9494 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9495 | `				}` |
|      158 |  9496 | `			}` |
|       22 |  9497 | `			if( zShort != sName.zString ){` |
|       22 |  9498 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9499 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9500 | `			}` |
|       10 |  9501 | `		}` |
|        - |  9502 | `		} /* end VmCallArgMap namespace scope */` |
|   695248 |  9503 | `		if( pEntry == 0 ){` |
|        - |  9504 | `			/* Call to undefined function */` |
|        5 |  9505 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9506 | `			/* Pop given arguments */` |
|        5 |  9507 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9508 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9509 | `			}` |
|        - |  9510 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9511 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9512 | `			break;` |
|        - |  9513 | `		}` |
|   695244 |  9514 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9515 | `		/* Start collecting function arguments */` |
|   695244 |  9516 | `		SySetReset(&aArg);` |
|  1874328 |  9517 | `		while( pArg < pTos ){` |
|  1179086 |  9518 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1179086 |  9519 | `			pArg++;` |
|        2 |  9520 | `		}` |
|        - |  9521 | `		/* Assume a null return value */` |
|   695244 |  9522 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9523 | `		/* Init the call context */` |
|   695244 |  9524 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9525 | `		/* Call the foreign function */` |
|   695244 |  9526 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9527 | `		/* Release the call context */` |
|   695244 |  9528 | `		VmReleaseCallContext(&sCtx);` |
|   695244 |  9529 | `		if( rc == PH7_ABORT ){` |
|        - |  9530 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - |  9531 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - |  9532 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      497 |  9533 | `			PH7_MemObjRelease(&sRet);` |
|      497 |  9534 | `			goto Abort;` |
|   694748 |  9535 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9536 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9537 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9538 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9539 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9540 | `				goto Exception;` |
|        - |  9541 | `			}` |
|        - |  9542 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9543 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9544 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9545 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9546 | `			}` |
|        - |  9547 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9548 | `			VmPopOperand(&pTos,1);` |
|        - |  9549 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9550 | `			pFrm = pVm->pFrame;` |
|      108 |  9551 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9552 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9553 | `			}` |
|      108 |  9554 | `			break;` |
|        - |  9555 | `		}` |
|   694638 |  9556 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9557 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9558 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9559 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9560 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9561 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9562 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9563 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9564 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9565 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9566 | `			}` |
|        - |  9567 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9568 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9569 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9570 | `			goto Suspend;` |
|        - |  9571 | `		}` |
|   694600 |  9572 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9573 | `			/* Pop function name and arguments */` |
|   672676 |  9574 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   336359 |  9575 | `		}` |
|        - |  9576 | `		/* Save foreign function return value */` |
|   694600 |  9577 | `		PH7_MemObjStore(&sRet,pTos);` |
|   694600 |  9578 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9579 | `	}` |
|   712930 |  9580 | `	break;` |
|        - |  9581 | `				  }` |
|        - |  9582 | `/*` |
|        - |  9583 | ` * OP_CONSUME: P1 * *` |
|        - |  9584 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9585 | ` */` |
|    15786 |  9586 | `case PH7_OP_CONSUME: {` |
|    31574 |  9587 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    31574 |  9588 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9589 |  |
|    31574 |  9590 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    31574 |  9591 | `	pCur = pOut;` |
|        - |  9592 | `	/* Start the consume process  */` |
|    63188 |  9593 | `	while( pOut <= pTos ){` |
|        - |  9594 | `		/* Force a string cast */` |
|    31616 |  9595 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1050 |  9596 | `			PH7_MemObjToString(pOut);` |
|      524 |  9597 | `		}` |
|    31616 |  9598 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9599 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9600 | `			/* Invoke the output consumer callback */` |
|    19232 |  9601 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19232 |  9602 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19232 |  9603 | `			SyBlobRelease(&pOut->sBlob);` |
|    19232 |  9604 | `			if( rc == SXERR_ABORT ){` |
|        - |  9605 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9606 | `				goto Abort;` |
|        - |  9607 | `			}` |
|     9615 |  9608 | `		}` |
|    31616 |  9609 | `		pOut++;` |
|        2 |  9610 | `	}` |
|    31574 |  9611 | `	pTos = &pCur[-1];` |
|    31572 |  9612 | `	break;` |
|        - |  9613 | `					 }` |
|        - |  9614 |  |
|        - |  9615 | `		} /* Switch() */` |
| 11746814 |  9616 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9617 | `	} /* For(;;) */` |
|    22036 |  9618 | `Done:` |
|    44074 |  9619 | `	SySetRelease(&aArg);` |
|    44074 |  9620 | `	return SXRET_OK;` |
|       72 |  9621 | `Suspend:` |
|      146 |  9622 | `	SySetRelease(&aArg);` |
|      146 |  9623 | `	return PH7_SUSPEND;` |
|      280 |  9624 | `Abort:` |
|      561 |  9625 | `	SySetRelease(&aArg);` |
|     1875 |  9626 | `	while( pTos >= pStack ){` |
|     1315 |  9627 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9628 | `		pTos--;` |
|        1 |  9629 | `	}` |
|      561 |  9630 | `	return PH7_ABORT;` |
|       29 |  9631 | `Exception:` |
|       60 |  9632 | `	SySetRelease(&aArg);` |
|      112 |  9633 | `	while( pTos >= pStack ){` |
|       54 |  9634 | `		PH7_MemObjRelease(pTos);` |
|       54 |  9635 | `		pTos--;` |
|        2 |  9636 | `	}` |
|       60 |  9637 | `	return PH7_EXCEPTION;` |
|    22419 |  9638 |  |
|        - |  9639 | `/*` |
|        - |  9640 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9641 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9642 | ` * See block-comment on that function for additional information.` |
|        - |  9643 | ` */` |
|    20428 |  9644 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9645 |  |
|        - |  9646 | `	ph7_value *pStack;` |
|        - |  9647 | `	sxi32 rc;` |
|        - |  9648 | `	/* Allocate a new operand stack */` |
|    20430 |  9649 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20430 |  9650 | `	if( pStack == 0 ){` |
|      ! 0 |  9651 | `		return SXERR_MEM;` |
|        - |  9652 | `	}` |
|        - |  9653 | `	/* Execute the program */` |
|    20430 |  9654 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9655 | `	/* Free the operand stack */` |
|    20430 |  9656 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9657 | `	/* Execution result */` |
|    20430 |  9658 | `	return rc;` |
|    10216 |  9659 |  |
|        - |  9660 | `/*` |
|        - |  9661 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9662 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9663 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9664 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9665 | ` * execution ends.` |
|        - |  9666 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9667 | ` * additional information.` |
|        - |  9668 | ` */` |
|     2820 |  9669 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9670 |  |
|        - |  9671 | `	VmShutdownCB *pEntry;` |
|        - |  9672 | `	ph7_value *apArg[10];` |
|        - |  9673 | `	sxu32 n,nEntry;` |
|        - |  9674 | `	int i;` |
|        - |  9675 | `	/* Point to the stack of registered callbacks */` |
|     2822 |  9676 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31022 |  9677 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28202 |  9678 | `		apArg[i] = 0;` |
|    14102 |  9679 | `	}` |
|        - |  9680 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - |  9681 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - |  9682 | `	 * callbacks, mirroring PHP.` |
|        - |  9683 | `	 */` |
|     2822 |  9684 | `	pVm->bHaltRequested = 0;` |
|     2832 |  9685 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 |  9686 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9687 | `		if( pEntry ){` |
|        - |  9688 | `			/* Prepare callback arguments if any */` |
|       12 |  9689 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9690 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9691 | `					break;` |
|        - |  9692 | `				}` |
|      ! 0 |  9693 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9694 | `			}` |
|        - |  9695 | `			/* Invoke the callback */` |
|       12 |  9696 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9697 | `			/*` |
|        - |  9698 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9699 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9700 | `			 */` |
|       12 |  9701 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 |  9702 | `			if( pEntry ){` |
|       12 |  9703 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 |  9704 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9705 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9706 | `				}` |
|        5 |  9707 | `			}` |
|       12 |  9708 | `			if( pVm->bHaltRequested ){` |
|        - |  9709 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 |  9710 | `				break;` |
|        - |  9711 | `			}` |
|        5 |  9712 | `		}` |
|        7 |  9713 | `	}` |
|     2822 |  9714 | `	SySetReset(&pVm->aShutdown);` |
|     2822 |  9715 |  |
|        - |  9716 | `/*` |
|        - |  9717 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9718 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9719 | ` * See block-comment on that function for additional information.` |
|        - |  9720 | ` */` |
|     2820 |  9721 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9722 |  |
|        - |  9723 | `	/* Make sure we are ready to execute this program */` |
|     2822 |  9724 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9725 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9726 | `	}` |
|        - |  9727 | `	/* Set the execution magic number  */` |
|     2822 |  9728 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9729 | `	/* Execute the program */` |
|     2822 |  9730 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9731 | `	/* Invoke any shutdown callbacks */` |
|     2822 |  9732 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9733 | `	/*` |
|        - |  9734 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9735 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9736 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9737 | `	 */` |
|     2822 |  9738 | `	return SXRET_OK;` |
|     1412 |  9739 |  |
|        - |  9740 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9741 | `/*` |
|        - |  9742 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9743 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9744 | ` */` |
|       46 |  9745 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9746 |  |
|        - |  9747 | `	ph7_exec_ctx *pCtx;` |
|        - |  9748 | `	ph7_value *pStack;` |
|        - |  9749 | `	VmFrame *pFrame;` |
|       48 |  9750 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9751 | `	if( pCtx == 0 ){` |
|      ! 0 |  9752 | `		return 0;` |
|        - |  9753 | `	}` |
|       48 |  9754 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9755 | `	pCtx->pVm = pVm;` |
|       48 |  9756 | `	pCtx->pFunc = pFunc;` |
|       48 |  9757 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9758 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9759 | `	pCtx->pc = 0;` |
|       48 |  9760 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9761 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9762 | `	/* Allocate a private operand stack */` |
|       48 |  9763 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9764 | `	if( pStack == 0 ){` |
|      ! 0 |  9765 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9766 | `		return 0;` |
|        - |  9767 | `	}` |
|       48 |  9768 | `	pCtx->pStack = pStack;` |
|        - |  9769 | `	/* Create a detached frame for the fiber */` |
|       48 |  9770 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9771 | `	if( pFrame == 0 ){` |
|      ! 0 |  9772 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9773 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9774 | `		return 0;` |
|        - |  9775 | `	}` |
|       48 |  9776 | `	pCtx->pFrame = pFrame;` |
|       48 |  9777 | `	return pCtx;` |
|       25 |  9778 |  |
|        - |  9779 | `/*` |
|        - |  9780 | ` * Start executing a fiber context for the first time.` |
|        - |  9781 | ` */` |
|       46 |  9782 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9783 |  |
|        - |  9784 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9785 | `	sxi32 rc;` |
|       48 |  9786 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9787 | `		return SXERR_INVALID;` |
|        - |  9788 | `	}` |
|        - |  9789 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9790 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9791 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9792 | `	/* Save and set the active context */` |
|       48 |  9793 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9794 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9795 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9796 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9797 | `	pVm->nRecursionDepth++;` |
|        - |  9798 | `	/* Execute from the beginning */` |
|       48 |  9799 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9800 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9801 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9802 | `	pVm->nRecursionDepth--;` |
|        - |  9803 | `	/* Restore the previous context */` |
|       48 |  9804 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9805 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9806 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9807 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9808 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9809 | `		if( pResult ){` |
|       24 |  9810 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9811 | `		}` |
|       46 |  9812 | `		return SXRET_OK;` |
|        - |  9813 | `	}` |
|        - |  9814 | `	/* Detach frame */` |
|        3 |  9815 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9816 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9817 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9818 | `	}` |
|        3 |  9819 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9820 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9821 | `		return PH7_ABORT;` |
|        - |  9822 | `	}` |
|        3 |  9823 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9824 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9825 | `		return PH7_EXCEPTION;` |
|        - |  9826 | `	}` |
|        - |  9827 | `	/* Normal completion */` |
|        3 |  9828 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9829 | `	if( pResult ){` |
|        3 |  9830 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9831 | `	}` |
|        3 |  9832 | `	return SXRET_OK;` |
|       25 |  9833 |  |
|        - |  9834 | `/*` |
|        - |  9835 | ` * Resume a suspended fiber context.` |
|        - |  9836 | ` */` |
|       98 |  9837 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9838 |  |
|        - |  9839 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9840 | `	sxi32 rc;` |
|      100 |  9841 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9842 | `		return SXERR_INVALID;` |
|        - |  9843 | `	}` |
|        - |  9844 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9845 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9846 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9847 | `	if( pResumeValue ){` |
|       40 |  9848 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9849 | `	}else{` |
|       62 |  9850 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9851 | `	}` |
|      100 |  9852 | `	pCtx->nTos++;` |
|        - |  9853 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9854 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9855 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9856 | `	/* Save and set the active context */` |
|      100 |  9857 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9858 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9859 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9860 | `	pVm->nRecursionDepth++;` |
|        - |  9861 | `	/* Resume execution from saved PC */` |
|      100 |  9862 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9863 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9864 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9865 | `	pVm->nRecursionDepth--;` |
|        - |  9866 | `	/* Restore the previous context */` |
|      100 |  9867 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9868 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9869 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9870 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9871 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9872 | `		if( pResult ){` |
|       18 |  9873 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9874 | `		}` |
|       64 |  9875 | `		return SXRET_OK;` |
|        - |  9876 | `	}` |
|        - |  9877 | `	/* Detach frame */` |
|       38 |  9878 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9879 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9880 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9881 | `	}` |
|       38 |  9882 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9883 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9884 | `		return PH7_ABORT;` |
|        - |  9885 | `	}` |
|       38 |  9886 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9887 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9888 | `		return PH7_EXCEPTION;` |
|        - |  9889 | `	}` |
|        - |  9890 | `	/* Normal completion */` |
|       38 |  9891 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9892 | `	if( pResult ){` |
|       20 |  9893 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9894 | `	}` |
|       38 |  9895 | `	return SXRET_OK;` |
|       51 |  9896 |  |
|        - |  9897 | `/*` |
|        - |  9898 | ` * Release an execution context and all its resources.` |
|        - |  9899 | ` */` |
|        4 |  9900 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9901 |  |
|        5 |  9902 | `	if( pCtx == 0 ){` |
|      ! 0 |  9903 | `		return;` |
|        - |  9904 | `	}` |
|        5 |  9905 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9906 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9907 | `		return;` |
|        - |  9908 | `	}` |
|        5 |  9909 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9910 | `	/* Release values */` |
|        5 |  9911 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9912 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9913 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9914 | `	if( pCtx->pFrame ){` |
|        - |  9915 | `		VmSlot *aSlot;` |
|        - |  9916 | `		sxu32 n;` |
|        - |  9917 | `		/* Free local variables */` |
|        5 |  9918 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9919 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9920 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9921 | `		}` |
|        - |  9922 | `		/* Remove local references */` |
|        5 |  9923 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9924 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9925 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9926 | `		}` |
|        5 |  9927 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9928 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9929 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9930 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9931 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9932 | `		pCtx->pFrame = 0;` |
|        2 |  9933 | `	}` |
|        - |  9934 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9935 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9936 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9937 | `	if( pCtx->pStack ){` |
|        5 |  9938 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9939 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9940 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9941 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9942 | `				pTos--;` |
|        1 |  9943 | `			}` |
|        2 |  9944 | `		}` |
|        5 |  9945 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9946 | `		pCtx->pStack = 0;` |
|        2 |  9947 | `	}` |
|        - |  9948 | `	/* Free the context itself */` |
|        5 |  9949 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9950 |  |
|        - |  9951 | `/*` |
|        - |  9952 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9953 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9954 | ` */` |
|       90 |  9955 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9956 |  |
|        - |  9957 | `	ph7_class_instance *pThis;` |
|        - |  9958 | `	SyString sAttr;` |
|        - |  9959 | `	ph7_value *pAttr;` |
|       92 |  9960 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9961 | `		return 0;` |
|        - |  9962 | `	}` |
|       92 |  9963 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9964 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9965 | `		return 0;` |
|        - |  9966 | `	}` |
|       92 |  9967 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9968 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9969 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9970 | `		return 0;` |
|        - |  9971 | `	}` |
|       62 |  9972 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9973 |  |
|        - |  9974 | `/*` |
|        - |  9975 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9976 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9977 | ` */` |
|       38 |  9978 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9979 |  |
|       40 |  9980 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9981 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9982 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9983 | `			"Cannot suspend outside of a fiber");` |
|        - |  9984 | `	}` |
|       40 |  9985 | `	if( nArg > 0 ){` |
|       40 |  9986 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9987 | `	}else{` |
|      ! 0 |  9988 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9989 | `	}` |
|       40 |  9990 | `	return PH7_SUSPEND;` |
|       21 |  9991 |  |
|        - |  9992 | `/*` |
|        - |  9993 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9994 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9995 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9996 | ` */` |
|       24 |  9997 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9998 |  |
|        - |  9999 | `	ph7_class_instance *pThis;` |
|        - | 10000 | `	ph7_value *pAttr;` |
|        - | 10001 | `	SyString sAttrName;` |
|       26 | 10002 | `	if( nArg < 2 ){` |
|      ! 0 | 10003 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10004 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10005 | `	}` |
|       26 | 10006 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10007 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10008 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10009 | `	}` |
|       26 | 10010 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10011 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10012 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10013 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10014 | `	}` |
|        - | 10015 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10016 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10017 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10018 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10019 | `	}` |
|        - | 10020 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10021 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10022 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10023 | `	if( pAttr ){` |
|       26 | 10024 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10025 | `	}` |
|       26 | 10026 | `	return PH7_OK;` |
|       14 | 10027 |  |
|        - | 10028 | `/*` |
|        - | 10029 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10030 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10031 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10032 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10033 | ` */` |
|       24 | 10034 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10035 | `	ph7_class_instance **ppThis)` |
|        2 | 10036 |  |
|       26 | 10037 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10038 | `	ph7_value *pCallable;` |
|        - | 10039 | `	SyString sAttrName;` |
|       26 | 10040 | `	*ppThis = 0;` |
|       26 | 10041 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10042 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10043 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10044 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10045 | `		return 0;` |
|        - | 10046 | `	}` |
|       26 | 10047 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10048 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10049 | `		SyString sName;` |
|        - | 10050 | `		SyHashEntry *pEntry;` |
|        - | 10051 | `		ph7_vm_func *pFunc;` |
|       26 | 10052 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10053 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10054 | `		if( pEntry == 0 ){` |
|      ! 0 | 10055 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10056 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10057 | `			return 0;` |
|        - | 10058 | `		}` |
|       26 | 10059 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10060 | `		return pFunc;` |
|      ! 0 | 10061 | `	}else{` |
|        - | 10062 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10063 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10064 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10065 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10066 | `		if( pMethod == 0 ){` |
|      ! 0 | 10067 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10068 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10069 | `			return 0;` |
|        - | 10070 | `		}` |
|      ! 0 | 10071 | `		*ppThis = pClosure;` |
|      ! 0 | 10072 | `		return &pMethod->sFunc;` |
|        - | 10073 | `	}` |
|       14 | 10074 |  |
|        - | 10075 | `/*` |
|        - | 10076 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10077 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10078 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10079 | ` */` |
|       46 | 10080 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10081 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10082 |  |
|       48 | 10083 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10084 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10085 | `	sxu32 nFormal, n;` |
|        - | 10086 | `	VmSlot sSlot;` |
|        - | 10087 | `	sxi32 rc;` |
|        - | 10088 | `	/* Install $this for closure/method callables */` |
|       48 | 10089 | `	if( pClosureThis ){` |
|        - | 10090 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10091 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10092 | `		if( pObj ){` |
|      ! 0 | 10093 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10094 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10095 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10096 | `		}` |
|      ! 0 | 10097 | `	}` |
|        - | 10098 | `	/* Install static variables */` |
|       48 | 10099 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10100 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10101 | `		ph7_value *pVal;` |
|      ! 0 | 10102 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10103 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10104 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10105 | `			if( pVal ){` |
|      ! 0 | 10106 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10107 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10108 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10109 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10110 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10111 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10112 | `				}` |
|      ! 0 | 10113 | `			}` |
|      ! 0 | 10114 | `		}` |
|      ! 0 | 10115 | `	}` |
|        - | 10116 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10117 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10118 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10119 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10120 | `		ph7_value *pObj;` |
|       20 | 10121 | `		if( n < (sxu32)nArg ){` |
|        - | 10122 | `			/* Argument provided — install with type casting */` |
|       20 | 10123 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10124 | `			if( pObj ){` |
|       20 | 10125 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10126 | `				/* Type casting */` |
|       20 | 10127 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10128 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10129 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10130 | `						if( xCast ){` |
|      ! 0 | 10131 | `							xCast(pObj);` |
|      ! 0 | 10132 | `						}` |
|      ! 0 | 10133 | `					}` |
|      ! 0 | 10134 | `				}` |
|       20 | 10135 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10136 | `				sSlot.pUserData = 0;` |
|       20 | 10137 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10138 | `			}` |
|        9 | 10139 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10140 | `			/* Default value */` |
|      ! 0 | 10141 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10142 | `			if( pObj ){` |
|      ! 0 | 10143 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10144 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10145 | `					return rc;` |
|        - | 10146 | `				}` |
|      ! 0 | 10147 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10148 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10149 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10150 | `						if( xCast ){` |
|      ! 0 | 10151 | `							xCast(pObj);` |
|      ! 0 | 10152 | `						}` |
|      ! 0 | 10153 | `					}` |
|      ! 0 | 10154 | `				}` |
|      ! 0 | 10155 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10156 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10157 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10158 | `			}` |
|      ! 0 | 10159 | `		}` |
|       11 | 10160 | `	}` |
|        - | 10161 | `	/* Install closure environment (captured variables) */` |
|       48 | 10162 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10163 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10164 | `		ph7_value *pValue;` |
|        - | 10165 | `		sxu32 iEnv;` |
|        3 | 10166 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10167 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10168 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10169 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10170 | `				continue;` |
|        - | 10171 | `			}` |
|        5 | 10172 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10173 | `			if( pValue == 0 ){` |
|      ! 0 | 10174 | `				continue;` |
|        - | 10175 | `			}` |
|        5 | 10176 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10177 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10178 | `		}` |
|        1 | 10179 | `	}` |
|       48 | 10180 | `	return SXRET_OK;` |
|       25 | 10181 |  |
|        - | 10182 | `/*` |
|        - | 10183 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10184 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10185 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10186 | ` */` |
|       26 | 10187 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10188 |  |
|       28 | 10189 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10190 | `	ph7_class_instance *pThis;` |
|        - | 10191 | `	ph7_class_instance *pClosureThis;` |
|        - | 10192 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10193 | `	ph7_vm_func *pFunc;` |
|        - | 10194 | `	ph7_value sResult;` |
|        - | 10195 | `	ph7_value *pCtxAttr;` |
|        - | 10196 | `	SyString sAttrName;` |
|        - | 10197 | `	sxi32 rc;` |
|       28 | 10198 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10199 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10200 | `	}` |
|       28 | 10201 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10202 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10203 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10204 | `	if( pExecCtx != 0 ){` |
|        3 | 10205 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10206 | `			"Cannot start a fiber that has already been started");` |
|        - | 10207 | `	}` |
|        - | 10208 | `	/* Resolve callable */` |
|       26 | 10209 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10210 | `	if( pFunc == 0 ){` |
|      ! 0 | 10211 | `		return PH7_EXCEPTION;` |
|        - | 10212 | `	}` |
|        - | 10213 | `	/* Create execution context now that we know the function */` |
|       26 | 10214 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10215 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10216 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10217 | `			"Fiber::start(): out of memory");` |
|        - | 10218 | `	}` |
|        - | 10219 | `	/* Store context in $this->__ctx */` |
|       26 | 10220 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10221 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10222 | `	if( pCtxAttr ){` |
|       26 | 10223 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10224 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10225 | `	}` |
|        - | 10226 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10227 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10228 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10229 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10230 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10231 | `	/* Unpack the args array and install into the frame */` |
|        - | 10232 | `	{` |
|       26 | 10233 | `		ph7_value **apValues = 0;` |
|       26 | 10234 | `		int nActual = 0;` |
|       26 | 10235 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10236 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10237 | `			ph7_hashmap_node *pNode;` |
|       26 | 10238 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10239 | `			if( nCount > 0 ){` |
|        3 | 10240 | `				sxu32 idx = 0;` |
|        4 | 10241 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10242 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10243 | `				if( apValues ){` |
|        3 | 10244 | `					pNode = pMap->pFirst;` |
|        7 | 10245 | `					while( pNode && idx < nCount ){` |
|        5 | 10246 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10247 | `						idx++;` |
|        5 | 10248 | `						pNode = pNode->pPrev;` |
|        1 | 10249 | `					}` |
|        3 | 10250 | `					nActual = (int)idx;` |
|        1 | 10251 | `				}` |
|        1 | 10252 | `			}` |
|       12 | 10253 | `		}` |
|       26 | 10254 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10255 | `		if( apValues ){` |
|        3 | 10256 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10257 | `		}` |
|        - | 10258 | `	}` |
|        - | 10259 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10260 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10261 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10262 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10263 | `		return PH7_ABORT;` |
|        - | 10264 | `	}` |
|       26 | 10265 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10266 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10267 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10268 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10269 | `		return PH7_ABORT;` |
|        - | 10270 | `	}` |
|       26 | 10271 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10272 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10273 | `		return PH7_EXCEPTION;` |
|        - | 10274 | `	}` |
|       26 | 10275 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10276 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10277 | `	return PH7_OK;` |
|       15 | 10278 |  |
|        - | 10279 | `/*` |
|        - | 10280 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10281 | ` */` |
|       36 | 10282 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10283 |  |
|       38 | 10284 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10285 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10286 | `	ph7_value sResult;` |
|        - | 10287 | `	ph7_value *pResumeVal;` |
|        - | 10288 | `	sxi32 rc;` |
|       38 | 10289 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10290 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10291 | `		return PH7_OK;` |
|        - | 10292 | `	}` |
|       38 | 10293 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10294 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10295 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10296 | `		return PH7_OK;` |
|        - | 10297 | `	}` |
|       38 | 10298 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10299 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10300 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10301 | `	}` |
|       36 | 10302 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10303 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10304 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10305 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10306 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10307 | `		return PH7_ABORT;` |
|        - | 10308 | `	}` |
|       36 | 10309 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10310 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10311 | `		return PH7_EXCEPTION;` |
|        - | 10312 | `	}` |
|       36 | 10313 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10314 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10315 | `	return PH7_OK;` |
|       20 | 10316 |  |
|        - | 10317 | `/*` |
|        - | 10318 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10319 | ` */` |
|        6 | 10320 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10321 |  |
|        8 | 10322 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10323 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10324 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10325 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10326 | `		return PH7_OK;` |
|        - | 10327 | `	}` |
|        8 | 10328 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10329 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10330 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10331 | `		return PH7_OK;` |
|        - | 10332 | `	}` |
|        8 | 10333 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10334 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10335 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10336 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10337 | `		}` |
|      ! 0 | 10338 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10339 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10340 | `	}` |
|        8 | 10341 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10342 | `	return PH7_OK;` |
|        5 | 10343 |  |
|        - | 10344 | `/*` |
|        - | 10345 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10346 | ` */` |
|        6 | 10347 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10348 |  |
|        - | 10349 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10350 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10351 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10352 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10353 | `	return PH7_OK;` |
|        4 | 10354 |  |
|      ! 0 | 10355 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10356 |  |
|        - | 10357 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10358 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10359 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10360 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10361 | `	return PH7_OK;` |
|      ! 0 | 10362 |  |
|        6 | 10363 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10364 |  |
|        - | 10365 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10366 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10367 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10368 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10369 | `	return PH7_OK;` |
|        4 | 10370 |  |
|        6 | 10371 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10372 |  |
|        - | 10373 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10374 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10375 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10376 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10377 | `	return PH7_OK;` |
|        4 | 10378 |  |
|        - | 10379 | `/*` |
|        - | 10380 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10381 | ` */` |
|        4 | 10382 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10383 |  |
|        5 | 10384 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10385 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10386 | `	if( nArg < 1 ){` |
|      ! 0 | 10387 | `		return PH7_OK;` |
|        - | 10388 | `	}` |
|        5 | 10389 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10390 | `	if( pExecCtx ){` |
|        5 | 10391 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10392 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10393 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10394 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10395 | `			SyString sAttrName;` |
|        - | 10396 | `			ph7_value *pAttr;` |
|        5 | 10397 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10398 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10399 | `			if( pAttr ){` |
|        5 | 10400 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10401 | `			}` |
|        2 | 10402 | `		}` |
|        2 | 10403 | `	}` |
|        5 | 10404 | `	return PH7_OK;` |
|        3 | 10405 |  |
|        - | 10406 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10407 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10408 |  |
|        - | 10409 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10410 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10411 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10412 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10413 |  |
|      ! 0 | 10414 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10415 |  |
|        - | 10416 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10417 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10418 | `	ph7_exec_ctx *pCtx;` |
|        - | 10419 | `	ph7_vm_func *pFunc;` |
|        - | 10420 | `	ph7_value *pCallable;` |
|        - | 10421 | `	ph7_value *pCtxAttr;` |
|        - | 10422 | `	SyString sAttrName;` |
|        - | 10423 | `	/* Must not already be started */` |
|      ! 0 | 10424 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10425 | `	if( pCtx != 0 ){` |
|      ! 0 | 10426 | `		return SXERR_INVALID;` |
|        - | 10427 | `	}` |
|      ! 0 | 10428 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10429 | `		return SXERR_INVALID;` |
|        - | 10430 | `	}` |
|      ! 0 | 10431 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10432 | `	/* Get the callable */` |
|      ! 0 | 10433 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10434 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10435 | `	if( pCallable == 0 ){` |
|      ! 0 | 10436 | `		return SXERR_INVALID;` |
|        - | 10437 | `	}` |
|        - | 10438 | `	/* Resolve callable */` |
|      ! 0 | 10439 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10440 | `		SyString sName;` |
|        - | 10441 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10442 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10443 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10444 | `		if( pEntry == 0 ){` |
|      ! 0 | 10445 | `			return SXERR_NOTFOUND;` |
|        - | 10446 | `		}` |
|      ! 0 | 10447 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10448 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10449 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10450 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10451 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10452 | `		if( pMethod == 0 ){` |
|      ! 0 | 10453 | `			return SXERR_INVALID;` |
|        - | 10454 | `		}` |
|      ! 0 | 10455 | `		pClosureThis = pClosure;` |
|      ! 0 | 10456 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10457 | `	}else{` |
|      ! 0 | 10458 | `		return SXERR_INVALID;` |
|        - | 10459 | `	}` |
|        - | 10460 | `	/* Create context */` |
|      ! 0 | 10461 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10462 | `	if( pCtx == 0 ){` |
|      ! 0 | 10463 | `		return SXERR_MEM;` |
|        - | 10464 | `	}` |
|        - | 10465 | `	/* Store in __ctx */` |
|      ! 0 | 10466 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10467 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10468 | `	if( pCtxAttr ){` |
|      ! 0 | 10469 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10470 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10471 | `	}` |
|        - | 10472 | `	/* Set up frame with args */` |
|      ! 0 | 10473 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10474 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10475 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10476 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10477 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10478 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10479 |  |
|      ! 0 | 10480 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10481 |  |
|      ! 0 | 10482 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10483 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10484 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10485 |  |
|      ! 0 | 10486 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10487 |  |
|      ! 0 | 10488 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10489 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10490 |  |
|      ! 0 | 10491 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10492 |  |
|      ! 0 | 10493 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10494 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10495 |  |
|      ! 0 | 10496 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10497 |  |
|      ! 0 | 10498 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10499 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10500 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10501 |  |
|        - | 10502 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10503 | `/*` |
|        - | 10504 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10505 | ` */` |
|       22 | 10506 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10507 |  |
|        - | 10508 | `	ph7_generator *pGen;` |
|       24 | 10509 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10510 | `	if( pGen == 0 ){` |
|      ! 0 | 10511 | `		return 0;` |
|        - | 10512 | `	}` |
|       24 | 10513 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10514 | `	pGen->pCtx = pCtx;` |
|       24 | 10515 | `	pGen->iImplicitKey = 0;` |
|       24 | 10516 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10517 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10518 | `	/* Link the generator back to the exec context */` |
|       24 | 10519 | `	pCtx->pPrivate = pGen;` |
|       24 | 10520 | `	return pGen;` |
|       13 | 10521 |  |
|        - | 10522 | `/*` |
|        - | 10523 | ` * Release a generator and its execution context.` |
|        - | 10524 | ` */` |
|      ! 0 | 10525 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10526 |  |
|      ! 0 | 10527 | `	if( pGen == 0 ){` |
|      ! 0 | 10528 | `		return;` |
|        - | 10529 | `	}` |
|      ! 0 | 10530 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10531 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10532 | `	if( pGen->pCtx ){` |
|      ! 0 | 10533 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10534 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10535 | `		pGen->pCtx = 0;` |
|      ! 0 | 10536 | `	}` |
|      ! 0 | 10537 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10538 |  |
|        - | 10539 | `/*` |
|        - | 10540 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10541 | ` */` |
|      236 | 10542 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10543 |  |
|        - | 10544 | `	ph7_class_instance *pThis;` |
|        - | 10545 | `	SyString sAttr;` |
|        - | 10546 | `	ph7_value *pAttr;` |
|      238 | 10547 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10548 | `		return 0;` |
|        - | 10549 | `	}` |
|      238 | 10550 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10551 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10552 | `		return 0;` |
|        - | 10553 | `	}` |
|      238 | 10554 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10555 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10556 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10557 | `		return 0;` |
|        - | 10558 | `	}` |
|      238 | 10559 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10560 |  |
|        - | 10561 | `/*` |
|        - | 10562 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10563 | ` */` |
|       22 | 10564 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10565 |  |
|        - | 10566 | `	ph7_generator *pGen;` |
|        - | 10567 | `	sxi32 rc;` |
|       24 | 10568 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10569 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10570 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10571 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10572 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10573 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10574 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10575 | `	}` |
|       24 | 10576 | `	return PH7_OK;` |
|       13 | 10577 |  |
|        - | 10578 | `/*` |
|        - | 10579 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10580 | ` */` |
|       68 | 10581 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10582 |  |
|        - | 10583 | `	ph7_generator *pGen;` |
|       70 | 10584 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10585 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10586 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10587 | `	return PH7_OK;` |
|       36 | 10588 |  |
|        - | 10589 | `/*` |
|        - | 10590 | ` * Generator::current() — return the last yielded value.` |
|        - | 10591 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10592 | ` */` |
|       68 | 10593 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10594 |  |
|        - | 10595 | `	ph7_generator *pGen;` |
|        - | 10596 | `	sxi32 rc;` |
|       70 | 10597 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10598 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10599 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10600 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10601 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10602 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10603 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10604 | `	}` |
|       70 | 10605 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10606 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10607 | `	}else{` |
|      ! 0 | 10608 | `		ph7_result_null(pCtx);` |
|        - | 10609 | `	}` |
|       70 | 10610 | `	return PH7_OK;` |
|       36 | 10611 |  |
|        - | 10612 | `/*` |
|        - | 10613 | ` * Generator::key() — return the last yielded key.` |
|        - | 10614 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10615 | ` */` |
|       12 | 10616 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10617 |  |
|        - | 10618 | `	ph7_generator *pGen;` |
|        - | 10619 | `	sxi32 rc;` |
|       13 | 10620 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10621 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10622 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10623 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10624 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10625 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10626 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10627 | `	}` |
|       13 | 10628 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10629 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10630 | `	}else{` |
|      ! 0 | 10631 | `		ph7_result_null(pCtx);` |
|        - | 10632 | `	}` |
|       13 | 10633 | `	return PH7_OK;` |
|        7 | 10634 |  |
|        - | 10635 | `/*` |
|        - | 10636 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10637 | ` */` |
|       60 | 10638 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10639 |  |
|        - | 10640 | `	ph7_generator *pGen;` |
|        - | 10641 | `	sxi32 rc;` |
|       62 | 10642 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10643 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10644 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10645 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10646 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10647 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10648 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10649 | `	}else{` |
|      ! 0 | 10650 | `		return PH7_OK;` |
|        - | 10651 | `	}` |
|       62 | 10652 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10653 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10654 | `	return PH7_OK;` |
|       32 | 10655 |  |
|        - | 10656 | `/*` |
|        - | 10657 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10658 | ` */` |
|        4 | 10659 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10660 |  |
|        - | 10661 | `	ph7_generator *pGen;` |
|        - | 10662 | `	ph7_value *pSendVal;` |
|        - | 10663 | `	sxi32 rc;` |
|        5 | 10664 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10665 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10666 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10667 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10668 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10669 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10670 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10671 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10672 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10673 | `	}else{` |
|      ! 0 | 10674 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10675 | `		return PH7_OK;` |
|        - | 10676 | `	}` |
|        5 | 10677 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10678 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10679 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10680 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10681 | `	}else{` |
|        3 | 10682 | `		ph7_result_null(pCtx);` |
|        - | 10683 | `	}` |
|        5 | 10684 | `	return PH7_OK;` |
|        3 | 10685 |  |
|        - | 10686 | `/*` |
|        - | 10687 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10688 | ` *` |
|        - | 10689 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10690 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10691 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10692 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10693 | ` * the exception to the caller.` |
|        - | 10694 | ` */` |
|      ! 0 | 10695 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10696 |  |
|        - | 10697 | `	ph7_generator *pGen;` |
|        - | 10698 | `	const char *zMsg;` |
|        - | 10699 | `	int nLen;` |
|      ! 0 | 10700 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10701 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10702 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10703 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10704 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10705 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10706 | `			"Cannot throw into a closed generator");` |
|        - | 10707 | `	}` |
|        - | 10708 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10709 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10710 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10711 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10712 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10713 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10714 | `	nLen = 0;` |
|      ! 0 | 10715 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10716 | `		/* Try to get the exception's message */` |
|        - | 10717 | `		SyString sAttr;` |
|        - | 10718 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10719 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10720 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10721 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10722 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10723 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10724 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10725 | `		}` |
|      ! 0 | 10726 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10727 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10728 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10729 | `	}` |
|      ! 0 | 10730 | `	(void)nLen;` |
|      ! 0 | 10731 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10732 |  |
|        - | 10733 | `/*` |
|        - | 10734 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10735 | ` */` |
|        2 | 10736 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10737 |  |
|        - | 10738 | `	ph7_generator *pGen;` |
|        3 | 10739 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10740 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10741 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10742 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10743 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10744 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10745 | `	}` |
|        3 | 10746 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10747 | `	return PH7_OK;` |
|        2 | 10748 |  |
|        - | 10749 | `/*` |
|        - | 10750 | ` * Generator::__destruct() — clean up.` |
|        - | 10751 | ` */` |
|      ! 0 | 10752 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10753 |  |
|        - | 10754 | `	ph7_generator *pGen;` |
|      ! 0 | 10755 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10756 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10757 | `	if( pGen ){` |
|      ! 0 | 10758 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10759 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10760 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10761 | `			SyString sAttrName;` |
|        - | 10762 | `			ph7_value *pAttr;` |
|      ! 0 | 10763 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10764 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10765 | `			if( pAttr ){` |
|      ! 0 | 10766 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10767 | `			}` |
|      ! 0 | 10768 | `		}` |
|      ! 0 | 10769 | `	}` |
|      ! 0 | 10770 | `	return PH7_OK;` |
|      ! 0 | 10771 |  |
|        - | 10772 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10773 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10774 | `/*` |
|        - | 10775 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10776 | ` * the desired message.` |
|        - | 10777 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10778 | ` * in 'api.c' for additional information.` |
|        - | 10779 | ` */` |
|      370 | 10780 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10781 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10782 | `	SyString *pString /* Message to output */` |
|        - | 10783 | `	)` |
|        2 | 10784 |  |
|      372 | 10785 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10786 | `	sxi32 rc = SXRET_OK;` |
|        - | 10787 | `	/* Call the output consumer */` |
|      372 | 10788 | `	if( pString->nByte > 0 ){` |
|      372 | 10789 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10790 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10791 | `	}` |
|      372 | 10792 | `	return rc;` |
|        2 | 10793 |  |
|        - | 10794 | `/*` |
|        - | 10795 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10796 | ` * callback to consume the formatted message.` |
|        - | 10797 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10798 | ` * in 'api.c' for additional information.` |
|        - | 10799 | ` */` |
|        2 | 10800 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10801 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10802 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10803 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10804 | `	)` |
|        1 | 10805 |  |
|        3 | 10806 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10807 | `	sxi32 rc = SXRET_OK;` |
|        - | 10808 | `	SyBlob sWorker;` |
|        - | 10809 | `	/* Format the message and call the output consumer */` |
|        3 | 10810 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10811 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10812 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10813 | `		/* Consume the formatted message */` |
|        3 | 10814 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10815 | `	}` |
|        3 | 10816 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10817 | `	/* Release the working buffer */` |
|        3 | 10818 | `	SyBlobRelease(&sWorker);` |
|        3 | 10819 | `	return rc;` |
|        1 | 10820 |  |
|        - | 10821 | `/*` |
|        - | 10822 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10823 | ` * This function never fail and always return a pointer` |
|        - | 10824 | ` * to a null terminated string.` |
|        - | 10825 | ` */` |
|       12 | 10826 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10827 |  |
|       13 | 10828 | `	const char *zOp = "Unknown     ";` |
|       13 | 10829 | `	switch(nOp){` |
|        3 | 10830 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10831 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10832 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10833 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10834 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10835 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10836 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10837 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10838 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10839 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10840 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10841 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10842 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10843 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10844 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10845 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10846 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10847 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10848 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10849 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10850 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10851 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10852 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10853 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10854 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10855 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10856 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10857 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10858 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10859 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10860 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10861 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10862 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10863 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10864 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10865 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10866 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10867 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10868 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10869 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10870 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10871 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10872 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10873 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10874 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10875 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10876 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10877 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10878 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10879 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10880 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10881 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10882 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10883 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10884 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10885 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10886 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10887 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10888 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10889 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10890 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10891 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10892 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10893 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10894 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10895 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10896 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10897 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10898 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10899 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10900 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10901 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10902 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10903 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10904 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10905 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10906 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10907 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10908 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10909 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10910 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10911 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10912 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10913 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10914 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10915 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10916 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10917 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10918 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10919 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10920 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10921 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10922 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10923 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10924 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10925 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10926 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10927 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10928 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10929 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10930 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10931 | `	default:` |
|      ! 0 | 10932 | `		break;` |
|        - | 10933 | `	}` |
|       13 | 10934 | `	return zOp;` |
|        1 | 10935 |  |
|        - | 10936 | `/*` |
|        - | 10937 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10938 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10939 | ` * is responsible of consuming the generated dump.` |
|        - | 10940 | ` */` |
|        2 | 10941 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10942 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10943 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10944 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10945 | `	)` |
|        1 | 10946 |  |
|        - | 10947 | `	sxi32 rc;` |
|        3 | 10948 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10949 | `	return rc;` |
|        1 | 10950 |  |
|        - | 10951 | `/*` |
|        - | 10952 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10953 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10954 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10955 | ` * in 'compile.c' for additional information.` |
|        - | 10956 | ` */` |
|       14 | 10957 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10958 |  |
|       15 | 10959 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10960 | `	/* Evaluate and expand constant value */` |
|       15 | 10961 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10962 |  |
|        - | 10963 | `/*` |
|        - | 10964 | ` * Section:` |
|        - | 10965 | ` *  Function handling functions.` |
|        - | 10966 | ` * Status:` |
|        - | 10967 | ` *    Stable.` |
|        - | 10968 | ` */` |
|        - | 10969 | `/*` |
|        - | 10970 | ` * int func_num_args(void)` |
|        - | 10971 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10972 | ` * Parameters` |
|        - | 10973 | ` *   None.` |
|        - | 10974 | ` * Return` |
|        - | 10975 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10976 | ` *  or -1 if called from the globe scope.` |
|        - | 10977 | ` */` |
|      980 | 10978 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10979 |  |
|        - | 10980 | `	VmFrame *pFrame;` |
|        - | 10981 | `	ph7_vm *pVm;` |
|        - | 10982 | `	/* Point to the target VM */` |
|      982 | 10983 | `	pVm = pCtx->pVm;` |
|        - | 10984 | `	/* Current frame */` |
|      982 | 10985 | `	pFrame = pVm->pFrame;` |
|      982 | 10986 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 10987 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10988 | `		SXUNUSED(nArg);` |
|      ! 0 | 10989 | `		SXUNUSED(apArg);` |
|        - | 10990 | `		/* Global frame,return -1 */` |
|      ! 0 | 10991 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10992 | `		return SXRET_OK;` |
|        - | 10993 | `	}` |
|        - | 10994 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 10995 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 10996 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 10997 | `	return SXRET_OK;` |
|      492 | 10998 |  |
|        - | 10999 | `/*` |
|        - | 11000 | ` * value func_get_arg(int $arg_num)` |
|        - | 11001 | ` *   Return an item from the argument list.` |
|        - | 11002 | ` * Parameters` |
|        - | 11003 | ` *  Argument number(index start from zero).` |
|        - | 11004 | ` * Return` |
|        - | 11005 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11006 | ` */` |
|       22 | 11007 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11008 |  |
|       24 | 11009 | `	ph7_value *pObj = 0;` |
|       24 | 11010 | `	VmSlot *pSlot = 0;` |
|        - | 11011 | `	VmFrame *pFrame;` |
|        - | 11012 | `	ph7_vm *pVm;` |
|        - | 11013 | `	/* Point to the target VM */` |
|       24 | 11014 | `	pVm = pCtx->pVm;` |
|        - | 11015 | `	/* Current frame */` |
|       24 | 11016 | `	pFrame = pVm->pFrame;` |
|       24 | 11017 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11018 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11019 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11020 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11021 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11022 | `		return SXRET_OK;` |
|        - | 11023 | `	}` |
|        - | 11024 | `	/* Extract the desired index */` |
|       21 | 11025 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11026 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11027 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11028 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11029 | `		return SXRET_OK;` |
|        - | 11030 | `	}` |
|        - | 11031 | `	/* Extract the desired argument */` |
|       21 | 11032 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11033 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11034 | `			/* Return the desired argument */` |
|       21 | 11035 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11036 | `		}else{` |
|        - | 11037 | `			/* No such argument,return false */` |
|      ! 0 | 11038 | `			ph7_result_bool(pCtx,0);` |
|        - | 11039 | `		}` |
|       11 | 11040 | `	}else{` |
|        - | 11041 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11042 | `		ph7_result_bool(pCtx,0);` |
|        - | 11043 | `	}` |
|       21 | 11044 | `	return SXRET_OK;` |
|       13 | 11045 |  |
|        - | 11046 | `/*` |
|        - | 11047 | ` * array func_get_args_byref(void)` |
|        - | 11048 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11049 | ` * Parameters` |
|        - | 11050 | ` *  None.` |
|        - | 11051 | ` * Return` |
|        - | 11052 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11053 | ` *  member of the current user-defined function's argument list.` |
|        - | 11054 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11055 | ` * NOTE:` |
|        - | 11056 | ` *  Arguments are returned to the array by reference.` |
|        - | 11057 | ` */` |
|        2 | 11058 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11059 |  |
|        - | 11060 | `	ph7_value *pArray;` |
|        - | 11061 | `	VmFrame *pFrame;` |
|        - | 11062 | `	VmSlot *aSlot;` |
|        - | 11063 | `	sxu32 n;` |
|        - | 11064 | `	/* Point to the current frame */` |
|        3 | 11065 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11066 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11067 | `	if( pFrame->pParent == 0 ){` |
|        - | 11068 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11069 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11070 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11071 | `		return SXRET_OK;` |
|        - | 11072 | `	}` |
|        - | 11073 | `	/* Create a new array */` |
|        3 | 11074 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11075 | `	if( pArray == 0 ){` |
|      ! 0 | 11076 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11077 | `		SXUNUSED(apArg);` |
|      ! 0 | 11078 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11079 | `		return SXRET_OK;` |
|        - | 11080 | `	}` |
|        - | 11081 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11082 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11083 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11084 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11085 | `	}` |
|        - | 11086 | `	/* Return the freshly created array */` |
|        3 | 11087 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11088 | `	return SXRET_OK;` |
|        2 | 11089 |  |
|        - | 11090 | `/*` |
|        - | 11091 | ` * array func_get_args(void)` |
|        - | 11092 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11093 | ` * Parameters` |
|        - | 11094 | ` *  None.` |
|        - | 11095 | ` * Return` |
|        - | 11096 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11097 | ` *  member of the current user-defined function's argument list.` |
|        - | 11098 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11099 | ` */` |
|       88 | 11100 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11101 |  |
|       90 | 11102 | `	ph7_value *pObj = 0;` |
|        - | 11103 | `	ph7_value *pArray;` |
|        - | 11104 | `	VmFrame *pFrame;` |
|        - | 11105 | `	VmSlot *aSlot;` |
|        - | 11106 | `	sxu32 n;` |
|        - | 11107 | `	/* Point to the current frame */` |
|       90 | 11108 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11109 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11110 | `	if( pFrame->pParent == 0 ){` |
|        - | 11111 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11112 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11113 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11114 | `		return SXRET_OK;` |
|        - | 11115 | `	}` |
|        - | 11116 | `	/* Create a new array */` |
|       90 | 11117 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11118 | `	if( pArray == 0 ){` |
|      ! 0 | 11119 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11120 | `		SXUNUSED(apArg);` |
|      ! 0 | 11121 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11122 | `		return SXRET_OK;` |
|        - | 11123 | `	}` |
|        - | 11124 | `	/* Start filling the array with the given arguments */` |
|       90 | 11125 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11126 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11127 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11128 | `		if( pObj ){` |
|      134 | 11129 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11130 | `		}` |
|       68 | 11131 | `	}` |
|        - | 11132 | `	/* Return the freshly created array */` |
|       90 | 11133 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11134 | `	return SXRET_OK;` |
|       46 | 11135 |  |
|        - | 11136 | `/*` |
|        - | 11137 | ` * bool function_exists(string $name)` |
|        - | 11138 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11139 | ` * Parameters` |
|        - | 11140 | ` *  The name of the desired function.` |
|        - | 11141 | ` * Return` |
|        - | 11142 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11143 | ` */` |
|     1742 | 11144 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11145 |  |
|        - | 11146 | `	const char *zName;` |
|        - | 11147 | `	ph7_vm *pVm;` |
|        - | 11148 | `	int nLen;` |
|        - | 11149 | `	int res;` |
|     1744 | 11150 | `	if( nArg < 1 ){` |
|        - | 11151 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11152 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11153 | `		return SXRET_OK;` |
|        - | 11154 | `	}` |
|        - | 11155 | `	/* Point to the target VM */` |
|     1744 | 11156 | `	pVm = pCtx->pVm;` |
|        - | 11157 | `	/* Extract the function name */` |
|     1744 | 11158 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11159 | `	/* Assume the function is not defined */` |
|     1744 | 11160 | `	res = 0;` |
|        - | 11161 | `	/* Perform the lookup */` |
|     2613 | 11162 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1738 | 11163 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11164 | `			/* Function is defined */` |
|      266 | 11165 | `			res = 1;` |
|      132 | 11166 | `	}` |
|     1744 | 11167 | `	ph7_result_bool(pCtx,res);` |
|     1744 | 11168 | `	return SXRET_OK;` |
|      873 | 11169 |  |
|        - | 11170 | `/*` |
|        - | 11171 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11172 | ` * [i.e: Whether it is callable or not].` |
|        - | 11173 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11174 | ` */` |
|    23476 | 11175 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11176 |  |
|    23478 | 11177 | `	int res = 0;` |
|    23478 | 11178 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11179 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11180 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11181 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11182 | `		 * standard PHP behavior. */` |
|       20 | 11183 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11184 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11185 | `			res = 1;` |
|       10 | 11186 | `		}` |
|        9 | 11187 | `		(void)CallInvoke;` |
|    23469 | 11188 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11189 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11190 | `		if( pMap->nEntry == 2 ){` |
|        - | 11191 | `			ph7_class *pClass;` |
|        - | 11192 | `			ph7_value *pV;` |
|        - | 11193 | `			/* Extract the target class */` |
|       12 | 11194 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11195 | `			if( pV ){` |
|       12 | 11196 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11197 | `				if( pClass ){` |
|        - | 11198 | `					ph7_class_method *pMethod;` |
|        - | 11199 | `					/* Extract the target method */` |
|       10 | 11200 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11201 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11202 | `						/* Perform the lookup */` |
|       10 | 11203 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11204 | `						if( pMethod ){` |
|        - | 11205 | `							/* Method is callable */` |
|        5 | 11206 | `							res = 1;` |
|        2 | 11207 | `						}` |
|        4 | 11208 | `					}` |
|        4 | 11209 | `				}` |
|        5 | 11210 | `			}` |
|        7 | 11211 | `		}` |
|    23447 | 11212 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11213 | `		const char *zName;` |
|        - | 11214 | `		int nLen;` |
|        - | 11215 | `		/* Extract the name */` |
|     5862 | 11216 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11217 | `		/* Perform the lookup */` |
|     5877 | 11218 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11219 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11220 | `				/* Function is callable */` |
|     5844 | 11221 | `				res = 1;` |
|     2921 | 11222 | `		}` |
|     2930 | 11223 | `	}` |
|    23478 | 11224 | `	return res;` |
|        2 | 11225 |  |
|        - | 11226 | `/*` |
|        - | 11227 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11228 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11229 | ` * Parameters` |
|        - | 11230 | ` * $name` |
|        - | 11231 | ` *    The callback function to check` |
|        - | 11232 | ` * $syntax_only` |
|        - | 11233 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11234 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11235 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11236 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11237 | ` *    a string.` |
|        - | 11238 | ` * Return` |
|        - | 11239 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11240 | ` */` |
|       20 | 11241 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11242 |  |
|        - | 11243 | `	ph7_vm *pVm;` |
|        - | 11244 | `	int res;` |
|       21 | 11245 | `	if( nArg < 1 ){` |
|        - | 11246 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11247 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11248 | `		return SXRET_OK;` |
|        - | 11249 | `	}` |
|        - | 11250 | `	/* Point to the target VM */` |
|       21 | 11251 | `	pVm = pCtx->pVm;` |
|        - | 11252 | `	/* Perform the requested operation */` |
|       21 | 11253 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11254 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11255 | `	return SXRET_OK;` |
|       11 | 11256 |  |
|        - | 11257 | `/*` |
|        - | 11258 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11259 | ` * defined below.` |
|        - | 11260 | ` */` |
|     1306 | 11261 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11262 |  |
|     1307 | 11263 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11264 | `	ph7_value sName;` |
|        - | 11265 | `	sxi32 rc;` |
|        - | 11266 | `	/* Prepare the function name for insertion */` |
|     1307 | 11267 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11268 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11269 | `	/* Perform the insertion */` |
|     1307 | 11270 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11271 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11272 | `	return rc;` |
|        1 | 11273 |  |
|        - | 11274 | `/*` |
|        - | 11275 | ` * array get_defined_functions(void)` |
|        - | 11276 | ` *  Returns an array of all defined functions.` |
|        - | 11277 | ` * Parameter` |
|        - | 11278 | ` *  None.` |
|        - | 11279 | ` * Return` |
|        - | 11280 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11281 | ` *  both built-in (internal) and user-defined.` |
|        - | 11282 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11283 | ` *  defined ones using $arr["user"].` |
|        - | 11284 | ` * Note:` |
|        - | 11285 | ` *  NULL is returned on failure.` |
|        - | 11286 | ` */` |
|        2 | 11287 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11288 |  |
|        - | 11289 | `	ph7_value *pArray,*pEntry;` |
|        - | 11290 | `	/* NOTE:` |
|        - | 11291 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11292 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11293 | `	 */` |
|        3 | 11294 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11295 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11296 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11297 | `		SXUNUSED(apArg);` |
|        - | 11298 | `		/* Return NULL */` |
|      ! 0 | 11299 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11300 | `		return SXRET_OK;` |
|        - | 11301 | `	}` |
|        3 | 11302 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11303 | `	if( pEntry == 0 ){` |
|        - | 11304 | `		/* Return NULL */` |
|      ! 0 | 11305 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11306 | `		return SXRET_OK;` |
|        - | 11307 | `	}` |
|        - | 11308 | `	/* Fill with the appropriate information */` |
|        3 | 11309 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11310 | `	/* Create the 'internal' index */` |
|        3 | 11311 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11312 | `	/* Create the user-func array */` |
|        3 | 11313 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11314 | `	if( pEntry == 0 ){` |
|        - | 11315 | `		/* Return NULL */` |
|      ! 0 | 11316 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11317 | `		return SXRET_OK;` |
|        - | 11318 | `	}` |
|        - | 11319 | `	/* Fill with the appropriate information */` |
|        3 | 11320 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11321 | `	/* Create the 'user' index */` |
|        3 | 11322 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11323 | `	/* Return the multi-dimensional array */` |
|        3 | 11324 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11325 | `	return SXRET_OK;` |
|        2 | 11326 |  |
|        - | 11327 | `/*` |
|        - | 11328 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11329 | ` *  Register a function for execution on shutdown.` |
|        - | 11330 | ` * Note` |
|        - | 11331 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11332 | ` *  be called in the same order as they were registered.` |
|        - | 11333 | ` * Parameters` |
|        - | 11334 | ` *  $callback` |
|        - | 11335 | ` *   The shutdown callback to register.` |
|        - | 11336 | ` * $param` |
|        - | 11337 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11338 | ` * Return` |
|        - | 11339 | ` *  Nothing.` |
|        - | 11340 | ` */` |
|       10 | 11341 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11342 |  |
|        - | 11343 | `	VmShutdownCB sEntry;` |
|        - | 11344 | `	int i,j;` |
|       12 | 11345 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11346 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11347 | `		return PH7_OK;` |
|        - | 11348 | `	}` |
|        - | 11349 | `	/* Zero the Entry */` |
|       12 | 11350 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11351 | `	/* Initialize fields */` |
|       12 | 11352 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11353 | `	/* Save the callback name for later invocation name */` |
|       12 | 11354 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11355 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11356 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11357 | `	}` |
|        - | 11358 | `	/* Copy arguments */` |
|       12 | 11359 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11360 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11361 | `			/* Limit reached */` |
|      ! 0 | 11362 | `			break;` |
|        - | 11363 | `		}` |
|      ! 0 | 11364 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11365 | `	}` |
|       12 | 11366 | `	sEntry.nArg = j;` |
|        - | 11367 | `	/* Install the callback */` |
|       12 | 11368 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11369 | `	return PH7_OK;` |
|        7 | 11370 |  |
|        - | 11371 | `/*` |
|        - | 11372 | ` * Section:` |
|        - | 11373 | ` *  Class handling functions.` |
|        - | 11374 | ` * Status:` |
|        - | 11375 | ` *    Stable.` |
|        - | 11376 | ` */` |
|        - | 11377 | `/*` |
|        - | 11378 | ` * Extract the top active class. NULL is returned` |
|        - | 11379 | ` * if the class stack is empty.` |
|        - | 11380 | ` */` |
|      960 | 11381 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11382 |  |
|      962 | 11383 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11384 | `	ph7_class **apClass;` |
|      962 | 11385 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11386 | `		/* Empty stack,return NULL */` |
|       15 | 11387 | `		return 0;` |
|        - | 11388 | `	}` |
|        - | 11389 | `	/* Peek the last entry */` |
|      948 | 11390 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      948 | 11391 | `	return apClass[pSet->nUsed - 1];` |
|      482 | 11392 |  |
|        - | 11393 | `/*` |
|        - | 11394 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11395 | ` *   Get the class that declared the currently executing method.` |
|        - | 11396 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11397 | ` *` |
|        - | 11398 | ` * Parameters` |
|        - | 11399 | ` *   pVm: Target VM` |
|        - | 11400 | ` *` |
|        - | 11401 | ` * Return` |
|        - | 11402 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11403 | ` *   - Not executing within a class method` |
|        - | 11404 | ` *` |
|        - | 11405 | ` * Note` |
|        - | 11406 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11407 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11408 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11409 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11410 | ` *   declaring class.` |
|        - | 11411 | ` */` |
|       98 | 11412 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11413 |  |
|      100 | 11414 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11415 | `	ph7_vm_func *pVmFunc;` |
|        - | 11416 |  |
|        - | 11417 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11418 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11419 |  |
|        - | 11420 | `	/* Check if we're in a method context */` |
|      100 | 11421 | `	if( pFrame->pParent ){` |
|       96 | 11422 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11423 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11424 | `			/* Return the declaring class */` |
|       96 | 11425 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11426 | `		}` |
|      ! 0 | 11427 | `	}` |
|        - | 11428 |  |
|        5 | 11429 | `	return 0;` |
|       51 | 11430 |  |
|        - | 11431 |  |
|        - | 11432 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11433 | `/*` |
|        - | 11434 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11435 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11436 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11437 | ` * return value indicates failure.` |
|        - | 11438 | ` */` |
|        - | 11439 | `/*` |
|        - | 11440 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11441 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11442 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11443 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11444 | ` */` |
|     2456 | 11445 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11446 | `	ph7_vm *pVm,` |
|        - | 11447 | `	ph7_class_instance *pThis,` |
|        - | 11448 | `	ph7_class_method *pMethod,` |
|        - | 11449 | `	ph7_value *pResult,` |
|        - | 11450 | `	int nArg,` |
|        - | 11451 | `	ph7_value **apArg,` |
|        - | 11452 | `	VmCallArgMap *pMap` |
|        - | 11453 | `	)` |
|        2 | 11454 |  |
|        - | 11455 | `	ph7_value *aStack;` |
|        - | 11456 | `	VmInstr aInstr[2];` |
|        - | 11457 | `	int iCursor;` |
|        - | 11458 | `	int i;` |
|        - | 11459 | `	sxi32 rc;` |
|     2458 | 11460 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2458 | 11461 | `	if( aStack == 0 ){` |
|      ! 0 | 11462 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11463 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11464 | `		return SXERR_MEM;` |
|        - | 11465 | `	}` |
|     3992 | 11466 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1536 | 11467 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1536 | 11468 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      769 | 11469 | `	}` |
|     2458 | 11470 | `	iCursor = nArg + 1;` |
|     2458 | 11471 | `	if( pThis ){` |
|     2452 | 11472 | `		pThis->iRef++;` |
|     2452 | 11473 | `		aStack[i].x.pOther = pThis;` |
|     2452 | 11474 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1225 | 11475 | `	}` |
|     2458 | 11476 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2458 | 11477 | `	i++;` |
|     2458 | 11478 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2458 | 11479 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2458 | 11480 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2458 | 11481 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2458 | 11482 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2458 | 11483 | `	aInstr[0].iP1 = nArg;` |
|     2458 | 11484 | `	aInstr[0].iP2 = 0;` |
|     2458 | 11485 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2458 | 11486 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2458 | 11487 | `	aInstr[1].iP1 = 1;` |
|     2458 | 11488 | `	aInstr[1].iP2 = 0;` |
|     2458 | 11489 | `	aInstr[1].p3  = 0;` |
|     2458 | 11490 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2458 | 11491 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11492 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11493 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2458 | 11494 | `	return rc;` |
|     1230 | 11495 |  |
|     1922 | 11496 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11497 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11498 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11499 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11500 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11501 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11502 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11503 | `	)` |
|        2 | 11504 |  |
|     1924 | 11505 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11506 |  |
|        - | 11507 | `/*` |
|        - | 11508 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11509 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11510 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11511 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11512 | ` *` |
|        - | 11513 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11514 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11515 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11516 | ` *` |
|        - | 11517 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11518 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11519 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11520 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11521 | ` *` |
|        - | 11522 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11523 | ` */` |
|      174 | 11524 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11525 | `	ph7_vm *pVm,` |
|        - | 11526 | `	ph7_class_instance *pThis,` |
|        - | 11527 | `	int nArg,` |
|        - | 11528 | `	ph7_value **apArg,` |
|        - | 11529 | `	ph7_value *pResult,` |
|        - | 11530 | `	VmCallArgMap *pMap` |
|        - | 11531 | `	)` |
|        2 | 11532 |  |
|        - | 11533 | `	ph7_class_method *pMethod;` |
|      176 | 11534 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11535 | `	if( pMethod == 0 ){` |
|       13 | 11536 | `		if( pResult ){` |
|       13 | 11537 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11538 | `		}` |
|       13 | 11539 | `		return SXERR_INVALID;` |
|        - | 11540 | `	}` |
|      164 | 11541 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11542 |  |
|        - | 11543 | `/*` |
|        - | 11544 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11545 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11546 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11547 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11548 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11549 | ` * lookup or 'goto Exception').` |
|        - | 11550 | ` *` |
|        - | 11551 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11552 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11553 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11554 | ` * reported.` |
|        - | 11555 | ` */` |
|       12 | 11556 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11557 |  |
|        - | 11558 | `	ph7_class *pErrorClass;` |
|       13 | 11559 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11560 | `	ph7_class_method *pCons;` |
|        - | 11561 | `	VmFrame *pThrowFrame;` |
|        - | 11562 | `	char zMsg[256];` |
|        - | 11563 | `	int nMsg;` |
|        - | 11564 | `	sxi32 rc;` |
|       25 | 11565 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11566 | `		"Object of type %.*s is not callable",` |
|       12 | 11567 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11568 | `		pThis->pClass->sName.zString);` |
|       13 | 11569 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11570 | `	if( pErrorClass ){` |
|       13 | 11571 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11572 | `	}` |
|       13 | 11573 | `	if( pErrInst == 0 ){` |
|        - | 11574 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11575 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11576 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11577 | `		 * visible to the user. */` |
|      ! 0 | 11578 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11579 | `		return SXERR_ABORT;` |
|        - | 11580 | `	}` |
|       13 | 11581 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11582 | `	if( pCons ){` |
|        - | 11583 | `		ph7_value sArg;` |
|        - | 11584 | `		ph7_value *apMsg[1];` |
|        - | 11585 | `		SyString sMsgStr;` |
|       13 | 11586 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11587 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11588 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11589 | `		apMsg[0] = &sArg;` |
|       13 | 11590 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11591 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11592 | `	}` |
|        - | 11593 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11594 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11595 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11596 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11597 | `	if( pThrowFrame ){` |
|       13 | 11598 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11599 | `	}` |
|       13 | 11600 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11601 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11602 | `	return rc;` |
|        7 | 11603 |  |
|        - | 11604 | `/*` |
|        - | 11605 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11606 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11607 | ` * in the apArg[] array.` |
|        - | 11608 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11609 | ` * return value indicates failure.` |
|        - | 11610 | ` */` |
|     1212 | 11611 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11612 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11613 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11614 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11615 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11616 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11617 | `	)` |
|        2 | 11618 |  |
|        - | 11619 | `	ph7_value *aStack;` |
|        - | 11620 | `	VmInstr aInstr[2];` |
|        - | 11621 | `	int i;` |
|     1214 | 11622 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11623 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11624 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11625 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 11626 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 11627 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 11628 | `			nArg,apArg,pResult,0);` |
|        - | 11629 | `	}` |
|     1122 | 11630 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11631 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11632 | `		if( pResult ){` |
|        - | 11633 | `			/* Assume a null return value */` |
|      ! 0 | 11634 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11635 | `		}` |
|      511 | 11636 | `		return SXERR_INVALID;` |
|        - | 11637 | `	}` |
|      612 | 11638 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11639 | `		/* Class method */` |
|       15 | 11640 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 11641 | `		ph7_class_method *pMethod = 0;` |
|       15 | 11642 | `		ph7_class_instance *pThis = 0;` |
|       15 | 11643 | `		ph7_class *pClass = 0;` |
|        - | 11644 | `		ph7_value *pValue;` |
|        - | 11645 | `		sxi32 rc;` |
|       15 | 11646 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11647 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11648 | `			if( pResult ){` |
|        - | 11649 | `				/* Assume a null return value */` |
|      ! 0 | 11650 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11651 | `			}` |
|      ! 0 | 11652 | `			return SXRET_OK;` |
|        - | 11653 | `		}` |
|        - | 11654 | `		/* Extract the class name or an instance of it */` |
|       15 | 11655 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 11656 | `		if( pValue ){` |
|       15 | 11657 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 11658 | `		}` |
|       15 | 11659 | `		if( pClass == 0 ){` |
|        - | 11660 | `			/* No such class,return NULL */` |
|      ! 0 | 11661 | `			if( pResult ){` |
|      ! 0 | 11662 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11663 | `			}` |
|      ! 0 | 11664 | `			return SXRET_OK;` |
|        - | 11665 | `		}` |
|       15 | 11666 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11667 | `			/* Point to the class instance */` |
|        9 | 11668 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 11669 | `		}` |
|        - | 11670 | `		/* Try to extract the method */` |
|       15 | 11671 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 11672 | `		if( pValue ){` |
|       15 | 11673 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 11674 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 11675 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 11676 | `			}` |
|        7 | 11677 | `		}` |
|       15 | 11678 | `		if( pMethod == 0 ){` |
|        - | 11679 | `			/* No such method,return NULL */` |
|      ! 0 | 11680 | `			if( pResult ){` |
|      ! 0 | 11681 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11682 | `			}` |
|      ! 0 | 11683 | `			return SXRET_OK;` |
|        - | 11684 | `		}` |
|        - | 11685 | `		/* Call the class method */` |
|       15 | 11686 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 11687 | `		return rc;` |
|        - | 11688 | `	}` |
|        - | 11689 | `	/* Create a new operand stack */` |
|      598 | 11690 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 11691 | `	if( aStack == 0 ){` |
|      ! 0 | 11692 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11693 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11694 | `		if( pResult ){` |
|        - | 11695 | `			/* Assume a null return value */` |
|      ! 0 | 11696 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11697 | `		}` |
|      ! 0 | 11698 | `		return SXERR_MEM;` |
|        - | 11699 | `	}` |
|        - | 11700 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 11701 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 11702 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11703 | `		/*` |
|        - | 11704 | `		 * Symisc eXtension:` |
|        - | 11705 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11706 | `		 */` |
|     1304 | 11707 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 11708 | `	}` |
|        - | 11709 | `	/* Push the function name */` |
|      598 | 11710 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 11711 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11712 | `	/* Emit the CALL istruction */` |
|      598 | 11713 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 11714 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 11715 | `	aInstr[0].iP2 = 0;` |
|      598 | 11716 | `	aInstr[0].p3  = 0;` |
|        - | 11717 | `	/* Emit the DONE instruction */` |
|      598 | 11718 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 11719 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 11720 | `	aInstr[1].iP2 = 0;` |
|      598 | 11721 | `	aInstr[1].p3  = 0;` |
|        - | 11722 | `	/* Execute the function body (if available) */` |
|        - | 11723 | `	{` |
|        - | 11724 | `		sxi32 rcExec;` |
|      598 | 11725 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11726 | `		/* Clean up the mess left behind */` |
|      598 | 11727 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11728 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 11729 | `		return rcExec;` |
|        - | 11730 | `	}` |
|      608 | 11731 |  |
|        - | 11732 | `/*` |
|        - | 11733 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11734 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11735 | ` * parameter.` |
|        - | 11736 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11737 | ` * return value indicates failure.` |
|        - | 11738 | ` */` |
|      240 | 11739 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11740 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11741 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11742 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11743 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11744 | `	)` |
|        1 | 11745 |  |
|        - | 11746 | `	ph7_value *pArg;` |
|        - | 11747 | `	SySet aArg;` |
|        - | 11748 | `	va_list ap;` |
|        - | 11749 | `	sxi32 rc;` |
|      241 | 11750 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11751 | `	/* Copy arguments one after one */` |
|      241 | 11752 | `	va_start(ap,pResult);` |
|      399 | 11753 | `	for(;;){` |
|      799 | 11754 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 11755 | `		if( pArg == 0 ){` |
|      241 | 11756 | `			break;` |
|        - | 11757 | `		}` |
|      559 | 11758 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11759 | `	}` |
|        - | 11760 | `	/* Call the core routine */` |
|      241 | 11761 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11762 | `	/* Cleanup */` |
|      241 | 11763 | `	SySetRelease(&aArg);` |
|      241 | 11764 | `	return rc;` |
|        1 | 11765 |  |
|        - | 11766 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11767 | `/*` |
|        - | 11768 | ` * bool defined(string $name)` |
|        - | 11769 | ` *  Checks whether a given named constant exists.` |
|        - | 11770 | ` * Parameter:` |
|        - | 11771 | ` *  Name of the desired constant.` |
|        - | 11772 | ` * Return` |
|        - | 11773 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11774 | ` */` |
|       20 | 11775 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11776 |  |
|        - | 11777 | `	const char *zName;` |
|       22 | 11778 | `	int nLen = 0;` |
|       22 | 11779 | `	int res = 0;` |
|       22 | 11780 | `	if( nArg < 1 ){` |
|        - | 11781 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11782 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11783 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11784 | `		return SXRET_OK;` |
|        - | 11785 | `	}` |
|        - | 11786 | `	/* Extract constant name */` |
|       22 | 11787 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11788 | `	/* Perform the lookup */` |
|       22 | 11789 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11790 | `		/* Already defined */` |
|       20 | 11791 | `		res = 1;` |
|        9 | 11792 | `	}` |
|       22 | 11793 | `	ph7_result_bool(pCtx,res);` |
|       22 | 11794 | `	return SXRET_OK;` |
|       12 | 11795 |  |
|        - | 11796 | `/*` |
|        - | 11797 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11798 | ` * below.` |
|        - | 11799 | ` */` |
|       10 | 11800 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11801 |  |
|       12 | 11802 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11803 | `	/* Expand constant value */` |
|       12 | 11804 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11805 |  |
|        - | 11806 | `/*` |
|        - | 11807 | ` * bool define(string $constant_name,expression value)` |
|        - | 11808 | ` *  Defines a named constant at runtime.` |
|        - | 11809 | ` * Parameter:` |
|        - | 11810 | ` *  $constant_name` |
|        - | 11811 | ` *   The name of the constant` |
|        - | 11812 | ` *  $value` |
|        - | 11813 | ` *   Constant value` |
|        - | 11814 | ` * Return:` |
|        - | 11815 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11816 | ` */` |
|       12 | 11817 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11818 |  |
|        - | 11819 | `	const char *zName;  /* Constant name */` |
|        - | 11820 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11821 | `	int nLen = 0;       /* Name length */` |
|        - | 11822 | `	sxi32 rc;` |
|       14 | 11823 | `	if( nArg < 2 ){` |
|        - | 11824 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11825 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11826 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11827 | `		return SXRET_OK;` |
|        - | 11828 | `	}` |
|       14 | 11829 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11830 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11831 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11832 | `		return SXRET_OK;` |
|        - | 11833 | `	}` |
|        - | 11834 | `	/* Extract constant name */` |
|       14 | 11835 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11836 | `	if( nLen < 1 ){` |
|      ! 0 | 11837 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11838 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11839 | `		return SXRET_OK;` |
|        - | 11840 | `	}` |
|        - | 11841 | `	/* Duplicate constant value */` |
|       14 | 11842 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11843 | `	if( pValue == 0 ){` |
|      ! 0 | 11844 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11845 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11846 | `		return SXRET_OK;` |
|        - | 11847 | `	}` |
|        - | 11848 | `	/* Initialize the memory object */` |
|       14 | 11849 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11850 | `	/* Register the constant */` |
|       14 | 11851 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11852 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11853 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11854 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11855 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11856 | `		return SXRET_OK;` |
|        - | 11857 | `	}` |
|        - | 11858 | `	/* Duplicate constant value */` |
|       14 | 11859 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11860 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11861 | `		/* Lower case the constant name */` |
|      ! 0 | 11862 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11863 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11864 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11865 | `				/* UTF-8 stream */` |
|      ! 0 | 11866 | `				zCur++;` |
|      ! 0 | 11867 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11868 | `					zCur++;` |
|      ! 0 | 11869 | `				}` |
|      ! 0 | 11870 | `				continue;` |
|        - | 11871 | `			}` |
|      ! 0 | 11872 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11873 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11874 | `				zCur[0] = (char)c;` |
|      ! 0 | 11875 | `			}` |
|      ! 0 | 11876 | `			zCur++;` |
|      ! 0 | 11877 | `		}` |
|        - | 11878 | `		/* Finally,register the constant */` |
|      ! 0 | 11879 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11880 | `	}` |
|        - | 11881 | `	/* All done,return TRUE */` |
|       14 | 11882 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11883 | `	return SXRET_OK;` |
|        8 | 11884 |  |
|        - | 11885 | `/*` |
|        - | 11886 | ` * value constant(string $name)` |
|        - | 11887 | ` *  Returns the value of a constant` |
|        - | 11888 | ` * Parameter` |
|        - | 11889 | ` *  $name` |
|        - | 11890 | ` *    Name of the constant.` |
|        - | 11891 | ` * Return` |
|        - | 11892 | ` *  Constant value or NULL if not defined.` |
|        - | 11893 | ` */` |
|        8 | 11894 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11895 |  |
|        - | 11896 | `	SyHashEntry *pEntry;` |
|        - | 11897 | `	ph7_constant *pCons;` |
|        - | 11898 | `	const char *zName; /* Constant name */` |
|        - | 11899 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11900 | `	int nLen;` |
|       10 | 11901 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11902 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11903 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11904 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11905 | `		return SXRET_OK;` |
|        - | 11906 | `	}` |
|        - | 11907 | `	/* Extract the constant name */` |
|       10 | 11908 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11909 | `	/* Perform the query */` |
|       10 | 11910 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11911 | `	if( pEntry == 0 ){` |
|        3 | 11912 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11913 | `		ph7_result_null(pCtx);` |
|        3 | 11914 | `		return SXRET_OK;` |
|        - | 11915 | `	}` |
|        8 | 11916 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11917 | `	/* Point to the structure that describe the constant */` |
|        8 | 11918 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11919 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11920 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11921 | `	/* Return that value */` |
|        8 | 11922 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11923 | `	/* Cleanup */` |
|        8 | 11924 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11925 | `	return SXRET_OK;` |
|        6 | 11926 |  |
|        - | 11927 | `/*` |
|        - | 11928 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11929 | ` * defined below.` |
|        - | 11930 | ` */` |
|      466 | 11931 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11932 |  |
|      467 | 11933 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11934 | `	ph7_value sName;` |
|        - | 11935 | `	sxi32 rc;` |
|        - | 11936 | `	/* Prepare the constant name for insertion */` |
|      467 | 11937 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 11938 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11939 | `	/* Perform the insertion */` |
|      467 | 11940 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 11941 | `	PH7_MemObjRelease(&sName);` |
|      467 | 11942 | `	return rc;` |
|        1 | 11943 |  |
|        - | 11944 | `/*` |
|        - | 11945 | ` * array get_defined_constants(void)` |
|        - | 11946 | ` *  Returns an associative array with the names of all defined` |
|        - | 11947 | ` *  constants.` |
|        - | 11948 | ` * Parameters` |
|        - | 11949 | ` *  NONE.` |
|        - | 11950 | ` * Returns` |
|        - | 11951 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11952 | ` */` |
|        2 | 11953 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11954 |  |
|        - | 11955 | `	ph7_value *pArray;` |
|        - | 11956 | `	/* Create the array first*/` |
|        3 | 11957 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11958 | `	if( pArray == 0 ){` |
|      ! 0 | 11959 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11960 | `		SXUNUSED(apArg);` |
|        - | 11961 | `		/* Return NULL */` |
|      ! 0 | 11962 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11963 | `		return SXRET_OK;` |
|        - | 11964 | `	}` |
|        - | 11965 | `	/* Fill the array with the defined constants */` |
|        3 | 11966 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11967 | `	/* Return the created array */` |
|        3 | 11968 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11969 | `	return SXRET_OK;` |
|        2 | 11970 |  |
|        - | 11971 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11972 | `/*` |
|        - | 11973 | ` * Section:` |
|        - | 11974 | ` *  Random numbers/string generators.` |
|        - | 11975 | ` * Status:` |
|        - | 11976 | ` *    Stable.` |
|        - | 11977 | ` */` |
|        - | 11978 | `/*` |
|        - | 11979 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11980 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 11981 | ` * implemented in src/sx/sxrand.c).` |
|        - | 11982 | ` */` |
|     2893 | 11983 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11984 |  |
|        - | 11985 | `	sxu32 iNum;` |
|     2895 | 11986 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2895 | 11987 | `	return iNum;` |
|        2 | 11988 |  |
|        - | 11989 | `/*` |
|        - | 11990 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11991 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11992 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 11993 | ` * implemented in src/sx/sxrand.c).` |
|        - | 11994 | ` */` |
|   236034 | 11995 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11996 |  |
|        - | 11997 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11998 | `	int i;` |
|        - | 11999 | `	/* Generate a binary string first */` |
|   236036 | 12000 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12001 | `	/* Turn the binary string into english based alphabet */` |
|  2596544 | 12002 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2360510 | 12003 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1180256 | 12004 | `	 }` |
|   236036 | 12005 |  |
|        - | 12006 | `/*` |
|        - | 12007 | ` * int rand()` |
|        - | 12008 | ` * int mt_rand()` |
|        - | 12009 | ` * int rand(int $min,int $max)` |
|        - | 12010 | ` * int mt_rand(int $min,int $max)` |
|        - | 12011 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12012 | ` * Parameter` |
|        - | 12013 | ` *  $min` |
|        - | 12014 | ` *    The lowest value to return (default: 0)` |
|        - | 12015 | ` *  $max` |
|        - | 12016 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12017 | ` * Return` |
|        - | 12018 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12019 | ` * Note:` |
|        - | 12020 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12021 | ` *  by te SQLite3 library.` |
|        - | 12022 | ` */` |
|       20 | 12023 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12024 |  |
|        - | 12025 | `	sxu32 iNum;` |
|        - | 12026 | `	/* Generate the random number */` |
|       21 | 12027 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12028 | `	if( nArg > 1 ){` |
|        - | 12029 | `		sxu32 iMin,iMax;` |
|        3 | 12030 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12031 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12032 | `		if( iMin < iMax ){` |
|        3 | 12033 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12034 | `			if( iDiv > 0 ){` |
|        3 | 12035 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12036 | `			}` |
|        1 | 12037 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12038 | `			iNum %= iMax;` |
|      ! 0 | 12039 | `		}` |
|        1 | 12040 | `	}` |
|        - | 12041 | `	/* Return the number */` |
|       21 | 12042 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12043 | `	return SXRET_OK;` |
|        1 | 12044 |  |
|        - | 12045 | `/*` |
|        - | 12046 | ` * int getrandmax(void)` |
|        - | 12047 | ` * int mt_getrandmax(void)` |
|        - | 12048 | ` * int rc4_getrandmax(void)` |
|        - | 12049 | ` *   Show largest possible random value` |
|        - | 12050 | ` * Return` |
|        - | 12051 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12052 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12053 | ` * Note:` |
|        - | 12054 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12055 | ` *  by te SQLite3 library.` |
|        - | 12056 | ` */` |
|        4 | 12057 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12058 |  |
|        2 | 12059 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12060 | `	SXUNUSED(apArg);` |
|        5 | 12061 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12062 | `	return SXRET_OK;` |
|        1 | 12063 |  |
|        - | 12064 | `/*` |
|        - | 12065 | ` * string rand_str()` |
|        - | 12066 | ` * string rand_str(int $len)` |
|        - | 12067 | ` *  Generate a random string (English alphabet).` |
|        - | 12068 | ` * Parameter` |
|        - | 12069 | ` *  $len` |
|        - | 12070 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12071 | ` * Return` |
|        - | 12072 | ` *   A pseudo random string.` |
|        - | 12073 | ` * Note:` |
|        - | 12074 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12075 | ` *  by te SQLite3 library.` |
|        - | 12076 | ` *  This function is a symisc extension.` |
|        - | 12077 | ` */` |
|      120 | 12078 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12079 |  |
|        - | 12080 | `	char zString[1024];` |
|      122 | 12081 | `	int iLen = 0x10;` |
|      122 | 12082 | `	if( nArg > 0 ){` |
|        - | 12083 | `		/* Get the desired length */` |
|      122 | 12084 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12085 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12086 | `			/* Default length */` |
|        3 | 12087 | `			iLen = 0x10;` |
|        1 | 12088 | `		}` |
|       60 | 12089 | `	}` |
|        - | 12090 | `	/* Generate the random string */` |
|      122 | 12091 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12092 | `	/* Return the generated string */` |
|      122 | 12093 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12094 | `	return SXRET_OK;` |
|        2 | 12095 |  |
|        - | 12096 | `/*` |
|        - | 12097 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12098 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12099 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12100 | ` */` |
|      488 | 12101 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12102 |  |
|      488 | 12103 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12104 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12105 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12106 | `			"TypeError",` |
|        - | 12107 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12108 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12109 | `			ph7_type_name(pArg)` |
|        - | 12110 | `			);` |
|        - | 12111 | `	}` |
|      483 | 12112 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12113 | `		int len;` |
|        9 | 12114 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12115 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12116 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12117 | `				"TypeError",` |
|        - | 12118 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12119 | `				zFunc,iArgPos,zParamName` |
|        - | 12120 | `				);` |
|        - | 12121 | `		}` |
|        2 | 12122 | `	}` |
|      479 | 12123 | `	return SXRET_OK;` |
|      245 | 12124 |  |
|        - | 12125 | `/*` |
|        - | 12126 | ` * int random_int(int $min, int $max)` |
|        - | 12127 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12128 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12129 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12130 | ` *  power-of-two mask covering the range.` |
|        - | 12131 | ` */` |
|      242 | 12132 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12133 |  |
|        - | 12134 | `	sxi64 iMin,iMax;` |
|        - | 12135 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12136 | `	unsigned int nAttempt;` |
|        - | 12137 | `	int rc;` |
|      243 | 12138 | `	if( nArg != 2 ){` |
|       10 | 12139 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12140 | `			"ArgumentCountError",` |
|        - | 12141 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12142 | `			nArg` |
|        - | 12143 | `			);` |
|        - | 12144 | `	}` |
|      237 | 12145 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12146 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12147 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12148 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12149 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12150 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12151 | `	if( iMin > iMax ){` |
|        3 | 12152 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12153 | `			"ValueError",` |
|        - | 12154 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12155 | `			);` |
|        - | 12156 | `	}` |
|      229 | 12157 | `	if( iMin == iMax ){` |
|        5 | 12158 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12159 | `		return SXRET_OK;` |
|        - | 12160 | `	}` |
|      225 | 12161 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12162 | `	uMask = uRange;` |
|      225 | 12163 | `	uMask \|= uMask >> 1;` |
|      225 | 12164 | `	uMask \|= uMask >> 2;` |
|      225 | 12165 | `	uMask \|= uMask >> 4;` |
|      225 | 12166 | `	uMask \|= uMask >> 8;` |
|      225 | 12167 | `	uMask \|= uMask >> 16;` |
|      225 | 12168 | `	uMask \|= uMask >> 32;` |
|      225 | 12169 | `	uResult = 0;` |
|      334 | 12170 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12171 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12172 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12173 | `		 * and the low-half mask would always read 0). */` |
|        - | 12174 | `		sxu64 uDraw;` |
|      334 | 12175 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12176 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12177 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12178 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12179 | `				"Exception",` |
|        - | 12180 | `				"Cannot gather sufficient random data"` |
|        - | 12181 | `				);` |
|        - | 12182 | `		}` |
|      334 | 12183 | `		uDraw &= uMask;` |
|      334 | 12184 | `		if( uDraw <= uRange ){` |
|      225 | 12185 | `			uResult = uDraw;` |
|      225 | 12186 | `			break;` |
|        - | 12187 | `		}` |
|       62 | 12188 | `	}` |
|      225 | 12189 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12190 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12191 | `			"Exception",` |
|        - | 12192 | `			"Cannot gather sufficient random data"` |
|        - | 12193 | `			);` |
|        - | 12194 | `	}` |
|      225 | 12195 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12196 | `	return SXRET_OK;` |
|      122 | 12197 |  |
|        - | 12198 | `/*` |
|        - | 12199 | ` * string random_bytes(int $length)` |
|        - | 12200 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12201 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12202 | ` */` |
|       24 | 12203 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12204 |  |
|        - | 12205 | `	sxi64 iLen;` |
|        - | 12206 | `	unsigned char zStack[256];` |
|        - | 12207 | `	void *pBuf;` |
|        - | 12208 | `	int rc;` |
|       25 | 12209 | `	int bHeap = 0;` |
|       25 | 12210 | `	if( nArg != 1 ){` |
|        7 | 12211 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12212 | `			"ArgumentCountError",` |
|        - | 12213 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12214 | `			nArg` |
|        - | 12215 | `			);` |
|        - | 12216 | `	}` |
|       21 | 12217 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12218 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12219 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12220 | `	if( iLen < 1 ){` |
|        5 | 12221 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12222 | `			"ValueError",` |
|        - | 12223 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12224 | `			);` |
|        - | 12225 | `	}` |
|        - | 12226 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12227 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12228 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12229 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12230 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12231 | `			"ValueError",` |
|        - | 12232 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12233 | `			);` |
|        - | 12234 | `	}` |
|       13 | 12235 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12236 | `		pBuf = zStack;` |
|        7 | 12237 | `	}else{` |
|      ! 0 | 12238 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12239 | `		if( pBuf == 0 ){` |
|      ! 0 | 12240 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12241 | `				"Exception",` |
|        - | 12242 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12243 | `				iLen` |
|        - | 12244 | `				);` |
|        - | 12245 | `		}` |
|      ! 0 | 12246 | `		bHeap = 1;` |
|        - | 12247 | `	}` |
|       13 | 12248 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12249 | `		if( bHeap ){` |
|      ! 0 | 12250 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12251 | `		}` |
|      ! 0 | 12252 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12253 | `			"Exception",` |
|        - | 12254 | `			"Cannot gather sufficient random data"` |
|        - | 12255 | `			);` |
|        - | 12256 | `	}` |
|       13 | 12257 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12258 | `	if( bHeap ){` |
|      ! 0 | 12259 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12260 | `	}` |
|       13 | 12261 | `	return SXRET_OK;` |
|       13 | 12262 |  |
|        - | 12263 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12264 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12265 | `/* Unique ID private data */` |
|        - | 12266 | `struct unique_id_data` |
|        - | 12267 |  |
|        - | 12268 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12269 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12270 | `};` |
|        - | 12271 | `/*` |
|        - | 12272 | ` * Binary to hex consumer callback.` |
|        - | 12273 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12274 | ` * defined below.` |
|        - | 12275 | ` */` |
|      192 | 12276 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12277 |  |
|      193 | 12278 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12279 | `	sxu32 nBuflen;` |
|        - | 12280 | `	/* Extract result buffer length */` |
|      193 | 12281 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12282 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12283 | `			/*` |
|        - | 12284 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12285 | `			 * string will be 13 characters long` |
|        - | 12286 | `			 */` |
|       25 | 12287 | `		return SXERR_ABORT;` |
|        - | 12288 | `	}` |
|      169 | 12289 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12290 | `		return SXERR_ABORT;` |
|        - | 12291 | `	}` |
|        - | 12292 | `	/* Safely Consume the hex stream */` |
|      169 | 12293 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12294 | `	return SXRET_OK;` |
|       97 | 12295 |  |
|        - | 12296 | `/*` |
|        - | 12297 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12298 | ` *  Generate a unique ID` |
|        - | 12299 | ` * Parameter` |
|        - | 12300 | ` * $prefix` |
|        - | 12301 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12302 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12303 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12304 | ` * $more_entropy` |
|        - | 12305 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12306 | ` *  that the result will be unique.` |
|        - | 12307 | ` * Return` |
|        - | 12308 | ` *  Returns the unique identifier, as a string.` |
|        - | 12309 | ` */` |
|       24 | 12310 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12311 |  |
|        - | 12312 | `	struct unique_id_data sUniq;` |
|        - | 12313 | `	unsigned char zDigest[20];` |
|       25 | 12314 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12315 | `	const char *zPrefix;` |
|        - | 12316 | `	SHA1Context sCtx;` |
|        - | 12317 | `	char zRandom[7];` |
|        - | 12318 | `	int nPrefix;` |
|        - | 12319 | `	int entropy;` |
|        - | 12320 | `	/* Generate a random string first */` |
|       25 | 12321 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12322 | `	/* Initialize fields */` |
|       25 | 12323 | `	zPrefix = 0;` |
|       25 | 12324 | `	nPrefix = 0;` |
|       25 | 12325 | `	entropy = 0;` |
|       25 | 12326 | `	if( nArg > 0 ){` |
|        - | 12327 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12328 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12329 | `		if( nArg > 1 ){` |
|      ! 0 | 12330 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12331 | `		}` |
|      ! 0 | 12332 | `	}` |
|       25 | 12333 | `	SHA1Init(&sCtx);` |
|        - | 12334 | `	/* Generate the random ID */` |
|       25 | 12335 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12336 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12337 | `	}` |
|        - | 12338 | `	/* Append the random ID */` |
|       25 | 12339 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12340 | `	/* Append the random string */` |
|       25 | 12341 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12342 | `	/* Increment the number */` |
|       25 | 12343 | `	pVm->unique_id++;` |
|       25 | 12344 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12345 | `	/* Hexify the digest */` |
|       25 | 12346 | `	sUniq.pCtx = pCtx;` |
|       25 | 12347 | `	sUniq.entropy = entropy;` |
|       25 | 12348 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12349 | `	/* All done */` |
|       25 | 12350 | `	return PH7_OK;` |
|        1 | 12351 |  |
|        - | 12352 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12353 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12354 | `/*` |
|        - | 12355 | ` * Section:` |
|        - | 12356 | ` *  Language construct implementation as foreign functions.` |
|        - | 12357 | ` * Status:` |
|        - | 12358 | ` *    Stable.` |
|        - | 12359 | ` */` |
|        - | 12360 | `/*` |
|        - | 12361 | ` * void echo($string...)` |
|        - | 12362 | ` *  Output one or more messages.` |
|        - | 12363 | ` * Parameters` |
|        - | 12364 | ` *  $string` |
|        - | 12365 | ` *   Message to output.` |
|        - | 12366 | ` * Return` |
|        - | 12367 | ` *  NULL.` |
|        - | 12368 | ` */` |
|      ! 0 | 12369 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12370 |  |
|        - | 12371 | `	const char *zData;` |
|      ! 0 | 12372 | `	int nDataLen = 0;` |
|        - | 12373 | `	ph7_vm *pVm;` |
|        - | 12374 | `	int i,rc;` |
|        - | 12375 | `	/* Point to the target VM */` |
|      ! 0 | 12376 | `	pVm = pCtx->pVm;` |
|        - | 12377 | `	/* Output */` |
|      ! 0 | 12378 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12379 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12380 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12381 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12382 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12383 | `			if( rc == SXERR_ABORT ){` |
|        - | 12384 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12385 | `				return PH7_ABORT;` |
|        - | 12386 | `			}` |
|      ! 0 | 12387 | `		}` |
|      ! 0 | 12388 | `	}` |
|      ! 0 | 12389 | `	return SXRET_OK;` |
|      ! 0 | 12390 |  |
|        - | 12391 | `/*` |
|        - | 12392 | ` * int print($string...)` |
|        - | 12393 | ` *  Output one or more messages.` |
|        - | 12394 | ` * Parameters` |
|        - | 12395 | ` *  $string` |
|        - | 12396 | ` *   Message to output.` |
|        - | 12397 | ` * Return` |
|        - | 12398 | ` *  1 always.` |
|        - | 12399 | ` */` |
|        2 | 12400 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12401 |  |
|        - | 12402 | `	const char *zData;` |
|        3 | 12403 | `	int nDataLen = 0;` |
|        - | 12404 | `	ph7_vm *pVm;` |
|        - | 12405 | `	int i,rc;` |
|        - | 12406 | `	/* Point to the target VM */` |
|        3 | 12407 | `	pVm = pCtx->pVm;` |
|        - | 12408 | `	/* Output */` |
|        5 | 12409 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12410 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12411 | `		if( nDataLen > 0 ){` |
|        3 | 12412 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12413 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12414 | `			if( rc == SXERR_ABORT ){` |
|        - | 12415 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12416 | `				return PH7_ABORT;` |
|        - | 12417 | `			}` |
|        1 | 12418 | `		}` |
|        2 | 12419 | `	}` |
|        - | 12420 | `	/* Return 1 */` |
|        3 | 12421 | `	ph7_result_int(pCtx,1);` |
|        3 | 12422 | `	return SXRET_OK;` |
|        2 | 12423 |  |
|        - | 12424 | `/*` |
|        - | 12425 | ` * void exit(string $msg)` |
|        - | 12426 | ` * void exit(int $status)` |
|        - | 12427 | ` * void die(string $ms)` |
|        - | 12428 | ` * void die(int $status)` |
|        - | 12429 | ` *   Output a message and terminate program execution.` |
|        - | 12430 | ` * Parameter` |
|        - | 12431 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12432 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12433 | ` *  and not printed` |
|        - | 12434 | ` * Return` |
|        - | 12435 | ` *  NULL` |
|        - | 12436 | ` */` |
|      ! 0 | 12437 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12438 |  |
|      ! 0 | 12439 | `	if( nArg > 0 ){` |
|      ! 0 | 12440 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12441 | `			const char *zData;` |
|      ! 0 | 12442 | `			int iLen = 0;` |
|        - | 12443 | `			/* Print exit message */` |
|      ! 0 | 12444 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12445 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12446 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12447 | `			sxi32 iExitStatus;` |
|        - | 12448 | `			/* Record exit status code */` |
|      ! 0 | 12449 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12450 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12451 | `		}` |
|      ! 0 | 12452 | `	}` |
|        - | 12453 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12454 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12455 | `	 */` |
|      ! 0 | 12456 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12457 | `	return PH7_ABORT;` |
|      ! 0 | 12458 |  |
|        - | 12459 | `/*` |
|        - | 12460 | ` * bool isset($var,...)` |
|        - | 12461 | ` *  Finds out whether a variable is set.` |
|        - | 12462 | ` * Parameters` |
|        - | 12463 | ` *  One or more variable to check.` |
|        - | 12464 | ` * Return` |
|        - | 12465 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12466 | ` */` |
|    92500 | 12467 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12468 |  |
|        - | 12469 | `	ph7_value *pObj;` |
|    92502 | 12470 | `	int res = 0;` |
|        - | 12471 | `	int i;` |
|    92502 | 12472 | `	if( nArg < 1 ){` |
|        - | 12473 | `		/* Missing arguments,return false */` |
|      ! 0 | 12474 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12475 | `		return SXRET_OK;` |
|        - | 12476 | `	}` |
|        - | 12477 | `	/* Iterate over available arguments */` |
|   120922 | 12478 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92512 | 12479 | `		pObj = apArg[i];` |
|    92512 | 12480 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12481 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12482 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12483 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63160 | 12484 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12485 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12486 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12487 | `			}` |
|    31579 | 12488 | `		}` |
|    92512 | 12489 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92512 | 12490 | `		if( !res ){` |
|        - | 12491 | `			/* Variable not set,return FALSE */` |
|    64092 | 12492 | `			ph7_result_bool(pCtx,0);` |
|    64092 | 12493 | `			return SXRET_OK;` |
|        - | 12494 | `		}` |
|    14212 | 12495 | `	}` |
|        - | 12496 | `	/* All given variable are set,return TRUE */` |
|    28412 | 12497 | `	ph7_result_bool(pCtx,1);` |
|    28412 | 12498 | `	return SXRET_OK;` |
|    46252 | 12499 |  |
|        - | 12500 | `/*` |
|        - | 12501 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12502 | ` * frame,the reference table and discard it's contents.` |
|        - | 12503 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12504 | ` */` |
|  3159712 | 12505 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12506 |  |
|        - | 12507 | `	ph7_value *pObj;` |
|        - | 12508 | `	VmRefObj *pRef;` |
|  3159714 | 12509 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3159714 | 12510 | `	if( pObj ){` |
|        - | 12511 | `		/* Release the object */` |
|  3159714 | 12512 | `		PH7_MemObjRelease(pObj);` |
|  1579856 | 12513 | `	}` |
|        - | 12514 | `	/* Remove old reference links */` |
|  3159714 | 12515 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3159714 | 12516 | `	if( pRef ){` |
|  3159708 | 12517 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12518 | `		/* Unlink from the reference table */` |
|  3159708 | 12519 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3159708 | 12520 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12521 | `			VmSlot sFree;` |
|        - | 12522 | `			/* Restore to the free list */` |
|  3159700 | 12523 | `			sFree.nIdx = nObjIdx;` |
|  3159700 | 12524 | `			sFree.pUserData = 0;` |
|  3159700 | 12525 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1579849 | 12526 | `		}` |
|  1579853 | 12527 | `	}` |
|  3159714 | 12528 | `	return SXRET_OK;` |
|        2 | 12529 |  |
|        - | 12530 | `/*` |
|        - | 12531 | ` * void unset($var,...)` |
|        - | 12532 | ` *   Unset one or more given variable.` |
|        - | 12533 | ` * Parameters` |
|        - | 12534 | ` *  One or more variable to unset.` |
|        - | 12535 | ` * Return` |
|        - | 12536 | ` *  Nothing.` |
|        - | 12537 | ` */` |
|     7500 | 12538 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12539 |  |
|        - | 12540 | `	ph7_value *pObj;` |
|        - | 12541 | `	ph7_vm *pVm;` |
|        - | 12542 | `	int i;` |
|        - | 12543 | `	/* Point to the target VM */` |
|     7502 | 12544 | `	pVm = pCtx->pVm;` |
|        - | 12545 | `	/* Iterate and unset */` |
|    15002 | 12546 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7502 | 12547 | `		pObj = apArg[i];` |
|     7502 | 12548 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      818 | 12549 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12550 | `				/* Throw an error */` |
|      ! 0 | 12551 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12552 | `			}` |
|      410 | 12553 | `		}else{` |
|     6686 | 12554 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12555 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6686 | 12556 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6680 | 12557 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3339 | 12558 | `			}` |
|        - | 12559 | `		}` |
|     3752 | 12560 | `	}` |
|     7502 | 12561 | `	return SXRET_OK;` |
|        2 | 12562 |  |
|        - | 12563 | `/*` |
|        - | 12564 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12565 | ` */` |
|      116 | 12566 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12567 |  |
|      117 | 12568 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12569 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12570 | `	ph7_value *pObj;` |
|        - | 12571 | `	sxu32 nIdx;` |
|        - | 12572 | `	/* Extract the memory object */` |
|      117 | 12573 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12574 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12575 | `	if( pObj ){` |
|      117 | 12576 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12577 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12578 | `				SyString sName;` |
|        - | 12579 | `				ph7_value sKey;` |
|        - | 12580 | `				/* Perform the insertion */` |
|      115 | 12581 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12582 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12583 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12584 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12585 | `			}` |
|       57 | 12586 | `		}` |
|       58 | 12587 | `	}` |
|      117 | 12588 | `	return SXRET_OK;` |
|        1 | 12589 |  |
|        - | 12590 | `/*` |
|        - | 12591 | ` * array get_defined_vars(void)` |
|        - | 12592 | ` *  Returns an array of all defined variables.` |
|        - | 12593 | ` * Parameter` |
|        - | 12594 | ` *  None` |
|        - | 12595 | ` * Return` |
|        - | 12596 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12597 | ` */` |
|        2 | 12598 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12599 |  |
|        3 | 12600 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12601 | `	ph7_value *pArray;` |
|        - | 12602 | `	/* Create a new array */` |
|        3 | 12603 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12604 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12605 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12606 | `		SXUNUSED(apArg);` |
|        - | 12607 | `		/* Return NULL */` |
|      ! 0 | 12608 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12609 | `		return SXRET_OK;` |
|        - | 12610 | `	}` |
|        - | 12611 | `	/* Superglobals first */` |
|        3 | 12612 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12613 | `	/* Then variable defined in the current frame */` |
|        3 | 12614 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12615 | `	/* Finally,return the created array */` |
|        3 | 12616 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12617 | `	return SXRET_OK;` |
|        2 | 12618 |  |
|        - | 12619 | `/*` |
|        - | 12620 | ` * bool gettype($var)` |
|        - | 12621 | ` *  Get the type of a variable` |
|        - | 12622 | ` * Parameters` |
|        - | 12623 | ` *   $var` |
|        - | 12624 | ` *    The variable being type checked.` |
|        - | 12625 | ` * Return` |
|        - | 12626 | ` *   String representation of the given variable type.` |
|        - | 12627 | ` */` |
|       32 | 12628 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12629 |  |
|       34 | 12630 | `	const char *zType = "Empty";` |
|       34 | 12631 | `	if( nArg > 0 ){` |
|       34 | 12632 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12633 | `	}` |
|        - | 12634 | `	/* Return the variable type */` |
|       34 | 12635 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12636 | `	return SXRET_OK;` |
|        2 | 12637 |  |
|        - | 12638 | `/*` |
|        - | 12639 | ` * string get_resource_type(resource $handle)` |
|        - | 12640 | ` *  This function gets the type of the given resource.` |
|        - | 12641 | ` * Parameters` |
|        - | 12642 | ` *  $handle` |
|        - | 12643 | ` *  The evaluated resource handle.` |
|        - | 12644 | ` * Return` |
|        - | 12645 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12646 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12647 | ` *  the return value will be the string Unknown.` |
|        - | 12648 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12649 | ` *  is not a resource.` |
|        - | 12650 | ` */` |
|        2 | 12651 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12652 |  |
|        3 | 12653 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12654 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12655 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12656 | `		return PH7_OK;` |
|        - | 12657 | `	}` |
|        3 | 12658 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12659 | `	return SXRET_OK;` |
|        2 | 12660 |  |
|        - | 12661 | `/*` |
|        - | 12662 | ` * void var_dump(expression,....)` |
|        - | 12663 | ` *   var_dump � Dumps information about a variable` |
|        - | 12664 | ` * Parameters` |
|        - | 12665 | ` *   One or more expression to dump.` |
|        - | 12666 | ` * Returns` |
|        - | 12667 | ` *  Nothing.` |
|        - | 12668 | ` */` |
|      218 | 12669 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12670 |  |
|        - | 12671 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12672 | `	int i;` |
|      220 | 12673 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12674 | `	/* Dump one or more expressions */` |
|      444 | 12675 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12676 | `		ph7_value *pObj = apArg[i];` |
|        - | 12677 | `		/* Reset the working buffer */` |
|      226 | 12678 | `		SyBlobReset(&sDump);` |
|        - | 12679 | `		/* Dump the given expression */` |
|      226 | 12680 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12681 | `		/* Output */` |
|      226 | 12682 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12683 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12684 | `		}` |
|      114 | 12685 | `	}` |
|        - | 12686 | `	/* Release the working buffer */` |
|      220 | 12687 | `	SyBlobRelease(&sDump);` |
|      220 | 12688 | `	return SXRET_OK;` |
|        2 | 12689 |  |
|        - | 12690 | `/*` |
|        - | 12691 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12692 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12693 | ` * Parameters` |
|        - | 12694 | ` *   expression: Expression to dump` |
|        - | 12695 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12696 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12697 | ` *            print_r() will return the information rather than print it.` |
|        - | 12698 | ` * Return` |
|        - | 12699 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12700 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12701 | ` */` |
|       16 | 12702 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12703 |  |
|       17 | 12704 | `	int ret_string = 0;` |
|        - | 12705 | `	SyBlob sDump;` |
|       17 | 12706 | `	if( nArg < 1 ){` |
|        - | 12707 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12708 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12709 | `		return SXRET_OK;` |
|        - | 12710 | `	}` |
|       17 | 12711 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12712 | `	if ( nArg > 1 ){` |
|        - | 12713 | `		/* Where to redirect output */` |
|       11 | 12714 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12715 | `	}` |
|        - | 12716 | `	/* Generate dump */` |
|       17 | 12717 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12718 | `	if( !ret_string ){` |
|        - | 12719 | `		/* Output dump */` |
|        7 | 12720 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12721 | `		/* Return true */` |
|        7 | 12722 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12723 | `	}else{` |
|        - | 12724 | `		/* Generated dump as return value */` |
|       11 | 12725 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12726 | `	}` |
|        - | 12727 | `	/* Release the working buffer */` |
|       17 | 12728 | `	SyBlobRelease(&sDump);` |
|       17 | 12729 | `	return SXRET_OK;` |
|        9 | 12730 |  |
|        - | 12731 | `/*` |
|        - | 12732 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12733 | ` * Same job as print_r. (see coment above)` |
|        - | 12734 | ` */` |
|        2 | 12735 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12736 |  |
|        3 | 12737 | `	int ret_string = 0;` |
|        - | 12738 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12739 | `	if( nArg < 1 ){` |
|        - | 12740 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12741 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12742 | `		return SXRET_OK;` |
|        - | 12743 | `	}` |
|        3 | 12744 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12745 | `	if ( nArg > 1 ){` |
|        - | 12746 | `		/* Where to redirect output */` |
|        3 | 12747 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12748 | `	}` |
|        - | 12749 | `	/* Generate dump */` |
|        3 | 12750 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12751 | `	if( !ret_string ){` |
|        - | 12752 | `		/* Output dump */` |
|      ! 0 | 12753 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12754 | `		/* Return NULL */` |
|      ! 0 | 12755 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12756 | `	}else{` |
|        - | 12757 | `		/* Generated dump as return value */` |
|        3 | 12758 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12759 | `	}` |
|        - | 12760 | `	/* Release the working buffer */` |
|        3 | 12761 | `	SyBlobRelease(&sDump);` |
|        3 | 12762 | `	return SXRET_OK;` |
|        2 | 12763 |  |
|        - | 12764 | `/*` |
|        - | 12765 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12766 | ` *  Set/get the various assert flags.` |
|        - | 12767 | ` * Parameter` |
|        - | 12768 | ` * $what` |
|        - | 12769 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12770 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12771 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12772 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12773 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12774 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12775 | ` * $value` |
|        - | 12776 | ` *   An optional new value for the option.` |
|        - | 12777 | ` * Return` |
|        - | 12778 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12779 | ` */` |
|       28 | 12780 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12781 |  |
|       30 | 12782 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12783 | `	int iOption;` |
|        - | 12784 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12785 | `	if( nArg < 1 ){` |
|        3 | 12786 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12787 | `			"ArgumentCountError",` |
|        - | 12788 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12789 | `			);` |
|        - | 12790 | `	}` |
|        - | 12791 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12792 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12793 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12794 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12795 | `			"TypeError",` |
|        - | 12796 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12797 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12798 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12799 | `			);` |
|        - | 12800 | `	}` |
|       28 | 12801 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12802 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12803 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12804 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12805 | `	switch( iOption ){` |
|        5 | 12806 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12807 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12808 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12809 | `		if( nArg > 1 ){` |
|        5 | 12810 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12811 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12812 | `			}else{` |
|        3 | 12813 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12814 | `			}` |
|        2 | 12815 | `		}` |
|       12 | 12816 | `		break;` |
|        1 | 12817 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12818 | `		/* Return old callback or null */` |
|        3 | 12819 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12820 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12821 | `		}else{` |
|        3 | 12822 | `			ph7_result_null(pCtx);` |
|        - | 12823 | `		}` |
|        3 | 12824 | `		if( nArg > 1 ){` |
|      ! 0 | 12825 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12826 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12827 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12828 | `			}else{` |
|      ! 0 | 12829 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12830 | `			}` |
|      ! 0 | 12831 | `		}` |
|        3 | 12832 | `		break;` |
|        5 | 12833 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12834 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12835 | `		if( nArg > 1 ){` |
|        5 | 12836 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12837 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12838 | `			}else{` |
|        3 | 12839 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12840 | `			}` |
|        2 | 12841 | `		}` |
|       11 | 12842 | `		break;` |
|      ! 0 | 12843 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12844 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12845 | `		break;` |
|        1 | 12846 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12847 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12848 | `		break;` |
|      ! 0 | 12849 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12850 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12851 | `		break;` |
|        1 | 12852 | `	default:` |
|        - | 12853 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12854 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12855 | `			"ValueError",` |
|        - | 12856 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12857 | `			);` |
|        - | 12858 | `	}` |
|       26 | 12859 | `	return PH7_OK;` |
|       16 | 12860 |  |
|        - | 12861 | `/*` |
|        - | 12862 | ` * bool assert(mixed $assertion)` |
|        - | 12863 | ` *  Checks if assertion is FALSE.` |
|        - | 12864 | ` * Parameter` |
|        - | 12865 | ` *  $assertion` |
|        - | 12866 | ` *    The assertion to test.` |
|        - | 12867 | ` * Return` |
|        - | 12868 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12869 | ` */` |
|       24 | 12870 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12871 |  |
|       26 | 12872 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12873 | `	int iFlags,iResult;` |
|        - | 12874 | `	const char *zDesc;` |
|        - | 12875 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12876 | `	if( nArg < 1 ){` |
|        3 | 12877 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12878 | `			"ArgumentCountError",` |
|        - | 12879 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12880 | `			);` |
|        - | 12881 | `	}` |
|       24 | 12882 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12883 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12884 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12885 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12886 | `		return PH7_OK;` |
|        - | 12887 | `	}` |
|        - | 12888 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12889 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12890 | `	if( !iResult ){` |
|        - | 12891 | `		/* Assertion failed */` |
|        - | 12892 | `		/* Extract optional description */` |
|       13 | 12893 | `		zDesc = 0;` |
|       13 | 12894 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12895 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12896 | `		}` |
|       13 | 12897 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12898 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12899 | `			ph7_value sFile,sLine;` |
|        - | 12900 | `			ph7_value *apCbArg[3];` |
|        - | 12901 | `			SyString *pFile;` |
|        - | 12902 | `			/* Extract the processed script */` |
|      ! 0 | 12903 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12904 | `			if( pFile == 0 ){` |
|      ! 0 | 12905 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12906 | `			}` |
|        - | 12907 | `			/* Invoke the callback */` |
|      ! 0 | 12908 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12909 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12910 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12911 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12912 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12913 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12914 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12915 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12916 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12917 | `		}` |
|       13 | 12918 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12919 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12920 | `			return PH7_ABORT;` |
|        - | 12921 | `		}` |
|        - | 12922 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12923 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12924 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12925 | `				"AssertionError",` |
|        - | 12926 | `				"%s",` |
|        1 | 12927 | `				zDesc` |
|        - | 12928 | `				);` |
|      ! 0 | 12929 | `		}else{` |
|       11 | 12930 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12931 | `				"AssertionError",` |
|        - | 12932 | `				"assert(false)"` |
|        - | 12933 | `				);` |
|        - | 12934 | `		}` |
|        - | 12935 | `	}` |
|        - | 12936 | `	/* Assertion passed */` |
|       11 | 12937 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12938 | `	return PH7_OK;` |
|       14 | 12939 |  |
|        - | 12940 | `/*` |
|        - | 12941 | ` * Section:` |
|        - | 12942 | ` *  Error reporting functions.` |
|        - | 12943 | ` * Status:` |
|        - | 12944 | ` *    Stable.` |
|        - | 12945 | ` */` |
|        - | 12946 | `/*` |
|        - | 12947 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12948 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12949 | ` * Parameters` |
|        - | 12950 | ` *  $error_msg` |
|        - | 12951 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12952 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12953 | ` * $error_type` |
|        - | 12954 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12955 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12956 | ` * Return` |
|        - | 12957 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12958 | ` */` |
|       12 | 12959 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12960 |  |
|       14 | 12961 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12962 | `	int rc = PH7_OK;` |
|       14 | 12963 | `	if( nArg > 0 ){` |
|        - | 12964 | `		const char *zErr;` |
|        - | 12965 | `		int nLen;` |
|        - | 12966 | `		/* Extract the error message */` |
|       12 | 12967 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12968 | `		if( nArg > 1 ){` |
|        - | 12969 | `			/* Extract the error type */` |
|       12 | 12970 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12971 | `			switch( nErr ){` |
|        1 | 12972 | `			case 1:   /* E_ERROR */` |
|        - | 12973 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12974 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12975 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12976 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12977 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12978 | `				break;` |
|        1 | 12979 | `			case 2:   /* E_WARNING */` |
|        - | 12980 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12981 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12982 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12983 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12984 | `				break;` |
|        3 | 12985 | `			default:` |
|        8 | 12986 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12987 | `				break;` |
|        - | 12988 | `			}` |
|        5 | 12989 | `		}` |
|        - | 12990 | `		/* Report error */` |
|       12 | 12991 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12992 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12993 | `			return rc;` |
|        - | 12994 | `		}` |
|        - | 12995 | `		/* Return true */` |
|       12 | 12996 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12997 | `	}else{` |
|        - | 12998 | `		/* Missing arguments,return FALSE */` |
|        3 | 12999 | `		ph7_result_bool(pCtx,0);` |
|        - | 13000 | `	}` |
|       14 | 13001 | `	return rc;` |
|        8 | 13002 |  |
|        - | 13003 | `/*` |
|        - | 13004 | ` * int error_reporting([int $level])` |
|        - | 13005 | ` *  Sets which PHP errors are reported.` |
|        - | 13006 | ` * Parameters` |
|        - | 13007 | ` *  $level` |
|        - | 13008 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13009 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13010 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13011 | ` *   levels will not always behave as expected.` |
|        - | 13012 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13013 | ` *   in the predefined constants.` |
|        - | 13014 | ` * Return` |
|        - | 13015 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13016 | ` *   parameter is given.` |
|        - | 13017 | ` */` |
|       32 | 13018 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13019 |  |
|       34 | 13020 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13021 | `	int nOld;` |
|        - | 13022 | `	/* Extract the old reporting level */` |
|       34 | 13023 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13024 | `	if( nArg > 0 ){` |
|        - | 13025 | `		int nNew;` |
|        - | 13026 | `		/* Extract the desired error reporting level */` |
|       28 | 13027 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13028 | `		if( !nNew ){` |
|        - | 13029 | `			/* Do not report errors at all */` |
|        5 | 13030 | `			pVm->bErrReport = 0;` |
|        3 | 13031 | `		}else{` |
|        - | 13032 | `			/* Report all errors */` |
|       24 | 13033 | `			pVm->bErrReport = 1;` |
|        - | 13034 | `		}` |
|       13 | 13035 | `	}` |
|        - | 13036 | `	/* Return the old level */` |
|       34 | 13037 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13038 | `	return PH7_OK;` |
|        2 | 13039 |  |
|        - | 13040 | `/*` |
|        - | 13041 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13042 | ` *  Send an error message somewhere.` |
|        - | 13043 | ` * Parameter` |
|        - | 13044 | ` *  $message` |
|        - | 13045 | ` *   The error message that should be logged.` |
|        - | 13046 | ` *  $message_type` |
|        - | 13047 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13048 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13049 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13050 | ` *       This is the default option.` |
|        - | 13051 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13052 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13053 | ` *    2  No longer an option.` |
|        - | 13054 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13055 | ` *       to the end of the message string.` |
|        - | 13056 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13057 | ` *  $destination` |
|        - | 13058 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13059 | ` *  $extra_headers` |
|        - | 13060 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13061 | ` * Return` |
|        - | 13062 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13063 | ` * NOTE:` |
|        - | 13064 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13065 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13066 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13067 | ` *  Otherwise this function is no-op.` |
|        - | 13068 | ` */` |
|        4 | 13069 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13070 |  |
|        - | 13071 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13072 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13073 | `	int iType = 0;` |
|        5 | 13074 | `	if( nArg < 1 ){` |
|        - | 13075 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13076 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13077 | `		return PH7_OK;` |
|        - | 13078 | `	}` |
|        5 | 13079 | `	if( pVm->xErrLog  ){` |
|        - | 13080 | `		/* Invoke the user callback */` |
|      ! 0 | 13081 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13082 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13083 | `		if( nArg > 1 ){` |
|      ! 0 | 13084 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13085 | `			if( nArg > 2 ){` |
|      ! 0 | 13086 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13087 | `				if( nArg > 3 ){` |
|      ! 0 | 13088 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13089 | `				}` |
|      ! 0 | 13090 | `			}` |
|      ! 0 | 13091 | `		}` |
|      ! 0 | 13092 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13093 | `	}` |
|        - | 13094 | `	/* Retun TRUE */` |
|        5 | 13095 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13096 | `	return PH7_OK;` |
|        3 | 13097 |  |
|        - | 13098 | `/*` |
|        - | 13099 | ` * bool restore_exception_handler(void)` |
|        - | 13100 | ` *  Restores the previously defined exception handler function.` |
|        - | 13101 | ` * Parameter` |
|        - | 13102 | ` *  None` |
|        - | 13103 | ` * Return` |
|        - | 13104 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13105 | ` */` |
|        4 | 13106 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13107 |  |
|        5 | 13108 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13109 | `	ph7_value *pOld,*pNew;` |
|        - | 13110 | `	/* Point to the old and the new handler */` |
|        5 | 13111 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13112 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13113 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13114 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13115 | `		SXUNUSED(apArg);` |
|        - | 13116 | `		/* No installed handler,return FALSE */` |
|        5 | 13117 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13118 | `		return PH7_OK;` |
|        - | 13119 | `	}` |
|        - | 13120 | `	/* Copy the old handler */` |
|      ! 0 | 13121 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13122 | `	PH7_MemObjRelease(pOld);` |
|        - | 13123 | `	/* Return TRUE */` |
|      ! 0 | 13124 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13125 | `	return PH7_OK;` |
|        3 | 13126 |  |
|        - | 13127 | `/*` |
|        - | 13128 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13129 | ` *  Sets a user-defined exception handler function.` |
|        - | 13130 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13131 | ` * NOTE` |
|        - | 13132 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13133 | ` *  the satndard PHP engine.` |
|        - | 13134 | ` * Parameters` |
|        - | 13135 | ` *  $exception_handler` |
|        - | 13136 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13137 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13138 | ` *   that was thrown.` |
|        - | 13139 | ` *  Note:` |
|        - | 13140 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13141 | ` * Return` |
|        - | 13142 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13143 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13144 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13145 | ` */` |
|        4 | 13146 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13147 |  |
|        6 | 13148 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13149 | `	ph7_value *pOld,*pNew;` |
|        - | 13150 | `	/* Point to the old and the new handler */` |
|        6 | 13151 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13152 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13153 | `	/* Return the old handler */` |
|        6 | 13154 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13155 | `	if( nArg > 0 ){` |
|        6 | 13156 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13157 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13158 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13159 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13160 | `		}else{` |
|        6 | 13161 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13162 | `			/* Install the new handler */` |
|        6 | 13163 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13164 | `		}` |
|        2 | 13165 | `	}` |
|        6 | 13166 | `	return PH7_OK;` |
|        2 | 13167 |  |
|        - | 13168 | `/*` |
|        - | 13169 | ` * bool restore_error_handler(void)` |
|        - | 13170 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13171 | ` * Parameters:` |
|        - | 13172 | ` *  None.` |
|        - | 13173 | ` * Return` |
|        - | 13174 | ` *  Always TRUE.` |
|        - | 13175 | ` */` |
|        6 | 13176 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13177 |  |
|        7 | 13178 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13179 | `	ph7_value *pOld,*pNew;` |
|        - | 13180 | `	/* Point to the old and the new handler */` |
|        7 | 13181 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13182 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13183 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13184 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13185 | `		SXUNUSED(apArg);` |
|        - | 13186 | `		/* No installed callback,return FALSE */` |
|        7 | 13187 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13188 | `		return PH7_OK;` |
|        - | 13189 | `	}` |
|        - | 13190 | `	/* Copy the old callback */` |
|      ! 0 | 13191 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13192 | `	PH7_MemObjRelease(pOld);` |
|        - | 13193 | `	/* Return TRUE */` |
|      ! 0 | 13194 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13195 | `	return PH7_OK;` |
|        4 | 13196 |  |
|        - | 13197 | `/*` |
|        - | 13198 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13199 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13200 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13201 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13202 | ` *  Sets a user-defined error handler function.` |
|        - | 13203 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13204 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13205 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13206 | ` *  conditions (using trigger_error()).` |
|        - | 13207 | ` * Parameters` |
|        - | 13208 | ` *  $error_handler` |
|        - | 13209 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13210 | ` *   describing the error.` |
|        - | 13211 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13212 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13213 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13214 | ` *   The function can be shown as:` |
|        - | 13215 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13216 | ` *     errno` |
|        - | 13217 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13218 | ` *   errstr` |
|        - | 13219 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13220 | ` *   errfile` |
|        - | 13221 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13222 | ` *     was raised in, as a string.` |
|        - | 13223 | ` *  Note:` |
|        - | 13224 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13225 | ` * Return` |
|        - | 13226 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13227 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13228 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13229 | ` */` |
|    10840 | 13230 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13231 |  |
|    10842 | 13232 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13233 | `	ph7_value *pOld,*pNew;` |
|        - | 13234 | `	/* Point to the old and the new handler */` |
|    10842 | 13235 | `	pOld = &pVm->aErrCB[0];` |
|    10842 | 13236 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13237 | `	/* Return the old handler */` |
|    10842 | 13238 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10842 | 13239 | `	if( nArg > 0 ){` |
|    10842 | 13240 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13241 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5415 | 13242 | `			PH7_MemObjRelease(pNew);` |
|     5415 | 13243 | `			ph7_result_bool(pCtx,1);` |
|     2708 | 13244 | `		}else{` |
|     5428 | 13245 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13246 | `			/* Install the new handler */` |
|     5428 | 13247 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13248 | `		}` |
|     5420 | 13249 | `	}` |
|    10842 | 13250 | `	return PH7_OK;` |
|        2 | 13251 |  |
|        - | 13252 | `/*` |
|        - | 13253 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13254 | ` *  Generates a backtrace.` |
|        - | 13255 | ` * Paramaeter` |
|        - | 13256 | ` *  $options` |
|        - | 13257 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13258 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13259 | ` *   all the function/method arguments, to save memory.` |
|        - | 13260 | ` * $limit` |
|        - | 13261 | ` *   (Not Used)` |
|        - | 13262 | ` * Return` |
|        - | 13263 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13264 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13265 | ` *          Name        Type      Description` |
|        - | 13266 | ` *          ------      ------     -----------` |
|        - | 13267 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13268 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13269 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13270 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13271 | ` *          object      object    The current object.` |
|        - | 13272 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13273 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13274 | ` */` |
|      902 | 13275 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13276 |  |
|      904 | 13277 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13278 | `	ph7_value *pArray;` |
|        - | 13279 | `	ph7_class *pClass;` |
|        - | 13280 | `	ph7_value *pValue;` |
|        - | 13281 | `	SyString *pFile;` |
|        - | 13282 | `	/* Create a new array */` |
|      904 | 13283 | `	pArray = ph7_context_new_array(pCtx);` |
|      904 | 13284 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      904 | 13285 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13286 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13287 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13288 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13289 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13290 | `		SXUNUSED(apArg);` |
|      ! 0 | 13291 | `		return PH7_OK;` |
|        - | 13292 | `	}` |
|        - | 13293 | `	/* Dump running function name and it's arguments  */` |
|      904 | 13294 | `	if( pVm->pFrame->pParent ){` |
|      904 | 13295 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13296 | `		ph7_vm_func *pFunc;` |
|        - | 13297 | `		ph7_value *pArg;` |
|      904 | 13298 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      904 | 13299 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      904 | 13300 | `		if( pFrame->pParent && pFunc ){` |
|      904 | 13301 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      904 | 13302 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      904 | 13303 | `			ph7_value_reset_string_cursor(pValue);` |
|      451 | 13304 | `		}` |
|        - | 13305 | `		/* Function arguments */` |
|      904 | 13306 | `		pArg = ph7_context_new_array(pCtx);` |
|      904 | 13307 | `		if( pArg  ){` |
|        - | 13308 | `			ph7_value *pObj;` |
|        - | 13309 | `			VmSlot *aSlot;` |
|        - | 13310 | `			sxu32 n;` |
|        - | 13311 | `			/* Start filling the array with the given arguments */` |
|      904 | 13312 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3614 | 13313 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2712 | 13314 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2712 | 13315 | `				if( pObj ){` |
|     2712 | 13316 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1355 | 13317 | `				}` |
|     1357 | 13318 | `			}` |
|        - | 13319 | `			/* Save the array */` |
|      904 | 13320 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      451 | 13321 | `		}` |
|      451 | 13322 | `	}` |
|      904 | 13323 | `	ph7_value_int(pValue,1);` |
|        - | 13324 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13325 | `	 * line numbers at run-time. )` |
|        - | 13326 | `	 */` |
|      904 | 13327 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13328 | `	/* Current processed script */` |
|      904 | 13329 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      904 | 13330 | `	if( pFile ){` |
|      904 | 13331 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      904 | 13332 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      904 | 13333 | `		ph7_value_reset_string_cursor(pValue);` |
|      451 | 13334 | `	}` |
|        - | 13335 | `	/* Top class */` |
|      904 | 13336 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      904 | 13337 | `	if( pClass ){` |
|      900 | 13338 | `		ph7_value_reset_string_cursor(pValue);` |
|      900 | 13339 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      900 | 13340 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      449 | 13341 | `	}` |
|        - | 13342 | `	/* Return the freshly created array */` |
|      904 | 13343 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13344 | `	/*` |
|        - | 13345 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13346 | `	 * as soon we return from this function.` |
|        - | 13347 | `	 */` |
|      904 | 13348 | `	return PH7_OK;` |
|      453 | 13349 |  |
|        - | 13350 | `/*` |
|        - | 13351 | ` * Generate a small backtrace.` |
|        - | 13352 | ` * Store the generated dump in the given BLOB` |
|        - | 13353 | ` */` |
|        4 | 13354 | `static int VmMiniBacktrace(` |
|        - | 13355 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13356 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13357 | `	)` |
|        1 | 13358 |  |
|        5 | 13359 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13360 | `	ph7_vm_func *pFunc;` |
|        - | 13361 | `	ph7_class *pClass;` |
|        - | 13362 | `	SyString *pFile;` |
|        - | 13363 | `	/* Called function */` |
|        5 | 13364 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13365 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13366 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13367 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13368 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13369 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13370 | `	}else{` |
|      ! 0 | 13371 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13372 | `	}` |
|        5 | 13373 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13374 | `	/* Current processed script */` |
|        5 | 13375 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13376 | `	if( pFile ){` |
|        5 | 13377 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13378 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13379 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13380 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13381 | `	}` |
|        - | 13382 | `	/* Top class */` |
|        5 | 13383 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13384 | `	if( pClass ){` |
|      ! 0 | 13385 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13386 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13387 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13388 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13389 | `	}` |
|        5 | 13390 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13391 | `	/* All done */` |
|        5 | 13392 | `	return SXRET_OK;` |
|        1 | 13393 |  |
|        - | 13394 | `/*` |
|        - | 13395 | ` * void debug_print_backtrace()` |
|        - | 13396 | ` *  Prints a backtrace` |
|        - | 13397 | ` * Parameters` |
|        - | 13398 | ` * None` |
|        - | 13399 | ` * Return` |
|        - | 13400 | ` * NULL` |
|        - | 13401 | ` */` |
|        2 | 13402 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13403 |  |
|        3 | 13404 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13405 | `	SyBlob sDump;` |
|        3 | 13406 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13407 | `	/* Generate the backtrace */` |
|        3 | 13408 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13409 | `	/* Output backtrace */` |
|        3 | 13410 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13411 | `	/* All done,cleanup */` |
|        3 | 13412 | `	SyBlobRelease(&sDump);` |
|        1 | 13413 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13414 | `	SXUNUSED(apArg);` |
|        3 | 13415 | `	return PH7_OK;` |
|        1 | 13416 |  |
|        - | 13417 | `/*` |
|        - | 13418 | ` * string debug_string_backtrace()` |
|        - | 13419 | ` *  Generate a backtrace` |
|        - | 13420 | ` * Parameters` |
|        - | 13421 | ` * None` |
|        - | 13422 | ` * Return` |
|        - | 13423 | ` *  A mini backtrace().` |
|        - | 13424 | ` * Note that this is a symisc extension.` |
|        - | 13425 | ` */` |
|        2 | 13426 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13427 |  |
|        3 | 13428 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13429 | `	SyBlob sDump;` |
|        3 | 13430 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13431 | `	/* Generate the backtrace */` |
|        3 | 13432 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13433 | `	/* Return the backtrace */` |
|        3 | 13434 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13435 | `	/* All done,cleanup */` |
|        3 | 13436 | `	SyBlobRelease(&sDump);` |
|        1 | 13437 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13438 | `	SXUNUSED(apArg);` |
|        3 | 13439 | `	return PH7_OK;` |
|        1 | 13440 |  |
|        - | 13441 | `/*` |
|        - | 13442 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13443 | ` * exception is triggered.` |
|        - | 13444 | ` */` |
|      512 | 13445 | `static sxi32 VmUncaughtException(` |
|        - | 13446 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13447 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13448 | `	)` |
|        1 | 13449 |  |
|        - | 13450 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13451 | `	int nArg = 1;` |
|        - | 13452 | `	sxi32 rc;` |
|      513 | 13453 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13454 | `		/* Nesting limit reached */` |
|      ! 0 | 13455 | `		return SXRET_OK;` |
|        - | 13456 | `	}` |
|        - | 13457 | `	/* Call any exception handler if available */` |
|      513 | 13458 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13459 | `	if( pThis ){` |
|        - | 13460 | `		/* Load the exception instance */` |
|      513 | 13461 | `		sArg.x.pOther = pThis;` |
|      513 | 13462 | `		pThis->iRef++;` |
|      513 | 13463 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13464 | `	}else{` |
|      ! 0 | 13465 | `		nArg = 0;` |
|        - | 13466 | `	}` |
|      513 | 13467 | `	apArg[0] = &sArg;` |
|        - | 13468 | `	/* Call the exception handler if available */` |
|      513 | 13469 | `	pVm->nExceptDepth++;` |
|      513 | 13470 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13471 | `	pVm->nExceptDepth--;` |
|      513 | 13472 | `	if( rc != SXRET_OK ){` |
|        - | 13473 | `		SyBlob sMsgBuf;` |
|      511 | 13474 | `		const char *zClass = "Exception";` |
|      511 | 13475 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13476 | `		const char *zMsg;` |
|        - | 13477 | `		sxu32 nMsg;` |
|        - | 13478 | `		const char *zFuncName;` |
|        - | 13479 | `		int nFuncLen;` |
|      511 | 13480 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13481 | `		if( pThis ){` |
|        - | 13482 | `			ph7_class_method *pGetMessage;` |
|        - | 13483 | `			ph7_value sMsg;` |
|        - | 13484 | `			const char *zTmp;` |
|        - | 13485 | `			int nTmp;` |
|      511 | 13486 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13487 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13488 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13489 | `			if( pGetMessage ){` |
|      511 | 13490 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13491 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13492 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13493 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13494 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13495 | `					}` |
|      255 | 13496 | `				}` |
|      511 | 13497 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13498 | `			}` |
|      255 | 13499 | `		}` |
|      511 | 13500 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13501 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13502 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13503 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13504 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13505 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13506 | `		rc = SXERR_ABORT;` |
|      255 | 13507 | `	}` |
|      513 | 13508 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13509 | `	return rc;` |
|      257 | 13510 |  |
|        - | 13511 | `/*` |
|        - | 13512 | ` * Throw a user exception.` |
|        - | 13513 | ` *` |
|        - | 13514 | ` * Exception dispatch follows this sequence:` |
|        - | 13515 | ` *` |
|        - | 13516 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13517 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13518 | ` *` |
|        - | 13519 | ` * 2. If NO catch matches:` |
|        - | 13520 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13521 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13522 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13523 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13524 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13525 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13526 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13527 | ` *` |
|        - | 13528 | ` * 3. If a catch DOES match:` |
|        - | 13529 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13530 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13531 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13532 | ` *       finally block.` |
|        - | 13533 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13534 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13535 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13536 | ` *       in pPendingException (step 2c).` |
|        - | 13537 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13538 | ` *    d. Run finally (if present).` |
|        - | 13539 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13540 | ` *       that handlers are restored and finally has run.` |
|        - | 13541 | ` */` |
|      850 | 13542 | `static sxi32 VmThrowException(` |
|        - | 13543 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13544 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13545 | `	)` |
|        2 | 13546 |  |
|        - | 13547 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13548 | `	ph7_exception **apException;` |
|        - | 13549 | `	ph7_exception *pException;` |
|        - | 13550 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13551 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13552 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      852 | 13553 | `	VmCoalesceDisarm(pVm);` |
|        - | 13554 | `	/* Point to the stack of loaded exceptions */` |
|      852 | 13555 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      852 | 13556 | `	pException = 0;` |
|      852 | 13557 | `	pCatch = 0;` |
|      852 | 13558 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13559 | `		ph7_exception_block *aCatch;` |
|        - | 13560 | `		ph7_class *pClass;` |
|        - | 13561 | `		SyString *aNames;` |
|        - | 13562 | `		sxu32 nNames;` |
|        - | 13563 | `		int matched;` |
|        - | 13564 | `		sxu32 j,k;` |
|        - | 13565 | `		/* Locate the appropriate block to execute */` |
|      332 | 13566 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      332 | 13567 | `		(void)SySetPop(&pVm->aException);` |
|      332 | 13568 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      340 | 13569 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13570 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      338 | 13571 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      338 | 13572 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      338 | 13573 | `			matched = 0;` |
|      364 | 13574 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13575 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13576 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13577 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      356 | 13578 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      356 | 13579 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13580 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13581 | `					continue;` |
|        - | 13582 | `				}` |
|      356 | 13583 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      330 | 13584 | `					matched = 1;` |
|      330 | 13585 | `					break;` |
|        - | 13586 | `				}` |
|       14 | 13587 | `			}` |
|      338 | 13588 | `			if( matched ){` |
|        - | 13589 | `				/* Catch block found,break immediately */` |
|      330 | 13590 | `				pCatch = &aCatch[j];` |
|      330 | 13591 | `				break;` |
|        - | 13592 | `			}` |
|        5 | 13593 | `		}` |
|      165 | 13594 | `	}` |
|        - | 13595 | `	/* Execute the cached block if available */` |
|      852 | 13596 | `	if( pCatch == 0 ){` |
|        - | 13597 | `		sxi32 rc;` |
|        - | 13598 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13599 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13600 | `			pException->iFinallyDone = 1;` |
|        3 | 13601 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13602 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13603 | `				return SXERR_ABORT;` |
|        - | 13604 | `			}` |
|        1 | 13605 | `		}` |
|        - | 13606 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13607 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13608 | `			/* Re-throw to the outer handler */` |
|        3 | 13609 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13610 | `		}` |
|        - | 13611 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13612 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13613 | `		 * exception instead of reporting it uncaught.` |
|        - | 13614 | `		 */` |
|      522 | 13615 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13616 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13617 | `			 * by looking for a catch frame on the stack.` |
|        - | 13618 | `			 */` |
|      522 | 13619 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13620 | `			int inCatch = 0;` |
|     1050 | 13621 | `			while( pF ){` |
|      538 | 13622 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13623 | `					inCatch = 1;` |
|        9 | 13624 | `					break;` |
|        - | 13625 | `				}` |
|      529 | 13626 | `				pF = pF->pParent;` |
|        1 | 13627 | `			}` |
|      522 | 13628 | `			if( inCatch ){` |
|        - | 13629 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13630 | `				pThis->iRef++;` |
|        9 | 13631 | `				pVm->pPendingException = pThis;` |
|        9 | 13632 | `				return SXRET_OK;` |
|        - | 13633 | `			}` |
|      256 | 13634 | `		}` |
|        - | 13635 | `		/* Truly uncaught */` |
|      513 | 13636 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13637 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13638 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13639 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13640 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13641 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13642 | `			}` |
|      ! 0 | 13643 | `		}` |
|      513 | 13644 | `		return rc;` |
|      ! 0 | 13645 | `	}else{` |
|      330 | 13646 | `		VmFrame *pFrame = pVm->pFrame;` |
|      330 | 13647 | `		ph7_exception **apSaved = 0;` |
|        - | 13648 | `		sxu32 nSavedCount;` |
|        - | 13649 | `		sxi32 rc;` |
|      330 | 13650 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      330 | 13651 | `		if( pException->pFrame == pFrame ){` |
|      230 | 13652 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      114 | 13653 | `		}` |
|        - | 13654 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13655 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13656 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13657 | `		 */` |
|      330 | 13658 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      330 | 13659 | `		if( nSavedCount > 0 ){` |
|       16 | 13660 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13661 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13662 | `			if( apSaved ){` |
|       16 | 13663 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13664 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13665 | `				SySetReset(&pVm->aException);` |
|        5 | 13666 | `			}` |
|        5 | 13667 | `		}` |
|        - | 13668 | `		/* Create a private frame first */` |
|      330 | 13669 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      330 | 13670 | `		if( rc == SXRET_OK ){` |
|      330 | 13671 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      330 | 13672 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      330 | 13673 | `			if( pObj ){` |
|      330 | 13674 | `				pThis->iRef++;` |
|      330 | 13675 | `				pObj->x.pOther = pThis;` |
|      330 | 13676 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      164 | 13677 | `			}` |
|        - | 13678 | `			/* Execute the catch block */` |
|      330 | 13679 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13680 | `			/* Leave the frame */` |
|      330 | 13681 | `			VmLeaveFrame(&(*pVm));` |
|      164 | 13682 | `		}` |
|        - | 13683 | `		/* Restore the outer exception handlers */` |
|      330 | 13684 | `		if( apSaved ){` |
|        - | 13685 | `			sxu32 k;` |
|        - | 13686 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13687 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13688 | `			 * Restore the original outer entries.` |
|        - | 13689 | `			 */` |
|       11 | 13690 | `			SySetReset(&pVm->aException);` |
|       21 | 13691 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13692 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13693 | `			}` |
|       11 | 13694 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13695 | `		}` |
|        - | 13696 | `		/* Execute the finally block after catch */` |
|      330 | 13697 | `		if( pException->iHasFinally ){` |
|       16 | 13698 | `			pException->iFinallyDone = 1;` |
|        - | 13699 | `			{` |
|       16 | 13700 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13701 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13702 | `					return SXERR_ABORT;` |
|        - | 13703 | `				}` |
|        - | 13704 | `			}` |
|        7 | 13705 | `		}` |
|      330 | 13706 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13707 | `			return SXERR_ABORT;` |
|        - | 13708 | `		}` |
|        - | 13709 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13710 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13711 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13712 | `		 */` |
|      330 | 13713 | `		if( pVm->pPendingException ){` |
|        9 | 13714 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13715 | `			pVm->pPendingException = 0;` |
|        9 | 13716 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13717 | `		}` |
|        - | 13718 | `	}` |
|        - | 13719 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13720 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13721 | `	 */` |
|      322 | 13722 | `	return SXRET_OK;` |
|      427 | 13723 |  |
|        - | 13724 | `/*` |
|        - | 13725 | ` * Section:` |
|        - | 13726 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13727 | ` * Status:` |
|        - | 13728 | ` *    Stable.` |
|        - | 13729 | ` */` |
|        - | 13730 | `/*` |
|        - | 13731 | ` * string ph7version(void)` |
|        - | 13732 | ` *  Returns the running version of the PH7 version.` |
|        - | 13733 | ` * Parameters` |
|        - | 13734 | ` *  None` |
|        - | 13735 | ` * Return` |
|        - | 13736 | ` * Current PH7 version.` |
|        - | 13737 | ` */` |
|        2 | 13738 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13739 |  |
|        1 | 13740 | `	SXUNUSED(nArg);` |
|        1 | 13741 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13742 | `	/* Current engine version */` |
|        3 | 13743 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13744 | `	return PH7_OK;` |
|        1 | 13745 |  |
|        - | 13746 | `/*` |
|        - | 13747 | ` * string phpversion([ string $extension ])` |
|        - | 13748 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 13749 | ` * Parameters` |
|        - | 13750 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 13751 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 13752 | ` * Return` |
|        - | 13753 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 13754 | ` */` |
|        4 | 13755 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13756 |  |
|        2 | 13757 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 13758 | `	if( nArg > 0 ){` |
|      ! 0 | 13759 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13760 | `		return PH7_OK;` |
|        - | 13761 | `	}` |
|        5 | 13762 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 13763 | `	return PH7_OK;` |
|        3 | 13764 |  |
|        - | 13765 | `/*` |
|        - | 13766 | ` * string php_sapi_name(void)` |
|        - | 13767 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 13768 | ` * Parameters` |
|        - | 13769 | ` *  None` |
|        - | 13770 | ` * Return` |
|        - | 13771 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 13772 | ` */` |
|        2 | 13773 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13774 |  |
|        3 | 13775 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 13776 | `	SXUNUSED(nArg);` |
|        1 | 13777 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 13778 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 13779 | `	return PH7_OK;` |
|        1 | 13780 |  |
|        - | 13781 | `/*` |
|        - | 13782 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13783 | ` */` |
|        - | 13784 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13785 | ` "<html><head>"\` |
|        - | 13786 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13787 | ` "<style type=\"text/css\">"\` |
|        - | 13788 | ` "div {"\` |
|        - | 13789 | `     "border: 1px solid #cccccc;"\` |
|        - | 13790 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13791 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13792 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13793 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13794 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13795 | `     "-o-border-radius: 10px;"\` |
|        - | 13796 | `     "border-radius: 10px;"\` |
|        - | 13797 | `     "padding-left: 2em;"\` |
|        - | 13798 | `     "background-color: white;"\` |
|        - | 13799 | `     "margin-left: auto;"\` |
|        - | 13800 | `     "font-family: verdana;"\` |
|        - | 13801 | `     "padding-right: 2em;"\` |
|        - | 13802 | `     "margin-right: auto;"\` |
|        - | 13803 | `     "}"\` |
|        - | 13804 | `     "body {"\` |
|        - | 13805 | `     "padding: 0.2em;"\` |
|        - | 13806 | `     "font-style: normal;"\` |
|        - | 13807 | `     "font-size: medium;"\` |
|        - | 13808 | `     "background-color: #f2f2f2;"\` |
|        - | 13809 | `     "}"\` |
|        - | 13810 | `     "hr {"\` |
|        - | 13811 | `     "border-style: solid none none;"\` |
|        - | 13812 | `     "border-width: 1px medium medium;"\` |
|        - | 13813 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13814 | `     "height: 1px;"\` |
|        - | 13815 | `     "}"\` |
|        - | 13816 | `     "a {"\` |
|        - | 13817 | `     "color: #3366cc;"\` |
|        - | 13818 | `     "text-decoration: none;"\` |
|        - | 13819 | `     "}"\` |
|        - | 13820 | `     "a:hover {"\` |
|        - | 13821 | `     "color: #999999;"\` |
|        - | 13822 | `     "}"\` |
|        - | 13823 | `     "a:active {"\` |
|        - | 13824 | `     "color: #663399;"\` |
|        - | 13825 | `     "}"\` |
|        - | 13826 | `     "h1 {"\` |
|        - | 13827 | `     "margin: 0;"\` |
|        - | 13828 | `     "padding: 0;"\` |
|        - | 13829 | `     "font-family: Verdana;"\` |
|        - | 13830 | `     "font-weight: bold;"\` |
|        - | 13831 | `     "font-style: normal;"\` |
|        - | 13832 | `     "font-size: medium;"\` |
|        - | 13833 | `     "text-transform: capitalize;"\` |
|        - | 13834 | `     "color: #0a328c;"\` |
|        - | 13835 | `     "}"\` |
|        - | 13836 | `     "p {"\` |
|        - | 13837 | `     "margin: 0 auto;"\` |
|        - | 13838 | `     "font-size: medium;"\` |
|        - | 13839 | `     "font-style: normal;"\` |
|        - | 13840 | `     "font-family: verdana;"\` |
|        - | 13841 | `     "}"\` |
|        - | 13842 | `"</style></head><body>"\` |
|        - | 13843 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13844 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13845 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13846 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13847 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13848 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13849 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13850 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13851 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13852 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13853 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13854 |  |
|        - | 13855 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13856 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13857 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13858 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13859 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13860 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13861 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13862 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13863 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13864 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13865 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13866 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13867 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13868 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13869 |  |
|        - | 13870 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13871 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13872 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13873 | `"&nbsp;*<br>"\` |
|        - | 13874 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13875 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13876 | `"&nbsp;* are met:<br>"\` |
|        - | 13877 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13878 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13879 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13880 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13881 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13882 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13883 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13884 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13885 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13886 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13887 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13888 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13889 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13890 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13891 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13892 | `"&nbsp;*<br>"\` |
|        - | 13893 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13894 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13895 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13896 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13897 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13898 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13899 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13900 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13901 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13902 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13903 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13904 | `"&nbsp;*/<br>"\` |
|        - | 13905 | `"</span></small></small></p>"\` |
|        - | 13906 | `"</div></body></html>"` |
|        - | 13907 | `/*` |
|        - | 13908 | ` * bool ph7credits(void)` |
|        - | 13909 | ` * bool ph7info(void)` |
|        - | 13910 | ` * bool ph7copyright(void)` |
|        - | 13911 | ` *  Prints out the credits for PH7 engine` |
|        - | 13912 | ` * Parameters` |
|        - | 13913 | ` *  None` |
|        - | 13914 | ` * Return` |
|        - | 13915 | ` *  Always TRUE` |
|        - | 13916 | ` */` |
|        2 | 13917 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13918 |  |
|        3 | 13919 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13920 | `	/* Expand the HTML page above*/` |
|        3 | 13921 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13922 | `	ph7_context_output_format(` |
|        1 | 13923 | `		pCtx,` |
|        - | 13924 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13925 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13926 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13927 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13928 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13929 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13930 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13931 | `#ifdef __WINNT__` |
|        - | 13932 | `		"Windows NT"` |
|        - | 13933 | `#elif defined(__UNIXES__)` |
|        - | 13934 | `		"UNIX-Like"` |
|        - | 13935 | `#else` |
|        - | 13936 | `		"Other OS"` |
|        - | 13937 | `#endif` |
|        - | 13938 | `		);` |
|        3 | 13939 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13940 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13941 | `	SXUNUSED(apArg);` |
|        - | 13942 | `	/* Return TRUE */` |
|        - | 13943 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13944 | `	return PH7_OK;` |
|        1 | 13945 |  |
|        - | 13946 | `/*` |
|        - | 13947 | ` * Section:` |
|        - | 13948 | ` *    URL related routines.` |
|        - | 13949 | ` * Status:` |
|        - | 13950 | ` *    Stable.` |
|        - | 13951 | ` */` |
|        - | 13952 | `/*` |
|        - | 13953 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13954 | ` *  Parse a URL and return its fields.` |
|        - | 13955 | ` * Parameters` |
|        - | 13956 | ` *  $url` |
|        - | 13957 | ` *   The URL to parse.` |
|        - | 13958 | ` * $component` |
|        - | 13959 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13960 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13961 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13962 | ` *  in which case the return value will be an integer).` |
|        - | 13963 | ` * Return` |
|        - | 13964 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13965 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13966 | ` *  this array are:` |
|        - | 13967 | ` *   scheme - e.g. http` |
|        - | 13968 | ` *   host` |
|        - | 13969 | ` *   port` |
|        - | 13970 | ` *   user` |
|        - | 13971 | ` *   pass` |
|        - | 13972 | ` *   path` |
|        - | 13973 | ` *   query - after the question mark ?` |
|        - | 13974 | ` *   fragment - after the hashmark #` |
|        - | 13975 | ` * Note:` |
|        - | 13976 | ` *  FALSE is returned on failure.` |
|        - | 13977 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13978 | ` *  with the standard PHP engine.` |
|        - | 13979 | ` */` |
|       28 | 13980 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13981 |  |
|        - | 13982 | `	const char *zStr; /* Input string */` |
|        - | 13983 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13984 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13985 | `	int nLen;` |
|        - | 13986 | `	sxi32 rc;` |
|       29 | 13987 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13988 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13989 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13990 | `		return PH7_OK;` |
|        - | 13991 | `	}` |
|        - | 13992 | `	/* Extract the given URI */` |
|       29 | 13993 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13994 | `	if( nLen < 1 ){` |
|        - | 13995 | `		/* Nothing to process,return FALSE */` |
|        3 | 13996 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13997 | `		return PH7_OK;` |
|        - | 13998 | `	}` |
|        - | 13999 | `	/* Get a parse */` |
|       27 | 14000 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14001 | `	if( rc != SXRET_OK ){` |
|        - | 14002 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14003 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14004 | `		return PH7_OK;` |
|        - | 14005 | `	}` |
|       27 | 14006 | `	if( nArg > 1 ){` |
|      ! 0 | 14007 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14008 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14009 | `		switch(nComponent){` |
|      ! 0 | 14010 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14011 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14012 | `			if( pComp->nByte < 1 ){` |
|        - | 14013 | `				/* No available value,return NULL */` |
|      ! 0 | 14014 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14015 | `			}else{` |
|      ! 0 | 14016 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14017 | `			}` |
|      ! 0 | 14018 | `			break;` |
|      ! 0 | 14019 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14020 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14021 | `			if( pComp->nByte < 1 ){` |
|        - | 14022 | `				/* No available value,return NULL */` |
|      ! 0 | 14023 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14024 | `			}else{` |
|      ! 0 | 14025 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14026 | `			}` |
|      ! 0 | 14027 | `			break;` |
|      ! 0 | 14028 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14029 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14030 | `			if( pComp->nByte < 1 ){` |
|        - | 14031 | `				/* No available value,return NULL */` |
|      ! 0 | 14032 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14033 | `			}else{` |
|      ! 0 | 14034 | `				int iPort = 0;` |
|        - | 14035 | `				/* Cast the value to integer */` |
|      ! 0 | 14036 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14037 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14038 | `			}` |
|      ! 0 | 14039 | `			break;` |
|      ! 0 | 14040 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14041 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14042 | `			if( pComp->nByte < 1 ){` |
|        - | 14043 | `				/* No available value,return NULL */` |
|      ! 0 | 14044 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14045 | `			}else{` |
|      ! 0 | 14046 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14047 | `			}` |
|      ! 0 | 14048 | `			break;` |
|      ! 0 | 14049 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14050 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14051 | `			if( pComp->nByte < 1 ){` |
|        - | 14052 | `				/* No available value,return NULL */` |
|      ! 0 | 14053 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14054 | `			}else{` |
|      ! 0 | 14055 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14056 | `			}` |
|      ! 0 | 14057 | `			break;` |
|      ! 0 | 14058 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14059 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14060 | `			if( pComp->nByte < 1 ){` |
|        - | 14061 | `				/* No available value,return NULL */` |
|      ! 0 | 14062 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14063 | `			}else{` |
|      ! 0 | 14064 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14065 | `			}` |
|      ! 0 | 14066 | `			break;` |
|      ! 0 | 14067 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14068 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14069 | `			if( pComp->nByte < 1 ){` |
|        - | 14070 | `				/* No available value,return NULL */` |
|      ! 0 | 14071 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14072 | `			}else{` |
|      ! 0 | 14073 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14074 | `			}` |
|      ! 0 | 14075 | `			break;` |
|      ! 0 | 14076 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14077 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14078 | `			if( pComp->nByte < 1 ){` |
|        - | 14079 | `				/* No available value,return NULL */` |
|      ! 0 | 14080 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14081 | `			}else{` |
|      ! 0 | 14082 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14083 | `			}` |
|      ! 0 | 14084 | `			break;` |
|      ! 0 | 14085 | `		default:` |
|        - | 14086 | `			/* No such entry,return NULL */` |
|      ! 0 | 14087 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14088 | `			break;` |
|        - | 14089 | `		}` |
|      ! 0 | 14090 | `	}else{` |
|        - | 14091 | `		ph7_value *pArray,*pValue;` |
|        - | 14092 | `		/* Return an associative array */` |
|       27 | 14093 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14094 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14095 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14096 | `			/* Out of memory */` |
|      ! 0 | 14097 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14098 | `			/* Return false */` |
|      ! 0 | 14099 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14100 | `			return PH7_OK;` |
|        - | 14101 | `		}` |
|        - | 14102 | `		/* Fill the array */` |
|       27 | 14103 | `		pComp = &sURI.sScheme;` |
|       27 | 14104 | `		if( pComp->nByte > 0 ){` |
|       19 | 14105 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14106 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14107 | `		}` |
|        - | 14108 | `		/* Reset the string cursor */` |
|       27 | 14109 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14110 | `		pComp = &sURI.sHost;` |
|       27 | 14111 | `		if( pComp->nByte > 0 ){` |
|       25 | 14112 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14113 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14114 | `		}` |
|        - | 14115 | `		/* Reset the string cursor */` |
|       27 | 14116 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14117 | `		pComp = &sURI.sPort;` |
|       27 | 14118 | `		if( pComp->nByte > 0 ){` |
|       11 | 14119 | `			int iPort = 0;/* cc warning */` |
|        - | 14120 | `			/* Convert to integer */` |
|       11 | 14121 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14122 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14123 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14124 | `		}` |
|        - | 14125 | `		/* Reset the string cursor */` |
|       27 | 14126 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14127 | `		pComp = &sURI.sUser;` |
|       27 | 14128 | `		if( pComp->nByte > 0 ){` |
|        7 | 14129 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14130 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14131 | `		}` |
|        - | 14132 | `		/* Reset the string cursor */` |
|       27 | 14133 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14134 | `		pComp = &sURI.sPass;` |
|       27 | 14135 | `		if( pComp->nByte > 0 ){` |
|        7 | 14136 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14137 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14138 | `		}` |
|        - | 14139 | `		/* Reset the string cursor */` |
|       27 | 14140 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14141 | `		pComp = &sURI.sPath;` |
|       27 | 14142 | `		if( pComp->nByte > 0 ){` |
|       17 | 14143 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14144 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14145 | `		}` |
|        - | 14146 | `		/* Reset the string cursor */` |
|       27 | 14147 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14148 | `		pComp = &sURI.sQuery;` |
|       27 | 14149 | `		if( pComp->nByte > 0 ){` |
|        5 | 14150 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14151 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14152 | `		}` |
|        - | 14153 | `		/* Reset the string cursor */` |
|       27 | 14154 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14155 | `		pComp = &sURI.sFragment;` |
|       27 | 14156 | `		if( pComp->nByte > 0 ){` |
|        5 | 14157 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14158 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14159 | `		}` |
|        - | 14160 | `		/* Return the created array */` |
|       27 | 14161 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14162 | `		/* NOTE:` |
|        - | 14163 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14164 | `		 * automatically as soon we return from this function.` |
|        - | 14165 | `		 */` |
|        - | 14166 | `	}` |
|        - | 14167 | `	/* All done */` |
|       27 | 14168 | `	return PH7_OK;` |
|       15 | 14169 |  |
|        - | 14170 | `/*` |
|        - | 14171 | ` * Section:` |
|        - | 14172 | ` *   Array related routines.` |
|        - | 14173 | ` * Status:` |
|        - | 14174 | ` *    Stable.` |
|        - | 14175 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14176 | ` *  Array related functions that need access to the underlying` |
|        - | 14177 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14178 | ` */` |
|        - | 14179 | `/*` |
|        - | 14180 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14181 | ` * of the following structure.` |
|        - | 14182 | ` */` |
|        - | 14183 | `struct compact_data` |
|        - | 14184 |  |
|        - | 14185 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14186 | `	int nRecCount;      /* Recursion count */` |
|        - | 14187 | `};` |
|        - | 14188 | `/*` |
|        - | 14189 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14190 | ` */` |
|      ! 0 | 14191 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14192 |  |
|      ! 0 | 14193 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14194 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14195 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14196 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14197 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14198 | `		SyString sVar;` |
|      ! 0 | 14199 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14200 | `		if( sVar.nByte > 0 ){` |
|        - | 14201 | `			/* Query the current frame */` |
|      ! 0 | 14202 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14203 | `			/* ^` |
|        - | 14204 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14205 | `			 */` |
|      ! 0 | 14206 | `			if( pKey ){` |
|        - | 14207 | `				/* Perform the insertion */` |
|      ! 0 | 14208 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14209 | `			}` |
|      ! 0 | 14210 | `		}` |
|      ! 0 | 14211 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14212 | `		int rc;` |
|        - | 14213 | `		/* Recursively traverse this array */` |
|      ! 0 | 14214 | `		pData->nRecCount++;` |
|      ! 0 | 14215 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14216 | `		pData->nRecCount--;` |
|      ! 0 | 14217 | `		return rc;` |
|        - | 14218 | `	}` |
|      ! 0 | 14219 | `	return SXRET_OK;` |
|      ! 0 | 14220 |  |
|        - | 14221 | `/*` |
|        - | 14222 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14223 | ` *  Create array containing variables and their values.` |
|        - | 14224 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14225 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14226 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14227 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14228 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14229 | ` * Parameters` |
|        - | 14230 | ` *  $varname` |
|        - | 14231 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14232 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14233 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14234 | ` *   it recursively.` |
|        - | 14235 | ` * Return` |
|        - | 14236 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14237 | ` */` |
|        2 | 14238 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14239 |  |
|        - | 14240 | `	ph7_value *pArray,*pObj;` |
|        3 | 14241 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14242 | `	const char *zName;` |
|        - | 14243 | `	SyString sVar;` |
|        - | 14244 | `	int i,nLen;` |
|        3 | 14245 | `	if( nArg < 1 ){` |
|        - | 14246 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14247 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14248 | `		return PH7_OK;` |
|        - | 14249 | `	}` |
|        - | 14250 | `	/* Create the array */` |
|        3 | 14251 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14252 | `	if( pArray == 0 ){` |
|        - | 14253 | `		/* Out of memory */` |
|      ! 0 | 14254 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14255 | `		/* Return NULL */` |
|      ! 0 | 14256 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14257 | `		return PH7_OK;` |
|        - | 14258 | `	}` |
|        - | 14259 | `	/* Perform the requested operation */` |
|        7 | 14260 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14261 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14262 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14263 | `				struct compact_data sData;` |
|      ! 0 | 14264 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14265 | `				/* Recursively walk the array */` |
|      ! 0 | 14266 | `				sData.nRecCount = 0;` |
|      ! 0 | 14267 | `				sData.pArray = pArray;` |
|      ! 0 | 14268 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14269 | `			}` |
|      ! 0 | 14270 | `		}else{` |
|        - | 14271 | `			/* Extract variable name */` |
|        5 | 14272 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14273 | `			if( nLen > 0 ){` |
|        5 | 14274 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14275 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14276 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14277 | `				if( pObj ){` |
|        5 | 14278 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14279 | `				}` |
|        2 | 14280 | `			}` |
|        - | 14281 | `		}` |
|        3 | 14282 | `	}` |
|        - | 14283 | `	/* Return the array */` |
|        3 | 14284 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14285 | `	return PH7_OK;` |
|        2 | 14286 |  |
|        - | 14287 | `/*` |
|        - | 14288 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14289 | ` * of the following structure.` |
|        - | 14290 | ` */` |
|        - | 14291 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14292 | `struct extract_aux_data` |
|        - | 14293 |  |
|        - | 14294 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14295 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14296 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14297 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14298 | `	int iFlags;           /* Control flags */` |
|        - | 14299 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14300 | `};` |
|        - | 14301 | `/* Forward declaration */` |
|        - | 14302 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14303 | `/*` |
|        - | 14304 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14305 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14306 | ` * Parameters` |
|        - | 14307 | ` * $var_array` |
|        - | 14308 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14309 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14310 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14311 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14312 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14313 | ` * $extract_type` |
|        - | 14314 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14315 | ` *  It can be one of the following values:` |
|        - | 14316 | ` *   EXTR_OVERWRITE` |
|        - | 14317 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14318 | ` *   EXTR_SKIP` |
|        - | 14319 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14320 | ` *   EXTR_PREFIX_SAME` |
|        - | 14321 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14322 | ` *   EXTR_PREFIX_ALL` |
|        - | 14323 | ` *       Prefix all variable names with prefix.` |
|        - | 14324 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14325 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14326 | ` *   EXTR_IF_EXISTS` |
|        - | 14327 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14328 | ` *       otherwise do nothing.` |
|        - | 14329 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14330 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14331 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14332 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14333 | ` *      the current symbol table.` |
|        - | 14334 | ` * $prefix` |
|        - | 14335 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14336 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14337 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14338 | ` *  underscore character.` |
|        - | 14339 | ` * Return` |
|        - | 14340 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14341 | ` */` |
|        4 | 14342 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14343 |  |
|        - | 14344 | `	extract_aux_data sAux;` |
|        - | 14345 | `	ph7_hashmap *pMap;` |
|        5 | 14346 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14347 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14348 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14349 | `		return PH7_OK;` |
|        - | 14350 | `	}` |
|        - | 14351 | `	/* Point to the target hashmap */` |
|        5 | 14352 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14353 | `	if( pMap->nEntry < 1 ){` |
|        - | 14354 | `		/* Empty map,return  0 */` |
|      ! 0 | 14355 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14356 | `		return PH7_OK;` |
|        - | 14357 | `	}` |
|        - | 14358 | `	/* Prepare the aux data */` |
|        5 | 14359 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14360 | `	if( nArg > 1 ){` |
|        3 | 14361 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14362 | `		if( nArg > 2 ){` |
|      ! 0 | 14363 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14364 | `		}` |
|        1 | 14365 | `	}` |
|        5 | 14366 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14367 | `	/* Invoke the worker callback */` |
|        5 | 14368 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14369 | `	/* Number of variables successfully imported */` |
|        5 | 14370 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14371 | `	return PH7_OK;` |
|        3 | 14372 |  |
|        - | 14373 | `/*` |
|        - | 14374 | ` * Worker callback for the [extract()] function defined` |
|        - | 14375 | ` * below.` |
|        - | 14376 | ` */` |
|        8 | 14377 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14378 |  |
|        9 | 14379 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14380 | `	int iFlags = pAux->iFlags;` |
|        9 | 14381 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14382 | `	ph7_value *pObj;` |
|        - | 14383 | `	SyString sVar;` |
|        9 | 14384 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14385 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14386 | `	}` |
|        - | 14387 | `	/* Perform a string cast */` |
|        9 | 14388 | `	PH7_MemObjToString(pKey);` |
|        9 | 14389 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14390 | `		/* Unavailable variable name */` |
|      ! 0 | 14391 | `		return SXRET_OK;` |
|        - | 14392 | `	}` |
|        9 | 14393 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14394 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14395 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14396 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14397 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14398 | `			);` |
|      ! 0 | 14399 | `	}else{` |
|       13 | 14400 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14401 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14402 | `	}` |
|        9 | 14403 | `	sVar.zString = pAux->zWorker;` |
|        - | 14404 | `	/* Try to extract the variable */` |
|        9 | 14405 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14406 | `	if( pObj ){` |
|        - | 14407 | `		/* Collision */` |
|        5 | 14408 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14409 | `			return SXRET_OK;` |
|        - | 14410 | `		}` |
|        5 | 14411 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14412 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14413 | `				/* Already prefixed */` |
|      ! 0 | 14414 | `				return SXRET_OK;` |
|        - | 14415 | `			}` |
|      ! 0 | 14416 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14417 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14418 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14419 | `				);` |
|      ! 0 | 14420 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14421 | `		}` |
|        3 | 14422 | `	}else{` |
|        - | 14423 | `		/* Create the variable */` |
|        5 | 14424 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14425 | `	}` |
|        9 | 14426 | `	if( pObj ){` |
|        - | 14427 | `		/* Overwrite the old value */` |
|        9 | 14428 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14429 | `		/* Increment counter */` |
|        9 | 14430 | `		pAux->iCount++;` |
|        4 | 14431 | `	}` |
|        9 | 14432 | `	return SXRET_OK;` |
|        5 | 14433 |  |
|        - | 14434 | `/*` |
|        - | 14435 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14436 | ` * defined below.` |
|        - | 14437 | ` */` |
|        2 | 14438 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14439 |  |
|        3 | 14440 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14441 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14442 | `	ph7_value *pObj;` |
|        - | 14443 | `	SyString sVar;` |
|        - | 14444 | `	/* Perform a string cast */` |
|        3 | 14445 | `	PH7_MemObjToString(pKey);` |
|        3 | 14446 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14447 | `		/* Unavailable variable name */` |
|      ! 0 | 14448 | `		return SXRET_OK;` |
|        - | 14449 | `	}` |
|        3 | 14450 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14451 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14452 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14453 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14454 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14455 | `			);` |
|        2 | 14456 | `	}else{` |
|      ! 0 | 14457 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14458 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14459 | `	}` |
|        3 | 14460 | `	sVar.zString = pAux->zWorker;` |
|        - | 14461 | `	/* Extract the variable */` |
|        3 | 14462 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14463 | `	if( pObj ){` |
|        3 | 14464 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14465 | `	}` |
|        3 | 14466 | `	return SXRET_OK;` |
|        2 | 14467 |  |
|        - | 14468 | `/*` |
|        - | 14469 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14470 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14471 | ` * Parameters` |
|        - | 14472 | ` * $types` |
|        - | 14473 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14474 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14475 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14476 | ` *  POST includes the POST uploaded file information.` |
|        - | 14477 | ` *  Note:` |
|        - | 14478 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14479 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14480 | ` * $prefix` |
|        - | 14481 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14482 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14483 | ` *  variable named $pref_userid.` |
|        - | 14484 | ` * Return` |
|        - | 14485 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14486 | ` */` |
|        2 | 14487 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14488 |  |
|        - | 14489 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14490 | `	extract_aux_data sAux;` |
|        - | 14491 | `	int nLen,nPrefixLen;` |
|        - | 14492 | `	ph7_value *pSuper;` |
|        - | 14493 | `	ph7_vm *pVm;` |
|        - | 14494 | `	/* By default import only $_GET variables  */` |
|        3 | 14495 | `	zImport = "G";` |
|        3 | 14496 | `	nLen = (int)sizeof(char);` |
|        3 | 14497 | `	zPrefix = 0;` |
|        3 | 14498 | `	nPrefixLen = 0;` |
|        3 | 14499 | `	if( nArg > 0 ){` |
|        3 | 14500 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14501 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14502 | `		}` |
|        3 | 14503 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14504 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14505 | `		}` |
|        1 | 14506 | `	}` |
|        - | 14507 | `	/* Point to the underlying VM */` |
|        3 | 14508 | `	pVm = pCtx->pVm;` |
|        - | 14509 | `	/* Initialize the aux data */` |
|        3 | 14510 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14511 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14512 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14513 | `	sAux.pVm = pVm;` |
|        - | 14514 | `	/* Extract */` |
|        3 | 14515 | `	zEnd = &zImport[nLen];` |
|        5 | 14516 | `	while( zImport < zEnd ){` |
|        3 | 14517 | `		int c = zImport[0];` |
|        3 | 14518 | `		pSuper = 0;` |
|        3 | 14519 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14520 | `			/* Import $_GET variables */` |
|        3 | 14521 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14522 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14523 | `			/* Import $_POST variables */` |
|      ! 0 | 14524 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14525 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14526 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14527 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14528 | `		}` |
|        3 | 14529 | `		if( pSuper ){` |
|        - | 14530 | `			/* Iterate throw array entries */` |
|        3 | 14531 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14532 | `		}` |
|        - | 14533 | `		/* Advance the cursor */` |
|        3 | 14534 | `		zImport++;` |
|        1 | 14535 | `	}` |
|        - | 14536 | `	/* All done,return TRUE*/` |
|        3 | 14537 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14538 | `	return PH7_OK;` |
|        1 | 14539 |  |
|        - | 14540 | `/*` |
|        - | 14541 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14542 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14543 | ` * information.` |
|        - | 14544 | ` */` |
|    12702 | 14545 | `static sxi32 VmEvalChunk(` |
|        - | 14546 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14547 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14548 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14549 | `	int iFlags,         /* Compile flag */` |
|        - | 14550 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14551 | `	)` |
|        2 | 14552 |  |
|        - | 14553 | `	SySet *pByteCode,aByteCode;` |
|        - | 14554 | `	SyBlob sSavedNs;` |
|    12704 | 14555 | `	ProcConsumer xErr = 0;` |
|    12704 | 14556 | `	void *pErrData = 0;` |
|        - | 14557 | `	/* Initialize bytecode container */` |
|    12704 | 14558 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12704 | 14559 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14560 | `	/* Reset the code generator */` |
|    12704 | 14561 | `	if( bTrueReturn ){` |
|        - | 14562 | `		/* Included file,log compile-time errors */` |
|     9548 | 14563 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9548 | 14564 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4773 | 14565 | `	}` |
|    12704 | 14566 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14567 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14568 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14569 | `	 * the caller's namespace is restored. */` |
|    12704 | 14570 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12704 | 14571 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12704 | 14572 | `	if( bTrueReturn ){` |
|        - | 14573 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9548 | 14574 | `		SyBlobReset(&pVm->sNamespace);` |
|     4773 | 14575 | `	}` |
|        - | 14576 | `	/* Swap bytecode container */` |
|    12704 | 14577 | `	pByteCode = pVm->pByteContainer;` |
|    12704 | 14578 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14579 | `	/* Compile the chunk */` |
|    12704 | 14580 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19055 | 14581 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14582 | `		/* Compilation error,return false */` |
|        3 | 14583 | `		if( pCtx ){` |
|        3 | 14584 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14585 | `		}` |
|        2 | 14586 | `	}else{` |
|        - | 14587 | `		/* Mount any newly defined classes */` |
|        - | 14588 | `		SyHashEntry *pEntry;` |
|        - | 14589 | `		ph7_class *pClass;` |
|        - | 14590 | `		ph7_value sResult; /* Return value */` |
|        - | 14591 | `		sxi32 rc;` |
|    12702 | 14592 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   794892 | 14593 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   775842 | 14594 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14595 | `			/* Only mount classes that haven't been mounted yet */` |
|   775842 | 14596 | `			if( !pClass->bMounted ){` |
|   204686 | 14597 | `				rc = VmMountUserClass(pVm,pClass);` |
|   204686 | 14598 | `				if( rc != SXRET_OK ){` |
|        - | 14599 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14600 | `					if( pCtx ){` |
|      ! 0 | 14601 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14602 | `					}` |
|      ! 0 | 14603 | `					goto Cleanup;` |
|        - | 14604 | `				}` |
|   102342 | 14605 | `			}` |
|        2 | 14606 | `		}` |
|    12702 | 14607 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14608 | `			/* Out of memory */` |
|      ! 0 | 14609 | `			if( pCtx ){` |
|      ! 0 | 14610 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14611 | `			}` |
|      ! 0 | 14612 | `			goto Cleanup;` |
|        - | 14613 | `		}` |
|    12702 | 14614 | `		if( bTrueReturn ){` |
|        - | 14615 | `			/* Assume a boolean true return value */` |
|     9548 | 14616 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4775 | 14617 | `		}else{` |
|        - | 14618 | `			/* Assume a null return value */` |
|     3156 | 14619 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14620 | `		}` |
|        - | 14621 | `		/* Execute the compiled chunk */` |
|    12702 | 14622 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12702 | 14623 | `		if( pCtx ){` |
|        - | 14624 | `			/* Set the execution result */` |
|     9568 | 14625 | `			ph7_result_value(pCtx,&sResult);` |
|     4783 | 14626 | `		}` |
|    12702 | 14627 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14628 | `	}` |
|     6351 | 14629 | `Cleanup:` |
|        - | 14630 | `	/* Cleanup the mess left behind */` |
|    12704 | 14631 | `	pVm->pByteContainer = pByteCode;` |
|    12704 | 14632 | `	SySetRelease(&aByteCode);` |
|        - | 14633 | `	/* Restore caller's namespace state */` |
|    12704 | 14634 | `	SyBlobReset(&pVm->sNamespace);` |
|    12704 | 14635 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12704 | 14636 | `	SyBlobRelease(&sSavedNs);` |
|    12704 | 14637 | `	return SXRET_OK;` |
|        2 | 14638 |  |
|        - | 14639 | `/*` |
|        - | 14640 | ` * value eval(string $code)` |
|        - | 14641 | ` *   Evaluate a string as PHP code.` |
|        - | 14642 | ` * Parameter` |
|        - | 14643 | ` *  code: PHP code to evaluate.` |
|        - | 14644 | ` * Return` |
|        - | 14645 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14646 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14647 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14648 | ` */` |
|       24 | 14649 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14650 |  |
|        - | 14651 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 14652 | `	if( nArg < 1 ){` |
|        - | 14653 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14654 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14655 | `		return SXRET_OK;` |
|        - | 14656 | `	}` |
|        - | 14657 | `	/* Chunk to evaluate */` |
|       26 | 14658 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 14659 | `	if( sChunk.nByte < 1 ){` |
|        - | 14660 | `		/* Empty string,return NULL */` |
|        3 | 14661 | `		ph7_result_null(pCtx);` |
|        3 | 14662 | `		return SXRET_OK;` |
|        - | 14663 | `	}` |
|        - | 14664 | `	/* Eval the chunk */` |
|       24 | 14665 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 14666 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14667 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 14668 | `		return PH7_ABORT;` |
|        - | 14669 | `	}` |
|       22 | 14670 | `	return SXRET_OK;` |
|       14 | 14671 |  |
|        - | 14672 | `/*` |
|        - | 14673 | ` * Check if a file path is already included.` |
|        - | 14674 | ` */` |
|    19088 | 14675 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14676 |  |
|        - | 14677 | `	SyString *aEntries;` |
|        - | 14678 | `	sxu32 n;` |
|    19090 | 14679 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14680 | `	/* Perform a linear search */` |
| 90955328 | 14681 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 90936246 | 14682 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14683 | `			/* Already included */` |
|        7 | 14684 | `			return TRUE;` |
|        - | 14685 | `		}` |
| 45468121 | 14686 | `	}` |
|    19084 | 14687 | `	return FALSE;` |
|     9546 | 14688 |  |
|        - | 14689 | `/*` |
|        - | 14690 | ` * Push a file path in the appropriate VM container.` |
|        - | 14691 | ` */` |
|    22214 | 14692 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14693 |  |
|        - | 14694 | `	SyString sPath;` |
|        - | 14695 | `	char *zDup;` |
|        - | 14696 | `#ifdef __WINNT__` |
|        - | 14697 | `	char *zCur;` |
|        - | 14698 | `#endif` |
|        - | 14699 | `	sxi32 rc;` |
|    22216 | 14700 | `	if( nLen < 0 ){` |
|     3128 | 14701 | `		nLen = SyStrlen(zPath);` |
|     1563 | 14702 | `	}` |
|        - | 14703 | `	/* Duplicate the file path first */` |
|    22216 | 14704 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22216 | 14705 | `	if( zDup == 0 ){` |
|      ! 0 | 14706 | `		return SXERR_MEM;` |
|        - | 14707 | `	}` |
|        - | 14708 | `#ifdef __WINNT__` |
|        - | 14709 | `	/* Normalize path on windows` |
|        - | 14710 | `	 * Example:` |
|        - | 14711 | `	 *    Path/To/File.php` |
|        - | 14712 | `	 * becomes` |
|        - | 14713 | `	 *   path\to\file.php` |
|        - | 14714 | `	 */` |
|        2 | 14715 | `	zCur = zDup;` |
|        2 | 14716 | `	while( zCur[0] != 0 ){` |
|        2 | 14717 | `		if( zCur[0] == '/' ){` |
|        2 | 14718 | `			zCur[0] = '\\';` |
|        2 | 14719 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14720 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14721 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14722 | `		}` |
|        2 | 14723 | `		zCur++;` |
|        2 | 14724 | `	}` |
|        - | 14725 | `#endif` |
|        - | 14726 | `	/* Install the file path */` |
|    22216 | 14727 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22216 | 14728 | `	if( !bMain ){` |
|    19090 | 14729 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14730 | `			/* Already included */` |
|        7 | 14731 | `			*pNew = 0;` |
|        4 | 14732 | `		}else{` |
|        - | 14733 | `			/* Insert in the corresponding container */` |
|    19084 | 14734 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19084 | 14735 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14736 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14737 | `				return rc;` |
|        - | 14738 | `			}` |
|    19084 | 14739 | `			*pNew = 1;` |
|        - | 14740 | `		}` |
|     9544 | 14741 | `	}` |
|    22216 | 14742 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22216 | 14743 | `	return SXRET_OK;` |
|    11109 | 14744 |  |
|        - | 14745 | `/*` |
|        - | 14746 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14747 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14748 | ` * indicates failure.` |
|        - | 14749 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14750 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14751 | ` * operations.` |
|        - | 14752 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14753 | ` * this function is a no-op.` |
|        - | 14754 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14755 | ` * constructs for more information.` |
|        - | 14756 | ` */` |
|     9556 | 14757 | `static sxi32 VmExecIncludedFile(` |
|        - | 14758 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14759 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14760 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14761 | `	 )` |
|        2 | 14762 |  |
|        - | 14763 | `	sxi32 rc;` |
|        - | 14764 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14765 | `	const ph7_io_stream *pStream;` |
|        - | 14766 | `	SyBlob sContents;` |
|        - | 14767 | `	void *pHandle;` |
|        - | 14768 | `	ph7_vm *pVm;` |
|        - | 14769 | `	int isNew;` |
|        - | 14770 | `	/* Initialize fields */` |
|     9558 | 14771 | `	pVm = pCtx->pVm;` |
|     9558 | 14772 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9558 | 14773 | `	isNew = 0;` |
|        - | 14774 | `	/* Extract the associated stream */` |
|     9558 | 14775 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14776 | `	/*` |
|        - | 14777 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14778 | `	 * in a read-only mode.` |
|        - | 14779 | `	 */` |
|     9558 | 14780 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9558 | 14781 | `	if( pHandle == 0 ){` |
|        8 | 14782 | `		return SXERR_IO;` |
|        - | 14783 | `	}` |
|     9552 | 14784 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9552 | 14785 | `	if( IncludeOnce && !isNew ){` |
|        - | 14786 | `		/* Already included */` |
|        5 | 14787 | `		rc = SXERR_EXISTS;` |
|        3 | 14788 | `	}else{` |
|        - | 14789 | `		/* Read the whole file contents */` |
|     9548 | 14790 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9548 | 14791 | `		if( rc == SXRET_OK ){` |
|        - | 14792 | `			SyString sScript;` |
|        - | 14793 | `			/* Compile and execute the script */` |
|     9548 | 14794 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9548 | 14795 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4773 | 14796 | `		}` |
|        - | 14797 | `	}` |
|        - | 14798 | `	/* Pop from the set of included file */` |
|     9552 | 14799 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14800 | `	/* Close the handle */` |
|     9552 | 14801 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14802 | `	/* Release the working buffer */` |
|     9552 | 14803 | `	SyBlobRelease(&sContents);` |
|        - | 14804 | `#else` |
|        - | 14805 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14806 | `	SXUNUSED(pPath);` |
|        - | 14807 | `	SXUNUSED(IncludeOnce);` |
|        - | 14808 | `	rc = SXERR_IO;` |
|        - | 14809 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9552 | 14810 | `	return rc;` |
|     4780 | 14811 |  |
|        - | 14812 | `/*` |
|        - | 14813 | ` * string get_include_path(void)` |
|        - | 14814 | ` *  Gets the current include_path configuration option.` |
|        - | 14815 | ` * Parameter` |
|        - | 14816 | ` *  None` |
|        - | 14817 | ` * Return` |
|        - | 14818 | ` *  Included paths as a string` |
|        - | 14819 | ` */` |
|        2 | 14820 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14821 |  |
|        3 | 14822 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14823 | `	SyString *aEntry;` |
|        - | 14824 | `	int dir_sep;` |
|        - | 14825 | `	sxu32 n;` |
|        - | 14826 | `#ifdef __WINNT__` |
|        1 | 14827 | `	dir_sep = ';';` |
|        - | 14828 | `#else` |
|        - | 14829 | `	/* Assume UNIX path separator */` |
|        2 | 14830 | `	dir_sep = ':';` |
|        - | 14831 | `#endif` |
|        1 | 14832 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14833 | `	SXUNUSED(apArg);` |
|        - | 14834 | `	/* Point to the list of import paths */` |
|        3 | 14835 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14836 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14837 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14838 | `		if( n > 0 ){` |
|        - | 14839 | `			/* Append dir seprator */` |
|      ! 0 | 14840 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14841 | `		}` |
|        - | 14842 | `		/* Append path */` |
|        3 | 14843 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14844 | `	}` |
|        3 | 14845 | `	return PH7_OK;` |
|        1 | 14846 |  |
|        - | 14847 | `/*` |
|        - | 14848 | ` * string get_get_included_files(void)` |
|        - | 14849 | ` *  Gets the current include_path configuration option.` |
|        - | 14850 | ` * Parameter` |
|        - | 14851 | ` *  None` |
|        - | 14852 | ` * Return` |
|        - | 14853 | ` *  Included paths as a string` |
|        - | 14854 | ` */` |
|        2 | 14855 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14856 |  |
|        3 | 14857 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14858 | `	ph7_value *pArray,*pWorker;` |
|        - | 14859 | `	SyString *pEntry;` |
|        - | 14860 | `	int c,d;` |
|        - | 14861 | `	/* Create an array and a working value */` |
|        3 | 14862 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14863 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14864 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14865 | `		/* Out of memory,return null */` |
|      ! 0 | 14866 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14867 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14868 | `		SXUNUSED(apArg);` |
|      ! 0 | 14869 | `		return PH7_OK;` |
|        - | 14870 | `	}` |
|        3 | 14871 | `	c = d = '/';` |
|        - | 14872 | `#ifdef __WINNT__` |
|        1 | 14873 | `	d = '\\';` |
|        - | 14874 | `#endif` |
|        - | 14875 | `	/* Iterate throw entries */` |
|        3 | 14876 | `	SySetResetCursor(pFiles);` |
|     3917 | 14877 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14878 | `		const char *zBase,*zEnd;` |
|        - | 14879 | `		int iLen;` |
|        - | 14880 | `		/* reset the string cursor */` |
|     3915 | 14881 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14882 | `		/* Extract base name */` |
|     3915 | 14883 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14884 | `		/* Ignore trailing '/' */` |
|     5872 | 14885 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14886 | `			zEnd--;` |
|      ! 0 | 14887 | `		}` |
|     3915 | 14888 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 14889 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 14890 | `			zEnd--;` |
|        1 | 14891 | `		}` |
|     3915 | 14892 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 14893 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14894 | `		/* Copy entry name */` |
|     3915 | 14895 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14896 | `		/* Perform the insertion */` |
|     3915 | 14897 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14898 | `	}` |
|        - | 14899 | `	/* All done,return the created array */` |
|        3 | 14900 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14901 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14902 | `	 * by the engine as soon we return from this foreign` |
|        - | 14903 | `	 * function.` |
|        - | 14904 | `	 */` |
|        3 | 14905 | `	return PH7_OK;` |
|        2 | 14906 |  |
|        - | 14907 | `/*` |
|        - | 14908 | ` * include:` |
|        - | 14909 | ` * According to the PHP reference manual.` |
|        - | 14910 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14911 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14912 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14913 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14914 | ` *  and the current working directory before failing. The include()` |
|        - | 14915 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14916 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14917 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14918 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14919 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14920 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14921 | ` *  directory to find the requested file.` |
|        - | 14922 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14923 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14924 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14925 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14926 | ` */` |
|     9538 | 14927 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14928 |  |
|        - | 14929 | `	SyString sFile;` |
|        - | 14930 | `	sxi32 rc;` |
|     9540 | 14931 | `	if( nArg < 1 ){` |
|        - | 14932 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14933 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14934 | `		return SXRET_OK;` |
|        - | 14935 | `	}` |
|        - | 14936 | `	/* File to include */` |
|     9540 | 14937 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9540 | 14938 | `	if( sFile.nByte < 1 ){` |
|        - | 14939 | `		/* Empty string,return NULL */` |
|      ! 0 | 14940 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14941 | `		return SXRET_OK;` |
|        - | 14942 | `	}` |
|        - | 14943 | `	/* Open,compile and execute the desired script */` |
|     9540 | 14944 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9540 | 14945 | `	if( rc != SXRET_OK ){` |
|        - | 14946 | `		/* Emit a warning and return false */` |
|        3 | 14947 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14948 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14949 | `	}` |
|     9540 | 14950 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14951 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 14952 | `		return PH7_ABORT;` |
|        - | 14953 | `	}` |
|     9536 | 14954 | `	return SXRET_OK;` |
|     4771 | 14955 |  |
|        - | 14956 | `/*` |
|        - | 14957 | ` * include_once:` |
|        - | 14958 | ` *  According to the PHP reference manual.` |
|        - | 14959 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14960 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14961 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14962 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14963 | ` *   just once.` |
|        - | 14964 | ` */` |
|        4 | 14965 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14966 |  |
|        - | 14967 | `	SyString sFile;` |
|        - | 14968 | `	sxi32 rc;` |
|        5 | 14969 | `	if( nArg < 1 ){` |
|        - | 14970 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14971 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14972 | `		return SXRET_OK;` |
|        - | 14973 | `	}` |
|        - | 14974 | `	/* File to include */` |
|        5 | 14975 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14976 | `	if( sFile.nByte < 1 ){` |
|        - | 14977 | `		/* Empty string,return NULL */` |
|      ! 0 | 14978 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14979 | `		return SXRET_OK;` |
|        - | 14980 | `	}` |
|        - | 14981 | `	/* Open,compile and execute the desired script */` |
|        5 | 14982 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14983 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14984 | `		/* File already included,return TRUE */` |
|        3 | 14985 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14986 | `		return SXRET_OK;` |
|        - | 14987 | `	}` |
|        3 | 14988 | `	if( rc != SXRET_OK ){` |
|        - | 14989 | `		/* Emit a warning and return false */` |
|      ! 0 | 14990 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14991 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14992 | ` 	}` |
|        3 | 14993 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 14994 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 14995 | `		return PH7_ABORT;` |
|        - | 14996 | `	}` |
|        3 | 14997 | `	return SXRET_OK;` |
|        3 | 14998 |  |
|        - | 14999 | `/*` |
|        - | 15000 | ` * require.` |
|        - | 15001 | ` *  According to the PHP reference manual.` |
|        - | 15002 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15003 | ` *   also produce a fatal level error.` |
|        - | 15004 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15005 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15006 | ` */` |
|        6 | 15007 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15008 |  |
|        - | 15009 | `	SyString sFile;` |
|        - | 15010 | `	sxi32 rc;` |
|        8 | 15011 | `	if( nArg < 1 ){` |
|        - | 15012 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15013 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15014 | `		return SXRET_OK;` |
|        - | 15015 | `	}` |
|        - | 15016 | `	/* File to include */` |
|        8 | 15017 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15018 | `	if( sFile.nByte < 1 ){` |
|        - | 15019 | `		/* Empty string,return NULL */` |
|      ! 0 | 15020 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15021 | `		return SXRET_OK;` |
|        - | 15022 | `	}` |
|        - | 15023 | `	/* Open,compile and execute the desired script */` |
|        8 | 15024 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15025 | `	if( rc != SXRET_OK ){` |
|        - | 15026 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15027 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15028 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15029 | `		return PH7_ABORT;` |
|        - | 15030 | `	}` |
|        8 | 15031 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15032 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15033 | `		return PH7_ABORT;` |
|        - | 15034 | `	}` |
|        8 | 15035 | `	return SXRET_OK;` |
|        5 | 15036 |  |
|        - | 15037 | `/*` |
|        - | 15038 | ` * require_once:` |
|        - | 15039 | ` *  According to the PHP reference manual.` |
|        - | 15040 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15041 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15042 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15043 | ` *   and how it differs from its non _once siblings.` |
|        - | 15044 | ` */` |
|        4 | 15045 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15046 |  |
|        - | 15047 | `	SyString sFile;` |
|        - | 15048 | `	sxi32 rc;` |
|        5 | 15049 | `	if( nArg < 1 ){` |
|        - | 15050 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15051 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15052 | `		return SXRET_OK;` |
|        - | 15053 | `	}` |
|        - | 15054 | `	/* File to include */` |
|        5 | 15055 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15056 | `	if( sFile.nByte < 1 ){` |
|        - | 15057 | `		/* Empty string,return NULL */` |
|      ! 0 | 15058 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15059 | `		return SXRET_OK;` |
|        - | 15060 | `	}` |
|        - | 15061 | `	/* Open,compile and execute the desired script */` |
|        5 | 15062 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15063 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15064 | `		/* File already included,return TRUE */` |
|        3 | 15065 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15066 | `		return SXRET_OK;` |
|        - | 15067 | `	}` |
|        3 | 15068 | `	if( rc != SXRET_OK ){` |
|        - | 15069 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15070 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15071 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15072 | `		return PH7_ABORT;` |
|        - | 15073 | `	}` |
|        3 | 15074 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15075 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15076 | `		return PH7_ABORT;` |
|        - | 15077 | `	}` |
|        3 | 15078 | `	return SXRET_OK;` |
|        3 | 15079 |  |
|        - | 15080 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15081 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15082 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15083 | `/*` |
|        - | 15084 | ` * Section:` |
|        - | 15085 | ` *  SPL Autoloading functions.` |
|        - | 15086 | ` * Status:` |
|        - | 15087 | ` *  Stable.` |
|        - | 15088 | ` */` |
|        - | 15089 | `/*` |
|        - | 15090 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15091 | ` *  Register given function as __autoload() implementation.` |
|        - | 15092 | ` * Parameters` |
|        - | 15093 | ` *  callback` |
|        - | 15094 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15095 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15096 | ` *  throw` |
|        - | 15097 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15098 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15099 | ` *  prepend` |
|        - | 15100 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15101 | ` *   autoload stack instead of appending it.` |
|        - | 15102 | ` * Return` |
|        - | 15103 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15104 | ` */` |
|       34 | 15105 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15106 |  |
|        - | 15107 | `	VmAutoloadCB sEntry;` |
|       36 | 15108 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15109 | `	int iPrepend = 0;` |
|        - | 15110 | `	sxu32 n;` |
|       36 | 15111 | `	if( nArg < 1 ){` |
|        - | 15112 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15113 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15114 | `		/* Check for duplicates first */` |
|        9 | 15115 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15116 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15117 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15118 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15119 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15120 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15121 | `				return SXRET_OK;` |
|        - | 15122 | `			}` |
|      ! 0 | 15123 | `		}` |
|        5 | 15124 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15125 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15126 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15127 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15128 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15129 | `		return SXRET_OK;` |
|        - | 15130 | `	}` |
|        - | 15131 | `	/* Validate that the callback is callable */` |
|       28 | 15132 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15133 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15134 | `		if( nArg >= 2 ){` |
|      ! 0 | 15135 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15136 | `		}` |
|      ! 0 | 15137 | `		if( iThrow ){` |
|      ! 0 | 15138 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15139 | `				"Argument is not callable");` |
|      ! 0 | 15140 | `		}` |
|      ! 0 | 15141 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15142 | `		return SXRET_OK;` |
|        - | 15143 | `	}` |
|        - | 15144 | `	/* Check for duplicates */` |
|       46 | 15145 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15146 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15147 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15148 | `			/* Already registered */` |
|      ! 0 | 15149 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15150 | `			return SXRET_OK;` |
|        - | 15151 | `		}` |
|       11 | 15152 | `	}` |
|        - | 15153 | `	/* Check prepend flag */` |
|       28 | 15154 | `	if( nArg >= 3 ){` |
|        3 | 15155 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15156 | `	}` |
|        - | 15157 | `	/* Store the callback */` |
|       28 | 15158 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15159 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15160 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15161 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15162 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15163 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15164 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15165 | `		VmAutoloadCB *aBase;` |
|        3 | 15166 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15167 | `		/* Rotate: move last entry to front */` |
|        3 | 15168 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15169 | `		if( aBase ){` |
|        - | 15170 | `			VmAutoloadCB sTemp;` |
|        - | 15171 | `			sxu32 i;` |
|        3 | 15172 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15173 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15174 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15175 | `			}` |
|        3 | 15176 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15177 | `		}` |
|        2 | 15178 | `	}else{` |
|       26 | 15179 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15180 | `	}` |
|       28 | 15181 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15182 | `	return SXRET_OK;` |
|       19 | 15183 |  |
|        - | 15184 | `/*` |
|        - | 15185 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15186 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15187 | ` * Parameters` |
|        - | 15188 | ` *  callback` |
|        - | 15189 | ` *   The autoload function being unregistered.` |
|        - | 15190 | ` * Return` |
|        - | 15191 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15192 | ` */` |
|       32 | 15193 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15194 |  |
|       34 | 15195 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15196 | `	sxu32 n,nEntry;` |
|       34 | 15197 | `	if( nArg < 1 ){` |
|      ! 0 | 15198 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15199 | `		return SXRET_OK;` |
|        - | 15200 | `	}` |
|       34 | 15201 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15202 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15203 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15204 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15205 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15206 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15207 | `			sxu32 i;` |
|       32 | 15208 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15209 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15210 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15211 | `			}` |
|        - | 15212 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15213 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15214 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15215 | `			return SXRET_OK;` |
|        - | 15216 | `		}` |
|        3 | 15217 | `	}` |
|        3 | 15218 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15219 | `	return SXRET_OK;` |
|       18 | 15220 |  |
|        - | 15221 | `/*` |
|        - | 15222 | ` * array spl_autoload_functions(void)` |
|        - | 15223 | ` *  Return all registered __autoload() functions.` |
|        - | 15224 | ` * Return` |
|        - | 15225 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15226 | ` *  an empty array is returned.` |
|        - | 15227 | ` */` |
|       20 | 15228 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15229 |  |
|       21 | 15230 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15231 | `	ph7_value *pArray;` |
|        - | 15232 | `	sxu32 n,nEntry;` |
|       10 | 15233 | `	SXUNUSED(nArg);` |
|       10 | 15234 | `	SXUNUSED(apArg);` |
|       21 | 15235 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15236 | `	if( pArray == 0 ){` |
|      ! 0 | 15237 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15238 | `		return SXRET_OK;` |
|        - | 15239 | `	}` |
|       21 | 15240 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15241 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15242 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15243 | `		if( pEntry ){` |
|       15 | 15244 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15245 | `		}` |
|        8 | 15246 | `	}` |
|       21 | 15247 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15248 | `	return SXRET_OK;` |
|       11 | 15249 |  |
|        - | 15250 | `/*` |
|        - | 15251 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15252 | ` *  Default implementation of __autoload().` |
|        - | 15253 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15254 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15255 | ` * Parameters` |
|        - | 15256 | ` *  class` |
|        - | 15257 | ` *   The class name being searched.` |
|        - | 15258 | ` *  file_extensions` |
|        - | 15259 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15260 | ` */` |
|        2 | 15261 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15262 |  |
|        - | 15263 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15264 | `	SyBlob sPath;` |
|        - | 15265 | `	int nClass;` |
|        - | 15266 | `	sxi32 rc;` |
|        3 | 15267 | `	if( nArg < 1 ){` |
|      ! 0 | 15268 | `		return SXRET_OK;` |
|        - | 15269 | `	}` |
|        3 | 15270 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15271 | `	if( nClass < 1 ){` |
|      ! 0 | 15272 | `		return SXRET_OK;` |
|        - | 15273 | `	}` |
|        - | 15274 | `	/* Default extensions */` |
|        3 | 15275 | `	zExt = ".php,.inc";` |
|        3 | 15276 | `	if( nArg >= 2 ){` |
|        - | 15277 | `		int nExt;` |
|      ! 0 | 15278 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15279 | `		if( nExt < 1 ){` |
|      ! 0 | 15280 | `			zExt = ".php,.inc";` |
|      ! 0 | 15281 | `		}` |
|      ! 0 | 15282 | `	}` |
|        3 | 15283 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15284 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15285 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15286 | `	zCur = zExt;` |
|        7 | 15287 | `	while( zCur < zEnd ){` |
|        - | 15288 | `		const char *zComma;` |
|        - | 15289 | `		SyString sFile;` |
|        - | 15290 | `		int i;` |
|        - | 15291 | `		/* Find next comma or end */` |
|        5 | 15292 | `		zComma = zCur;` |
|       21 | 15293 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15294 | `			zComma++;` |
|        1 | 15295 | `		}` |
|        - | 15296 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15297 | `		SyBlobReset(&sPath);` |
|       69 | 15298 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15299 | `			char c = zClass[i];` |
|       65 | 15300 | `			if( c == '\\' ){` |
|      ! 0 | 15301 | `				c = '/';` |
|       65 | 15302 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15303 | `				c = c + ('a' - 'A');` |
|        6 | 15304 | `			}` |
|       65 | 15305 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15306 | `		}` |
|        - | 15307 | `		/* Append extension */` |
|        5 | 15308 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15309 | `		/* Try to include the file */` |
|        5 | 15310 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15311 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15312 | `		if( rc == SXRET_OK ){` |
|        - | 15313 | `			/* File included successfully */` |
|      ! 0 | 15314 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15315 | `			return SXRET_OK;` |
|        - | 15316 | `		}` |
|        - | 15317 | `		/* Move past the comma */` |
|        5 | 15318 | `		zCur = zComma;` |
|        5 | 15319 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15320 | `			zCur++;` |
|        1 | 15321 | `		}` |
|        1 | 15322 | `	}` |
|        3 | 15323 | `	SyBlobRelease(&sPath);` |
|        3 | 15324 | `	return SXRET_OK;` |
|        2 | 15325 |  |
|        - | 15326 | `/* Table of built-in VM functions. */` |
|        - | 15327 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15328 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15329 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15330 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15331 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15332 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15333 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15334 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15335 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15336 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15337 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15338 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15339 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15340 | `	    /* Constants management */` |
|        - | 15341 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15342 | `	{ "define",   vm_builtin_define               },` |
|        - | 15343 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15344 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15345 | `	   /* Class/Object functions */` |
|        - | 15346 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15347 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15348 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15349 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15350 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15351 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15352 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15353 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15354 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15355 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15356 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15357 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15358 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15359 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15360 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15361 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15362 | `	   /* SPL Autoloading */` |
|        - | 15363 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15364 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15365 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15366 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15367 | `	   /* Random numbers/strings generators */` |
|        - | 15368 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15369 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15370 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15371 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15372 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15373 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15374 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15375 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15376 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15377 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15378 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15379 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15380 | `	   /* Language constructs functions */` |
|        - | 15381 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15382 | `	{ "print", vm_builtin_print                   },` |
|        - | 15383 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15384 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15385 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15386 | `	  /* Variable handling functions */` |
|        - | 15387 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15388 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15389 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15390 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15391 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15392 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15393 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15394 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15395 | `	  /* Ouput control functions */` |
|        - | 15396 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15397 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15398 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15399 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15400 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15401 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15402 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15403 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15404 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15405 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15406 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15407 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15408 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15409 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15410 | `	  /* Assertion functions */` |
|        - | 15411 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15412 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15413 | `	  /* Error reporting functions */` |
|        - | 15414 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15415 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15416 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15417 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15418 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15419 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15420 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15421 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15422 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15423 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15424 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15425 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15426 | `	  /* Release info */` |
|        - | 15427 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15428 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15429 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15430 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15431 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15432 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15433 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15434 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15435 | `	  /* hashmap */` |
|        - | 15436 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15437 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15438 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15439 | `	  /* URL related function */` |
|        - | 15440 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15441 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15442 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15443 | `	   /* XML processing functions */` |
|        - | 15444 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15445 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15446 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15447 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15448 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15449 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15450 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15451 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15452 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15453 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15454 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15455 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15456 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15457 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15458 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15459 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15460 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15461 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15462 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15463 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15464 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15465 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15466 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15467 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15468 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15469 | `	   /* Command line processing */` |
|        - | 15470 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15471 | `	   /* JSON encoding/decoding */` |
|        - | 15472 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15473 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15474 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15475 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15476 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15477 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15478 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15479 | `	   /* Files/URI inclusion facility */` |
|        - | 15480 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15481 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15482 | `	{ "include",      vm_builtin_include          },` |
|        - | 15483 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15484 | `	{ "require",      vm_builtin_require          },` |
|        - | 15485 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15486 | `};` |
|        - | 15487 | `/*` |
|        - | 15488 | ` * Register the built-in VM functions defined above.` |
|        - | 15489 | ` */` |
|     2820 | 15490 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15491 |  |
|        - | 15492 | `	sxi32 rc;` |
|        - | 15493 | `	sxu32 n;` |
|   380702 | 15494 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15495 | `		/* Note that these special functions have access` |
|        - | 15496 | `		 * to the underlying virtual machine as their` |
|        - | 15497 | `		 * private data.` |
|        - | 15498 | `		 */` |
|   377882 | 15499 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   377882 | 15500 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15501 | `			return rc;` |
|        - | 15502 | `		}` |
|   188942 | 15503 | `	}` |
|     2822 | 15504 | `	return SXRET_OK;` |
|     1412 | 15505 |  |
|        - | 15506 | `/*` |
|        - | 15507 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15508 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15509 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15510 | ` */` |
|   100480 | 15511 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15512 |  |
|   100482 | 15513 | `	if( !iLoadable ){` |
|    98426 | 15514 | `		return pClass;` |
|        - | 15515 | `	}` |
|     2062 | 15516 | `	while(pClass){` |
|     2058 | 15517 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2054 | 15518 | `			return pClass;` |
|        - | 15519 | `		}` |
|        5 | 15520 | `		pClass = pClass->pNextName;` |
|        1 | 15521 | `	}` |
|        5 | 15522 | `	return 0;` |
|    50242 | 15523 |  |
|        - | 15524 | `/*` |
|        - | 15525 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15526 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15527 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15528 | ` * registered in the VM's class table.` |
|        - | 15529 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15530 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15531 | ` */` |
|       38 | 15532 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15533 |  |
|        - | 15534 | `	VmAutoloadCB *pEntry;` |
|        - | 15535 | `	ph7_value sArg,sResult;` |
|        - | 15536 | `	SyHashEntry *pHashEntry;` |
|        - | 15537 | `	ph7_class *pClass;` |
|        - | 15538 | `	sxu32 n,nEntry;` |
|       40 | 15539 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15540 | `	if( nEntry < 1 ){` |
|       26 | 15541 | `		return 0;` |
|        - | 15542 | `	}` |
|        - | 15543 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15544 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15545 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15546 | `	}` |
|        - | 15547 | `	/* Mark this class as being autoloaded */` |
|       14 | 15548 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15549 | `	/* Prepare the class name argument */` |
|       14 | 15550 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15551 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15552 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15553 | `	pClass = 0;` |
|       28 | 15554 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15555 | `		ph7_value *apArg[1];` |
|       24 | 15556 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15557 | `		if( pEntry == 0 ){` |
|      ! 0 | 15558 | `			continue;` |
|        - | 15559 | `		}` |
|       24 | 15560 | `		apArg[0] = &sArg;` |
|       24 | 15561 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15562 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15563 | `			continue;` |
|        - | 15564 | `		}` |
|        - | 15565 | `		/* Check if the class is now available */` |
|       24 | 15566 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15567 | `		if( pHashEntry ){` |
|       10 | 15568 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15569 | `			if( pClass ){` |
|       10 | 15570 | `				break;` |
|        - | 15571 | `			}` |
|      ! 0 | 15572 | `		}` |
|        9 | 15573 | `	}` |
|       14 | 15574 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15575 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15576 | `	/* Remove reentrancy guard */` |
|       14 | 15577 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15578 | `	return pClass;` |
|       21 | 15579 |  |
|        - | 15580 | `/*` |
|        - | 15581 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15582 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15583 | ` */` |
|       18 | 15584 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15585 |  |
|       20 | 15586 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15587 |  |
|        - | 15588 | `/*` |
|        - | 15589 | ` * Check if the given name refer to an installed class.` |
|        - | 15590 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15591 | ` */` |
|   100492 | 15592 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15593 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15594 | `	const char *zName,  /* Name of the target class */` |
|        - | 15595 | `	sxu32 nByte,        /* zName length */` |
|        - | 15596 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15597 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15598 | `						 */` |
|        - | 15599 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15600 | `	)` |
|        2 | 15601 |  |
|        - | 15602 | `	SyHashEntry *pEntry;` |
|        - | 15603 | `	ph7_class *pClass;` |
|    50246 | 15604 | `	SXUNUSED(iNest);` |
|        - | 15605 | `	/* Exact class lookup.` |
|        - | 15606 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15607 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   100494 | 15608 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   100494 | 15609 | `	if( pEntry == 0 ){` |
|        - | 15610 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15611 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15612 | `	}` |
|   100474 | 15613 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   100474 | 15614 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    50248 | 15615 |  |
|        - | 15616 | `/*` |
|        - | 15617 | ` * Reference Table Implementation` |
|        - | 15618 | ` * Status: stable <chm@symisc.net>` |
|        - | 15619 | ` * Intro` |
|        - | 15620 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15621 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15622 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15623 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15624 | ` *  Refer to the official for more information on this powerful` |
|        - | 15625 | ` *  extension.` |
|        - | 15626 | ` */` |
|        - | 15627 | `/*` |
|        - | 15628 | ` * Allocate a new reference entry.` |
|        - | 15629 | ` */` |
|  3200850 | 15630 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15631 |  |
|        - | 15632 | `	VmRefObj *pRef;` |
|        - | 15633 | `	/* Allocate a new instance */` |
|  3200852 | 15634 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3200852 | 15635 | `	if( pRef == 0 ){` |
|      ! 0 | 15636 | `		return 0;` |
|        - | 15637 | `	}` |
|        - | 15638 | `	/* Zero the structure */` |
|  3200852 | 15639 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15640 | `	/* Initialize fields */` |
|  3200852 | 15641 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3200852 | 15642 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3200852 | 15643 | `	pRef->nIdx = nIdx;` |
|  3200852 | 15644 | `	return pRef;` |
|  1600427 | 15645 |  |
|        - | 15646 | `/*` |
|        - | 15647 | ` * Default hash function used by the reference table` |
|        - | 15648 | ` * for lookup/insertion operations.` |
|        - | 15649 | ` */` |
| 17533145 | 15650 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15651 |  |
|        - | 15652 | `	/* Calculate the hash based on the memory object index */` |
| 17533147 | 15653 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15654 |  |
|        - | 15655 | `/*` |
|        - | 15656 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15657 | ` * in the reference table.` |
|        - | 15658 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15659 | ` * otherwise.` |
|        - | 15660 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15661 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15662 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15663 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15664 | ` * Refer to the official for more information on this powerful` |
|        - | 15665 | ` * extension.` |
|        - | 15666 | ` */` |
|  9543214 | 15667 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15668 |  |
|        - | 15669 | `	VmRefObj *pRef;` |
|        - | 15670 | `	sxu32 nBucket;` |
|        - | 15671 | `	/* Point to the appropriate bucket */` |
|  9543216 | 15672 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15673 | `	/* Perform the lookup */` |
|  9543216 | 15674 | `	pRef = pVm->apRefObj[nBucket];` |
| 20989991 | 15675 | `	for(;;){` |
| 41962861 | 15676 | `		if( pRef == 0 ){` |
|  3305918 | 15677 | `			break;` |
|        - | 15678 | `		}` |
| 38656945 | 15679 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15680 | `			/* Entry found */` |
|  6237300 | 15681 | `			return pRef;` |
|        - | 15682 | `		}` |
|        - | 15683 | `		/* Point to the next entry */` |
| 32419647 | 15684 | `		pRef = pRef->pNextCollide;` |
|        2 | 15685 | `	}` |
|        - | 15686 | `	/* No such entry,return NULL */` |
|  3305918 | 15687 | `	return 0;` |
|  4771609 | 15688 |  |
|        - | 15689 | `/*` |
|        - | 15690 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15691 | ` *` |
|        - | 15692 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15693 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15694 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15695 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15696 | ` * Refer to the official for more information on this powerful` |
|        - | 15697 | ` * extension.` |
|        - | 15698 | ` */` |
|  3200850 | 15699 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15700 |  |
|        - | 15701 | `	sxu32 nBucket;` |
|  3200852 | 15702 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15703 | `		VmRefObj **apNew;` |
|        - | 15704 | `		sxu32 nNew;` |
|        - | 15705 | `		/* Allocate a larger table */` |
|     4472 | 15706 | `		nNew = pVm->nRefSize << 1;` |
|     4472 | 15707 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4472 | 15708 | `		if( apNew ){` |
|     4472 | 15709 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15710 | `			sxu32 n;` |
|        - | 15711 | `			/* Zero the structure */` |
|     4472 | 15712 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15713 | `			/* Rehash all referenced entries */` |
|  2847974 | 15714 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15715 | `				/* Remove old collision links */` |
|  2843504 | 15716 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15717 | `				/* Point to the appropriate bucket */` |
|  2843504 | 15718 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15719 | `				/* Insert the entry  */` |
|  2843504 | 15720 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843504 | 15721 | `				if( apNew[nBucket] ){` |
|  2301116 | 15722 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15723 | `				}` |
|  2843504 | 15724 | `				apNew[nBucket] = pEntry;` |
|        - | 15725 | `				/* Point to the next entry */` |
|  2843504 | 15726 | `				pEntry = pEntry->pNext;` |
|  1421753 | 15727 | `			}` |
|        - | 15728 | `			/* Release the old table */` |
|     4472 | 15729 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15730 | `			/* Install the new one */` |
|     4472 | 15731 | `			pVm->apRefObj = apNew;` |
|     4472 | 15732 | `			pVm->nRefSize = nNew;` |
|     2235 | 15733 | `		}` |
|     2235 | 15734 | `	}` |
|        - | 15735 | `	/* Point to the appropriate bucket */` |
|  3200852 | 15736 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15737 | `	/* Insert the entry */` |
|  3200852 | 15738 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3200852 | 15739 | `	if( pVm->apRefObj[nBucket] ){` |
|  2614283 | 15740 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307149 | 15741 | `	}` |
|  3200852 | 15742 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3200852 | 15743 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3200852 | 15744 | `	pVm->nRefUsed++;` |
|  3200852 | 15745 | `	return SXRET_OK;` |
|        2 | 15746 |  |
|        - | 15747 | `/*` |
|        - | 15748 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15749 | ` * the reference table.` |
|        - | 15750 | ` * This function is invoked when the user perform an unset` |
|        - | 15751 | ` * call [i.e: unset($var); ].` |
|        - | 15752 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15753 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15754 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15755 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15756 | ` * Refer to the official for more information on this powerful` |
|        - | 15757 | ` * extension.` |
|        - | 15758 | ` */` |
|  3159706 | 15759 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15760 |  |
|        - | 15761 | `	ph7_hashmap_node **apNode;` |
|        - | 15762 | `	SyHashEntry **apEntry;` |
|        - | 15763 | `	sxu32 n;` |
|        - | 15764 | `	/* Point to the reference table */` |
|  3159708 | 15765 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3159708 | 15766 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15767 | `	/* Unlink the entry from the reference table */` |
|  3270638 | 15768 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   110932 | 15769 | `		if( apEntry[n] ){` |
|   110882 | 15770 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55440 | 15771 | `		}` |
|    55467 | 15772 | `	}` |
|  6208620 | 15773 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3048914 | 15774 | `		if( apNode[n] ){` |
|     6812 | 15775 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3405 | 15776 | `		}` |
|  1524458 | 15777 | `	}` |
|  3159708 | 15778 | `	if( pRef->pPrevCollide ){` |
|  1214129 | 15779 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   607705 | 15780 | `	}else{` |
|  1945581 | 15781 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15782 | `	}` |
|  3159708 | 15783 | `	if( pRef->pNextCollide ){` |
|  1801362 | 15784 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   900677 | 15785 | `	}` |
|  3159708 | 15786 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15787 | `	/* Release the node */` |
|  3159708 | 15788 | `	SySetRelease(&pRef->aReference);` |
|  3159708 | 15789 | `	SySetRelease(&pRef->aArrEntries);` |
|  3159708 | 15790 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3159708 | 15791 | `	pVm->nRefUsed--;` |
|  3159708 | 15792 | `	return SXRET_OK;` |
|        2 | 15793 |  |
|        - | 15794 | `/*` |
|        - | 15795 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15796 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15797 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15798 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15799 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15800 | ` * Refer to the official for more information on this powerful` |
|        - | 15801 | ` * extension.` |
|        - | 15802 | ` */` |
|  3236262 | 15803 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15804 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15805 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15806 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15807 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15808 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15809 | `	)` |
|        2 | 15810 |  |
|  3236264 | 15811 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15812 | `	VmRefObj *pRef;` |
|        - | 15813 | `	/* Check if the referenced object already exists */` |
|  3236264 | 15814 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3236264 | 15815 | `	if( pRef == 0 ){` |
|        - | 15816 | `		/* Create a new entry */` |
|  3200852 | 15817 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3200852 | 15818 | `		if( pRef == 0 ){` |
|      ! 0 | 15819 | `			return SXERR_MEM;` |
|        - | 15820 | `		}` |
|  3200852 | 15821 | `		pRef->iFlags = iFlags;` |
|        - | 15822 | `		/* Install the entry */` |
|  3200852 | 15823 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1600425 | 15824 | `	}` |
|  3236264 | 15825 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3236264 | 15826 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15827 | `		VmSlot sRef;` |
|        - | 15828 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15829 | `		 * be deleted when we leave this frame.` |
|        - | 15830 | `		 */` |
|   105176 | 15831 | `		sRef.nIdx = nIdx;` |
|   105176 | 15832 | `		sRef.pUserData = pEntry;` |
|   105176 | 15833 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15834 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15835 | `		}` |
|    52587 | 15836 | `	}` |
|  3236264 | 15837 | `	if( pEntry ){` |
|        - | 15838 | `		/* Address of the hash-entry */` |
|   140364 | 15839 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70181 | 15840 | `	}` |
|  3236264 | 15841 | `	if( pMapEntry ){` |
|        - | 15842 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3087656 | 15843 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1543827 | 15844 | `	}` |
|  3236264 | 15845 | `	return SXRET_OK;` |
|  1618133 | 15846 |  |
|        - | 15847 | `/*` |
|        - | 15848 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15849 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15850 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15851 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15852 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15853 | ` * Refer to the official for more information on this powerful` |
|        - | 15854 | ` * extension.` |
|        - | 15855 | ` */` |
|  3147240 | 15856 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15857 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15858 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15859 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15860 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15861 | `	)` |
|        2 | 15862 |  |
|        - | 15863 | `	VmRefObj *pRef;` |
|        - | 15864 | `	sxu32 n;` |
|        - | 15865 | `	/* Check if the referenced object already exists */` |
|  3147242 | 15866 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3147242 | 15867 | `	if( pRef == 0 ){` |
|        - | 15868 | `		/* Not such entry */` |
|   105062 | 15869 | `		return SXERR_NOTFOUND;` |
|        - | 15870 | `	}` |
|        - | 15871 | `	/* Remove the desired entry */` |
|  3042182 | 15872 | `	if( pEntry ){` |
|        - | 15873 | `		SyHashEntry **apEntry;` |
|       74 | 15874 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 15875 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 15876 | `			if( apEntry[n] == pEntry ){` |
|        - | 15877 | `				/* Nullify the entry */` |
|       74 | 15878 | `				apEntry[n] = 0;` |
|        - | 15879 | `				/*` |
|        - | 15880 | `				 * NOTE:` |
|        - | 15881 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15882 | `				 * we avoid wasting spaces.` |
|        - | 15883 | `				 */` |
|       36 | 15884 | `			}` |
|       97 | 15885 | `		}` |
|       36 | 15886 | `	}` |
|  3042182 | 15887 | `	if( pMapEntry ){` |
|        - | 15888 | `		ph7_hashmap_node **apNode;` |
|  3042110 | 15889 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6084312 | 15890 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3042204 | 15891 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15892 | `				/* nullify the entry */` |
|  3042110 | 15893 | `				apNode[n] = 0;` |
|  1521054 | 15894 | `			}` |
|  1521103 | 15895 | `		}` |
|  1521054 | 15896 | `	}` |
|  3042182 | 15897 | `	return SXRET_OK;` |
|  1573622 | 15898 |  |
|        - | 15899 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15900 | `/*` |
|        - | 15901 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15902 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15903 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15904 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15905 | ` * For more information on how to register IO stream devices,please` |
|        - | 15906 | ` * refer to the official documentation.` |
|        - | 15907 | ` */` |
|    29048 | 15908 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15909 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15910 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15911 | `	int nByte              /* *pzDevice length*/` |
|        - | 15912 | `	)` |
|        2 | 15913 |  |
|        - | 15914 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15915 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15916 | `	SyString sDev,sCur;` |
|        - | 15917 | `	sxu32 n,nEntry;` |
|        - | 15918 | `	int rc;` |
|        - | 15919 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29050 | 15920 | `	zNext = zCur = zIn = *pzDevice;` |
|    29050 | 15921 | `	zEnd = &zIn[nByte];` |
|  1855798 | 15922 | `	while( zIn < zEnd ){` |
|  1826752 | 15923 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15924 | `			/* Got one */` |
|        3 | 15925 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15926 | `			break;` |
|        - | 15927 | `		}` |
|        - | 15928 | `		/* Advance the cursor */` |
|  1826750 | 15929 | `		zIn++;` |
|        2 | 15930 | `	}` |
|    29050 | 15931 | `	if( zIn >= zEnd ){` |
|        - | 15932 | `		/* No such scheme,return the default stream */` |
|    29048 | 15933 | `		return pVm->pDefStream;` |
|        - | 15934 | `	}` |
|        3 | 15935 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15936 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15937 | `	SyStringFullTrim(&sDev);` |
|        - | 15938 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15939 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15940 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15941 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15942 | `		pStream = apStream[n];` |
|        3 | 15943 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15944 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15945 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15946 | `		if( rc == 0 ){` |
|        - | 15947 | `			/* Stream device found */` |
|        3 | 15948 | `			*pzDevice = zNext;` |
|        3 | 15949 | `			return pStream;` |
|        - | 15950 | `		}` |
|      ! 0 | 15951 | `	}` |
|        - | 15952 | `	/* No such stream,return NULL */` |
|      ! 0 | 15953 | `	return 0;` |
|    14526 | 15954 |  |
|        - | 15955 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15956 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15957 |  |
