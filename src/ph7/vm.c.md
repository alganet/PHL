# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7009/8943 lines (78.37%)

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
|   927708 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   141 |  |
|   927713 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   927679 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   927669 |   148 | `	return FALSE;` |
|   463881 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   337678 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   337683 |   162 | `	sxu8 bReal = FALSE;` |
|   337683 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   337683 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   337613 |   166 | `		return FALSE;` |
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
|   168866 |   183 |  |
|        - |   184 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   185 | `/* Constant expander used by define(); used below to recognise user-defined` |
|        - |   186 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|        - |   187 | ` * a define() overwrites them. */` |
|        - |   188 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData);` |
|        - |   189 | `/*` |
|        - |   190 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   191 | ` * it can be expanded from the target PHP program.` |
|        - |   192 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   193 | ` * simple and work as follows:` |
|        - |   194 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   195 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   196 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   197 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   198 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   199 | ` * (Windows,Linux,...) and so on.` |
|        - |   200 | ` * Please refer to the official documentation for additional information.` |
|        - |   201 | ` */` |
|   637514 |   202 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   203 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   204 | `	const SyString *pName,  /* Constant name */` |
|        - |   205 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   206 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   207 | `	)` |
|        5 |   208 |  |
|        - |   209 | `	ph7_constant *pCons;` |
|        - |   210 | `	SyHashEntry *pEntry;` |
|        - |   211 | `	char *zDupName;` |
|        - |   212 | `	sxi32 rc;` |
|   637519 |   213 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   637519 |   214 | `	if( pEntry ){` |
|        - |   215 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   216 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |   217 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|        - |   218 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|        - |   219 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|        4 |   220 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|        4 |   221 | `		 && pCons->pUserData != pUserData ){` |
|        3 |   222 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|        3 |   223 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|        1 |   224 | `		}` |
|        6 |   225 | `		pCons->xExpand = xExpand;` |
|        6 |   226 | `		pCons->pUserData = pUserData;` |
|        6 |   227 | `		return SXRET_OK;` |
|        - |   228 | `	}` |
|        - |   229 | `	/* Allocate a new constant instance */` |
|   637515 |   230 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   637515 |   231 | `	if( pCons == 0 ){` |
|      ! 0 |   232 | `		return 0;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate constant name */` |
|   637515 |   235 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   637515 |   236 | `	if( zDupName == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return 0;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Install the constant */` |
|   637515 |   241 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   637515 |   242 | `	pCons->xExpand = xExpand;` |
|   637515 |   243 | `	pCons->pUserData = pUserData;` |
|   637515 |   244 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   637515 |   245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   246 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   247 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   248 | `		return rc;` |
|        - |   249 | `	}` |
|        - |   250 | `	/* All done,constant can be invoked from PHP code */` |
|   637515 |   251 | `	return SXRET_OK;` |
|   318762 |   252 |  |
|        - |   253 | `/*` |
|        - |   254 | ` * Allocate a new foreign function instance.` |
|        - |   255 | ` * This function return SXRET_OK on success. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|  1409120 |   260 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   261 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   262 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   263 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   264 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   265 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   266 | `	)` |
|        5 |   267 |  |
|        - |   268 | `	ph7_user_func *pFunc;` |
|        - |   269 | `	char *zDup;` |
|        - |   270 | `	/* Allocate a new user function */` |
|  1409125 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1409125 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1409125 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1409125 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1409125 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1409125 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1409125 |   285 | `	pFunc->pVm   = pVm;` |
|  1409125 |   286 | `	pFunc->xFunc = xFunc;` |
|  1409125 |   287 | `	pFunc->pUserData = pUserData;` |
|  1409125 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1409125 |   290 | `	*ppOut = pFunc;` |
|  1409125 |   291 | `	return SXRET_OK;` |
|   704565 |   292 |  |
|        - |   293 | `/*` |
|        - |   294 | ` * Install a foreign function and it's associated callback so that` |
|        - |   295 | ` * it can be invoked from the target PHP code.` |
|        - |   296 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   297 | ` * return value indicates failure.` |
|        - |   298 | ` * Please refer to the official documentation for an introduction to` |
|        - |   299 | ` * the foreign function mechanism.` |
|        - |   300 | ` */` |
|  1411966 |   301 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   302 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   303 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   304 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   305 | `	void *pUserData           /* Foreign function private data */` |
|        - |   306 | `	)` |
|        5 |   307 |  |
|        - |   308 | `	ph7_user_func *pFunc;` |
|        - |   309 | `	SyHashEntry *pEntry;` |
|        - |   310 | `	sxi32 rc;` |
|        - |   311 | `	/* Overwrite any previously registered function with the same name */` |
|  1411971 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1411971 |   313 | `	if( pEntry ){` |
|     2851 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2851 |   315 | `		pFunc->pUserData = pUserData;` |
|     2851 |   316 | `		pFunc->xFunc = xFunc;` |
|     2851 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2851 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1409125 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1409125 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1409125 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1409125 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1409125 |   333 | `	return SXRET_OK;` |
|   705988 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   280488 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        5 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   280493 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   280493 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   280493 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   280493 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   280493 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   280493 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   280493 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   280493 |   362 | `	pFunc->iFlags = iFlags;` |
|   280493 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   280493 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   280493 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   280493 |   368 | `	return SXRET_OK;` |
|        5 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1467546 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        5 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1467551 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    42315 |   388 | `		pName = &pFunc->sName;` |
|    21155 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1467551 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1467551 |   392 | `	if( pEntry ){` |
|  1269613 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1269613 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1269613 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   197943 |   402 | `	pFunc->pNextName = 0;` |
|   197943 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   197943 |   404 | `	return rc;` |
|   733778 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   121290 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        5 |   413 |  |
|   121295 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   121295 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   121295 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   121265 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   121265 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   121265 |   429 | `	return rc;` |
|    60650 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4293978 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   435 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   436 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   437 | `	sxi32 iP1,    /* First operand */` |
|        - |   438 | `	sxu32 iP2,    /* Second operand */` |
|        - |   439 | `	void *p3,     /* Third operand */` |
|        - |   440 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   441 | `	)` |
|        5 |   442 |  |
|        - |   443 | `	VmInstr sInstr;` |
|        - |   444 | `	sxi32 rc;` |
|        - |   445 | `	/* Fill the VM instruction */` |
|  4293983 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4293983 |   447 | `	sInstr.iP1 = iP1;` |
|  4293983 |   448 | `	sInstr.iP2 = iP2;` |
|  4293983 |   449 | `	sInstr.p3  = p3;` |
|  4293983 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   233243 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   116619 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4293983 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4293983 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4293983 |   460 | `	return rc;` |
|        5 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   557336 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |   466 |  |
|   557341 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   557341 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   557341 |   474 | `	return SXRET_OK;` |
|        5 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   278668 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |   480 |  |
|   278673 |   481 | `	return pVm->pByteContainer;` |
|        5 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   229998 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   230003 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   230003 |   490 | `	return pInstr;` |
|        5 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1289796 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |   496 |  |
|  1289801 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   212690 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |   503 |  |
|   212695 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   845626 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |   510 |  |
|   845631 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |   512 |  |
|    33860 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    33865 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33865 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    33865 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33865 |   522 | `	return &aInstr[n - 2];` |
|    16935 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    23528 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        5 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    23533 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    23533 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    23533 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    23533 |   542 | `	pFrame->pUserData = pUserData;` |
|    23533 |   543 | `	pFrame->pThis = pThis;` |
|    23533 |   544 | `	pFrame->pVm = pVm;` |
|    23533 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    23533 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    23533 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    23533 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    23533 |   549 | `	return pFrame;` |
|    11769 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    23456 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        5 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    23461 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    23461 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    23461 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    23461 |   571 | `	pVm->pFrame = pFrame;` |
|    23461 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    20291 |   574 | `		*ppFrame = pFrame;` |
|    10143 |   575 | `	}` |
|    23461 |   576 | `	return SXRET_OK;` |
|    11733 |   577 |  |
|        - |   578 | `/*` |
|        - |   579 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   580 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   581 | ` * information.` |
|        - |   582 | ` */` |
|       70 |   583 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        5 |   584 |  |
|        - |   585 | `	VmFrame *pTarget,*pFrame;` |
|       75 |   586 | `	SyHashEntry *pEntry = 0;` |
|        - |   587 | `	sxi32 rc;` |
|        - |   588 | `	/* Point to the upper frame */` |
|       75 |   589 | `	pFrame = pVm->pFrame;` |
|       75 |   590 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       75 |   591 | `	pTarget = pFrame;` |
|       75 |   592 | `	pFrame = pTarget->pParent;` |
|       75 |   593 | `	while( pFrame ){` |
|       75 |   594 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   595 | `			/* Query the current frame */` |
|       75 |   596 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       75 |   597 | `			if( pEntry ){` |
|        - |   598 | `				/* Variable found */` |
|       75 |   599 | `				break;` |
|        - |   600 | `			}` |
|      ! 0 |   601 | `		}` |
|        - |   602 | `		/* Point to the upper frame */` |
|      ! 0 |   603 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   604 | `	}` |
|       75 |   605 | `	if( pEntry == 0 ){` |
|        - |   606 | `		/* Inexistant variable */` |
|      ! 0 |   607 | `		return SXERR_NOTFOUND;` |
|        - |   608 | `	}` |
|        - |   609 | `	/* Link to the current frame */` |
|       75 |   610 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       75 |   611 | `	if( rc == SXRET_OK ){` |
|        - |   612 | `		sxu32 nIdx;` |
|       75 |   613 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       75 |   614 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       35 |   615 | `	}` |
|       75 |   616 | `	return rc;` |
|       40 |   617 |  |
|        - |   618 | `/*` |
|        - |   619 | ` * Leave the top-most active frame.` |
|        - |   620 | ` */` |
|    20282 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |   622 |  |
|    20287 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    20287 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    20287 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    20287 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    19435 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   127369 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   107939 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    53972 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    19435 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   127443 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   108013 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    54009 |   640 | `			}` |
|     9715 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    20287 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    20287 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    20287 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    20287 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    20287 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    10141 |   649 | `	}` |
|    20287 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7184026 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |   657 |  |
|  7188733 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     4707 |   659 | `		pFrame = pFrame->pParent;` |
|        5 |   660 | `	}` |
|  7184031 |   661 | `	return pFrame;` |
|        5 |   662 |  |
|        - |   663 | `/*` |
|        - |   664 | ` * After a callee invoked from an OP_CALL site (object __invoke, an array` |
|        - |   665 | ` * callable, or a NEW constructor) returns PH7_EXCEPTION, decide how the current` |
|        - |   666 | ` * frame unwinds. The catch body, if any, already ran in-place inside` |
|        - |   667 | ` * VmThrowException. Returns TRUE and sets *pResumePc to the post-try resume` |
|        - |   668 | ` * point when THIS frame's own try caught the exception; returns FALSE to signal` |
|        - |   669 | ` * the caller to propagate (goto Exception) so the exception unwinds through` |
|        - |   670 | ` * intermediate frames that have no local handler.` |
|        - |   671 | ` */` |
|       12 |   672 | `static int VmCalleeExceptionResume(ph7_vm *pVm,sxi32 *pResumePc)` |
|        1 |   673 |  |
|       13 |   674 | `	VmFrame *pFrame = pVm->pFrame;` |
|       12 |   675 | `	if( (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0` |
|       11 |   676 | `	 && !(pFrame->iFlags & VM_FRAME_THROW) ){` |
|       11 |   677 | `		*pResumePc = (sxi32)pFrame->iExceptionJump - 1;` |
|       11 |   678 | `		return TRUE;` |
|        - |   679 | `	}` |
|        3 |   680 | `	return FALSE;` |
|        7 |   681 |  |
|        - |   682 | `/*` |
|        - |   683 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|        - |   684 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|        - |   685 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|        - |   686 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|        - |   687 | ` * nested try/finally inside a catch body). Each finally runs with` |
|        - |   688 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value via` |
|        - |   689 | ` * pVm->sCatchReturn. Returns SXERR_ABORT if a finally aborted, SXRET_OK otherwise.` |
|        - |   690 | ` */` |
|    48332 |   691 | `static sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |   692 |  |
|        - |   693 | `	sxu32 nUsed;` |
|    48347 |   694 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|       12 |   695 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       12 |   696 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|       12 |   697 | `		(void)SySetPop(&pVm->aException);` |
|       12 |   698 | `		pExc->pFrame = 0;` |
|       12 |   699 | `		VmLeaveFrame(&(*pVm));` |
|       12 |   700 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|       12 |   701 | `			pExc->iFinallyDone = 1;` |
|       12 |   702 | `			if( VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE) == SXERR_ABORT ){` |
|      ! 0 |   703 | `				return SXERR_ABORT;` |
|        - |   704 | `			}` |
|        5 |   705 | `		}` |
|        2 |   706 | `	}` |
|    48337 |   707 | `	return SXRET_OK;` |
|    24171 |   708 |  |
|        - |   709 | `/*` |
|        - |   710 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|        - |   711 | ` * deferred pVm->sCatchReturn into the enclosing function's result, clear the` |
|        - |   712 | ` * signal, and tear down any try frames left open above pEntryFrame whose` |
|        - |   713 | ` * OP_POP_EXCEPTION the return bypassed (bounded by pEntryFrame so a tangled` |
|        - |   714 | ` * exception-in-finally chain can't over-leave). Only the real function body` |
|        - |   715 | ` * (bReturnPropagates=FALSE) calls this; a nested mini-program leaves the signal` |
|        - |   716 | ` * set so it propagates outward to its own enclosing function.` |
|        - |   717 | ` */` |
|       32 |   718 | `static void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|        2 |   719 |  |
|       34 |   720 | `	if( pResult ){` |
|       34 |   721 | `		PH7_MemObjStore(&pVm->sCatchReturn,pResult);` |
|       16 |   722 | `	}` |
|       34 |   723 | `	pVm->bReturnRequested = 0;` |
|       34 |   724 | `	PH7_MemObjRelease(&pVm->sCatchReturn);` |
|       36 |   725 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|        3 |   726 | `		VmLeaveFrame(&(*pVm));` |
|        1 |   727 | `	}` |
|       34 |   728 |  |
|        - |   729 | `/*` |
|        - |   730 | ` * Compare two functions signature and return the comparison result.` |
|        - |   731 | ` */` |
|      836 |   732 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   733 |  |
|      837 |   734 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   735 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   736 | `	const char *zSin = pSecond->zString;` |
|      837 |   737 | `	const char *zFin = pFirst->zString;` |
|      837 |   738 | `	const char *zPtr = zFin;` |
|      421 |   739 | `	for(;;){` |
|      843 |   740 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   741 | `			break;` |
|        - |   742 | `		}` |
|       19 |   743 | `		if( zFin[0] != zSin[0] ){` |
|        - |   744 | `			/* mismatch */` |
|       13 |   745 | `			break;` |
|        - |   746 | `		}` |
|        7 |   747 | `		zFin++;` |
|        7 |   748 | `		zSin++;` |
|        1 |   749 | `	}` |
|      837 |   750 | `	return (int)(zFin-zPtr);` |
|        1 |   751 |  |
|        - |   752 | `/*` |
|        - |   753 | ` * Select the appropriate VM function for the current call context.` |
|        - |   754 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   755 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   756 | ` * Refer to the official documentation for more information.` |
|        - |   757 | ` */` |
|      138 |   758 | `static ph7_vm_func * VmOverload(` |
|        - |   759 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   760 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   761 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   762 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   763 | `	)` |
|        2 |   764 |  |
|        - |   765 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   766 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   767 | `	ph7_vm_func *pLink;` |
|        - |   768 | `	SyString sArgSig;` |
|        - |   769 | `	SyBlob sSig;` |
|        - |   770 |  |
|      140 |   771 | `	pLink = pList;` |
|      140 |   772 | `	i = 0;` |
|        - |   773 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   774 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   775 | `		if( pLink == 0 ){` |
|       78 |   776 | `			break;` |
|        - |   777 | `		}` |
|      948 |   778 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   779 | `			/* Candidate for overloading */` |
|      902 |   780 | `			apSet[i++] = pLink;` |
|      450 |   781 | `		}` |
|        - |   782 | `		/* Point to the next entry */` |
|      948 |   783 | `		pLink = pLink->pNextName;` |
|        2 |   784 | `	}` |
|      140 |   785 | `	if( i < 1 ){` |
|        - |   786 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   787 | `		return pList;` |
|        - |   788 | `	}` |
|      140 |   789 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   790 | `		/* Return the only candidate */` |
|       32 |   791 | `		return apSet[0];` |
|        - |   792 | `	}` |
|        - |   793 | `	/* Calculate function signature */` |
|      109 |   794 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   795 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   796 | `		int c = 'n'; /* null */` |
|      259 |   797 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   798 | `			/* Hashmap */` |
|       45 |   799 | `			c = 'h';` |
|      237 |   800 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   801 | `			/* bool */` |
|      ! 0 |   802 | `			c = 'b';` |
|      215 |   803 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   804 | `			/* int */` |
|        7 |   805 | `			c = 'i';` |
|      212 |   806 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   807 | `			/* String */` |
|      107 |   808 | `			c = 's';` |
|      156 |   809 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   810 | `			/* Float */` |
|      ! 0 |   811 | `			c = 'f';` |
|      103 |   812 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   813 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   814 | `			int marker = 'o';` |
|        3 |   815 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   816 | `			SyString *pName = &pClass->sName;` |
|        3 |   817 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   818 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   819 | `			c = -1;` |
|        1 |   820 | `		}` |
|      259 |   821 | `		if( c > 0 ){` |
|      257 |   822 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   823 | `		}` |
|      130 |   824 | `	}` |
|      109 |   825 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   826 | `	iTarget = 0;` |
|      109 |   827 | `	iMax = -1;` |
|        - |   828 | `	/* Select the appropriate function */` |
|      945 |   829 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   830 | `		/* Compare the two signatures */` |
|      837 |   831 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   832 | `		if( iCur > iMax ){` |
|      113 |   833 | `			iMax = iCur;` |
|      113 |   834 | `			iTarget = j;` |
|       56 |   835 | `		}` |
|      419 |   836 | `	}` |
|      109 |   837 | `	SyBlobRelease(&sSig);` |
|        - |   838 | `	/* Appropriate function for the current call context */` |
|      109 |   839 | `	return apSet[iTarget];` |
|       71 |   840 |  |
|        - |   841 | `/* Forward declaration */` |
|        - |   842 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   843 | `static sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);` |
|        - |   844 | `/*` |
|        - |   845 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   846 | ` * it can be instanciated from the executed PHP script.` |
|        - |   847 | ` */` |
|        - |   848 | `/*` |
|        - |   849 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |   850 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |   851 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |   852 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |   853 | ` * properties register their enforcement slot. It is factored out of` |
|        - |   854 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |   855 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |   856 | ` */` |
|   360390 |   857 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   858 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   859 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   860 | `	)` |
|        5 |   861 |  |
|        - |   862 | `	ph7_class_attr *pAttr;` |
|        - |   863 | `	SyHashEntry *pEntry;` |
|        - |   864 | `	/* Reset the loop cursor */` |
|   360395 |   865 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   866 | `	/* Process only static and constant attribute */` |
|  1418432 |   867 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   868 | `		/* Extract the current attribute */` |
|   877851 |   869 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   877851 |   870 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   871 | `			ph7_value *pMemObj;` |
|        - |   872 | `			/* Reserve a memory object for this constant/static attribute */` |
|     3963 |   873 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3963 |   874 | `			if( pMemObj == 0 ){` |
|      ! 0 |   875 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   876 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   877 | `					&pClass->sName,&pAttr->sName` |
|        - |   878 | `					);` |
|      ! 0 |   879 | `				return SXERR_MEM;` |
|        - |   880 | `			}` |
|     3963 |   881 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   882 | `				/* Initialize attribute default value (any complex expression) */` |
|     3959 |   883 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);` |
|        - |   884 | `				/* Typed class constant (PHP 8.3): enforce the computed value` |
|        - |   885 | `				 * against the declared type. A mismatch is a non-catchable` |
|        - |   886 | `				 * fatal, raised here at definition time (matching PHP). */` |
|     5931 |   887 | `				if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|     1982 |   888 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|     1071 |   889 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj);` |
|     1071 |   890 | `					if( rcType != SXRET_OK ){` |
|        6 |   891 | `						return rcType;` |
|        - |   892 | `					}` |
|      532 |   893 | `				}` |
|     1975 |   894 | `			}` |
|        - |   895 | `			/* Record attribute index */` |
|     3958 |   896 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   897 | `			/* Install static attribute in the reference table */` |
|     3958 |   898 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   899 | `			/* If this is a typed static property, register the slot so the` |
|        - |   900 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   901 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   902 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|        - |   903 | `			 * Typed *constants* are excluded — they are immutable and were` |
|        - |   904 | `			 * already enforced above, so they need no store-time slot. */` |
|     3954 |   905 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     2520 |   906 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       17 |   907 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       17 |   908 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   909 | `					return SXERR_MEM;` |
|        - |   910 | `				}` |
|       17 |   911 | `				pVmAttrS->pAttr = pAttr;` |
|       17 |   912 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       17 |   913 | `				pVmAttrS->iState = 0;` |
|       17 |   914 | `				pVmAttrS->pOwner = pClass;` |
|        - |   915 | `				/* Static typed property with no default starts uninitialized` |
|        - |   916 | `				 * (constants are already excluded by the enclosing condition). */` |
|       17 |   917 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        6 |   918 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   919 | `				}` |
|       17 |   920 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   921 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   922 | `					return SXERR_MEM;` |
|        - |   923 | `				}` |
|        7 |   924 | `			}` |
|     1977 |   925 | `		}` |
|        5 |   926 | `	}` |
|   360391 |   927 | `	return SXRET_OK;` |
|   180200 |   928 |  |
|   360158 |   929 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   930 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   931 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   932 | `	)` |
|        5 |   933 |  |
|        - |   934 | `	ph7_class_method *pMeth;` |
|        - |   935 | `	SyHashEntry *pEntry;` |
|        - |   936 | `	sxi32 rc;` |
|        - |   937 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   360163 |   938 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   360163 |   939 | `	if( rc != SXRET_OK ){` |
|        6 |   940 | `		return rc;` |
|        - |   941 | `	}` |
|        - |   942 | `	/* Install class methods */` |
|   360159 |   943 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   944 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   945 | `		 */` |
|   197057 |   946 | `		return SXRET_OK;` |
|        - |   947 | `	}` |
|        - |   948 | `	/* Create constructor alias if not yet done */` |
|   163107 |   949 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   950 | `		/* User constructor with the same base class name */` |
|     6749 |   951 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6749 |   952 | `		if( pEntry ){` |
|      ! 0 |   953 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   954 | `			/* Create the alias */` |
|      ! 0 |   955 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   956 | `		}` |
|     3372 |   957 | `	}` |
|        - |   958 | `	/* Install the methods now */` |
|   163107 |   959 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1669902 |   960 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1425249 |   961 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1425249 |   962 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1425241 |   963 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1425241 |   964 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   965 | `				return rc;` |
|        - |   966 | `			}` |
|   712618 |   967 | `		}` |
|        5 |   968 | `	}` |
|        - |   969 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   163107 |   970 | `	pClass->bMounted = TRUE;` |
|   163107 |   971 | `	return SXRET_OK;` |
|   180084 |   972 |  |
|        - |   973 | `/*` |
|        - |   974 | ` * Allocate a private frame for attributes of the given` |
|        - |   975 | ` * class instance (Object in the PHP jargon).` |
|        - |   976 | ` */` |
|     2218 |   977 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   978 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   979 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   980 | `	)` |
|        5 |   981 |  |
|     2223 |   982 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   983 | `	ph7_class_attr *pAttr;` |
|        - |   984 | `	SyHashEntry *pEntry;` |
|        - |   985 | `	sxi32 rc;` |
|        - |   986 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2223 |   987 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     9289 |   988 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   989 | `		VmClassAttr *pVmAttr;` |
|        - |   990 | `		/* Extract the current attribute */` |
|     7071 |   991 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7071 |   992 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     7071 |   993 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   994 | `			return SXERR_MEM;` |
|        - |   995 | `		}` |
|     7071 |   996 | `		pVmAttr->pAttr = pAttr;` |
|     7071 |   997 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   998 | `			ph7_value *pMemObj;` |
|        - |   999 | `			/* Reserve a memory object for this attribute */` |
|     7045 |  1000 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     7045 |  1001 | `			if( pMemObj == 0 ){` |
|      ! 0 |  1002 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1003 | `				return SXERR_MEM;` |
|        - |  1004 | `			}` |
|     7045 |  1005 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     7045 |  1006 | `			pVmAttr->iState = 0;` |
|     7045 |  1007 | `			pVmAttr->pOwner = pClass;` |
|     7045 |  1008 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |  1009 | `				/* Initialize attribute default value (any complex expression) */` |
|     2423 |  1010 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);` |
|     5836 |  1011 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |  1012 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |  1013 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       76 |  1014 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |  1015 | `			}` |
|     7045 |  1016 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     7045 |  1017 | `			if( rc != SXRET_OK ){` |
|        - |  1018 | `				VmSlot sSlot;` |
|        - |  1019 | `				/* Restore memory object */` |
|      ! 0 |  1020 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |  1021 | `				sSlot.pUserData = 0;` |
|      ! 0 |  1022 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |  1023 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1024 | `				return SXERR_MEM;` |
|        - |  1025 | `			}` |
|        - |  1026 | `			/* Install attribute in the reference table */` |
|     7045 |  1027 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |  1028 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |  1029 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |  1030 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     7045 |  1031 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      185 |  1032 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      185 |  1033 | `				if( rc != SXRET_OK ){` |
|        - |  1034 | `					VmSlot sSlot;` |
|      ! 0 |  1035 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |  1036 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |  1037 | `					sSlot.pUserData = 0;` |
|      ! 0 |  1038 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |  1039 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1040 | `					return SXERR_MEM;` |
|        - |  1041 | `				}` |
|       90 |  1042 | `			}` |
|     3525 |  1043 | `		}else{` |
|        - |  1044 | `			/* Install static/constant attribute */` |
|       29 |  1045 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       29 |  1046 | `			pVmAttr->iState = 0;` |
|       29 |  1047 | `			pVmAttr->pOwner = pClass;` |
|       29 |  1048 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       29 |  1049 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  1050 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1051 | `				return SXERR_MEM;` |
|        - |  1052 | `			}` |
|        - |  1053 | `		}` |
|        5 |  1054 | `	}` |
|     2223 |  1055 | `	return SXRET_OK;` |
|     1114 |  1056 |  |
|        - |  1057 | `/* Forward declaration */` |
|        - |  1058 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |  1059 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |  1060 | `/*` |
|        - |  1061 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  1062 | ` */` |
|        - |  1063 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  1064 | `/*` |
|        - |  1065 | ` * Reserve a constant memory object.` |
|        - |  1066 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1067 | ` */` |
|   460138 |  1068 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  1069 |  |
|        - |  1070 | `	ph7_value *pObj;` |
|        - |  1071 | `	sxi32 rc;` |
|   460143 |  1072 | `	if( pIndex ){` |
|        - |  1073 | `		/* Object index in the object table */` |
|   450651 |  1074 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   225323 |  1075 | `	}` |
|        - |  1076 | `	/* Reserve a slot for the new object */` |
|   460143 |  1077 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   460143 |  1078 | `	if( rc != SXRET_OK ){` |
|        - |  1079 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1080 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1081 | `		 */` |
|      ! 0 |  1082 | `		return 0;` |
|        - |  1083 | `	}` |
|   460143 |  1084 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   460143 |  1085 | `	return pObj;` |
|   230074 |  1086 |  |
|        - |  1087 | `/*` |
|        - |  1088 | ` * Reserve a memory object.` |
|        - |  1089 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1090 | ` */` |
|  2152384 |  1091 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  1092 |  |
|        - |  1093 | `	ph7_value *pObj;` |
|        - |  1094 | `	sxi32 rc;` |
|  2152389 |  1095 | `	if( pIndex ){` |
|        - |  1096 | `		/* Object index in the object table */` |
|  2152389 |  1097 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1076192 |  1098 | `	}` |
|        - |  1099 | `	/* Reserve a slot for the new object */` |
|  2152389 |  1100 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2152389 |  1101 | `	if( rc != SXRET_OK ){` |
|        - |  1102 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1103 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1104 | `		 */` |
|      ! 0 |  1105 | `		return 0;` |
|        - |  1106 | `	}` |
|  2152389 |  1107 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2152389 |  1108 | `	return pObj;` |
|  1076197 |  1109 |  |
|        - |  1110 | `/* Forward declaration */` |
|        - |  1111 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1112 | `/* Forward declarations for Fiber C functions */` |
|        - |  1113 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1114 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1115 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1116 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1117 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1118 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1119 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1120 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1121 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1122 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1123 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1124 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1125 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1126 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1127 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1128 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1129 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1130 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1131 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1132 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1133 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1134 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1135 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1136 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1137 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1138 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1139 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1140 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1141 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1142 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1143 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1144 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1145 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1146 | `/*` |
|        - |  1147 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1148 | ` * directly as foreign functions.` |
|        - |  1149 | ` */` |
|        - |  1150 | `#define PH7_BUILTIN_LIB \` |
|        - |  1151 | `	"interface Throwable {"\` |
|        - |  1152 | `	"public function getMessage();"\` |
|        - |  1153 | `	"public function getCode();"\` |
|        - |  1154 | `	"public function getFile();"\` |
|        - |  1155 | `	"public function getLine();"\` |
|        - |  1156 | `	"public function getTrace();"\` |
|        - |  1157 | `	"public function getTraceAsString();"\` |
|        - |  1158 | `	"public function getPrevious();"\` |
|        - |  1159 | `	"public function __toString();"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"interface Traversable {}"\` |
|        - |  1162 | `	"interface ArrayAccess {"\` |
|        - |  1163 | `	"public function offsetExists($offset);"\` |
|        - |  1164 | `	"public function offsetGet($offset);"\` |
|        - |  1165 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1166 | `	"public function offsetUnset($offset);"\` |
|        - |  1167 | `	"}"\` |
|        - |  1168 | `	"interface Countable {"\` |
|        - |  1169 | `	"public function count();"\` |
|        - |  1170 | `	"}"\` |
|        - |  1171 | `	"interface Stringable {"\` |
|        - |  1172 | `	"public function __toString();"\` |
|        - |  1173 | `	"}"\` |
|        - |  1174 | `	"interface JsonSerializable {"\` |
|        - |  1175 | `	"public function jsonSerialize();"\` |
|        - |  1176 | `	"}"\` |
|        - |  1177 | `	"interface UnitEnum {"\` |
|        - |  1178 | `	"public static function cases();"\` |
|        - |  1179 | `	"}"\` |
|        - |  1180 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1181 | `	"public static function from($value);"\` |
|        - |  1182 | `	"public static function tryFrom($value);"\` |
|        - |  1183 | `	"}"\` |
|        - |  1184 | `	"class Exception implements Throwable { "\` |
|        - |  1185 | `    "protected $message = '';"\` |
|        - |  1186 | `    "protected $code = 0;"\` |
|        - |  1187 | `    "protected $file;"\` |
|        - |  1188 | `    "protected $line;"\` |
|        - |  1189 | `    "protected $trace;"\` |
|        - |  1190 | `    "protected $previous;"\` |
|        - |  1191 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1192 | `	"   if( isset($message) ){"\` |
|        - |  1193 | `	"	  $this->message = $message;"\` |
|        - |  1194 | `	"   }"\` |
|        - |  1195 | `	"   $this->code = $code;"\` |
|        - |  1196 | `	"   $this->file = __FILE__;"\` |
|        - |  1197 | `	"   $this->line = __LINE__;"\` |
|        - |  1198 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1199 | `	"   if( isset($previous) ){"\` |
|        - |  1200 | `	"     $this->previous = $previous;"\` |
|        - |  1201 | `	"   }"\` |
|        - |  1202 | `	"}"\` |
|        - |  1203 | `	"public function getMessage(){"\` |
|        - |  1204 | `	"   return $this->message;"\` |
|        - |  1205 | `	"}"\` |
|        - |  1206 | `	" public function getCode(){"\` |
|        - |  1207 | `	"  return $this->code;"\` |
|        - |  1208 | `	"}"\` |
|        - |  1209 | `	"public function getFile(){"\` |
|        - |  1210 | `	"  return $this->file;"\` |
|        - |  1211 | `	"}"\` |
|        - |  1212 | `	"public function getLine(){"\` |
|        - |  1213 | `	"  return $this->line;"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `	"public function getTrace(){"\` |
|        - |  1216 | `	"   return $this->trace;"\` |
|        - |  1217 | `	"}"\` |
|        - |  1218 | `	"public function getTraceAsString(){"\` |
|        - |  1219 | `	"  return debug_string_backtrace();"\` |
|        - |  1220 | `	"}"\` |
|        - |  1221 | `	"public function getPrevious(){"\` |
|        - |  1222 | `	"    return $this->previous;"\` |
|        - |  1223 | `	"}"\` |
|        - |  1224 | `	"public function __toString(){"\` |
|        - |  1225 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1226 | `    "}"\` |
|        - |  1227 | `	"}"\` |
|        - |  1228 | `	"class Error implements Throwable { "\` |
|        - |  1229 | `    "protected $message = '';"\` |
|        - |  1230 | `    "protected $code = 0;"\` |
|        - |  1231 | `    "protected $file;"\` |
|        - |  1232 | `    "protected $line;"\` |
|        - |  1233 | `    "protected $trace;"\` |
|        - |  1234 | `    "protected $previous;"\` |
|        - |  1235 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1236 | `	"   if( isset($message) ){"\` |
|        - |  1237 | `	"	  $this->message = $message;"\` |
|        - |  1238 | `	"   }"\` |
|        - |  1239 | `	"   $this->code = $code;"\` |
|        - |  1240 | `	"   $this->file = __FILE__;"\` |
|        - |  1241 | `	"   $this->line = __LINE__;"\` |
|        - |  1242 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1243 | `	"   if( isset($previous) ){"\` |
|        - |  1244 | `	"     $this->previous = $previous;"\` |
|        - |  1245 | `	"   }"\` |
|        - |  1246 | `	"}"\` |
|        - |  1247 | `	"public function getMessage(){"\` |
|        - |  1248 | `	"   return $this->message;"\` |
|        - |  1249 | `	"}"\` |
|        - |  1250 | `	"public function getCode(){"\` |
|        - |  1251 | `	"  return $this->code;"\` |
|        - |  1252 | `	"}"\` |
|        - |  1253 | `	"public function getFile(){"\` |
|        - |  1254 | `	"  return $this->file;"\` |
|        - |  1255 | `	"}"\` |
|        - |  1256 | `	"public function getLine(){"\` |
|        - |  1257 | `	"  return $this->line;"\` |
|        - |  1258 | `	"}"\` |
|        - |  1259 | `	"public function getTrace(){"\` |
|        - |  1260 | `	"   return $this->trace;"\` |
|        - |  1261 | `	"}"\` |
|        - |  1262 | `	"public function getTraceAsString(){"\` |
|        - |  1263 | `	"  return debug_string_backtrace();"\` |
|        - |  1264 | `	"}"\` |
|        - |  1265 | `	"public function getPrevious(){"\` |
|        - |  1266 | `	"    return $this->previous;"\` |
|        - |  1267 | `	"}"\` |
|        - |  1268 | `	"public function __toString(){"\` |
|        - |  1269 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1270 | `	"}"\` |
|        - |  1271 | `	"}"\` |
|        - |  1272 | `	"class TypeError extends Error { }"\` |
|        - |  1273 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1274 | `	"class ValueError extends Error { }"\` |
|        - |  1275 | `	"class FiberError extends Error { }"\` |
|        - |  1276 | `	"class AssertionError extends Error { }"\` |
|        - |  1277 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1278 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1279 | `	"class ErrorException extends Exception { "\` |
|        - |  1280 | `	"protected $severity;"\` |
|        - |  1281 | `	"public function __construct(string $message = null,"\` |
|        - |  1282 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1283 | `	"   if( isset($message) ){"\` |
|        - |  1284 | `	"	  $this->message = $message;"\` |
|        - |  1285 | `	"   }"\` |
|        - |  1286 | `	"   $this->severity = $severity;"\` |
|        - |  1287 | `	"   $this->code = $code;"\` |
|        - |  1288 | `	"   $this->file = $filename;"\` |
|        - |  1289 | `	"   $this->line = $lineno;"\` |
|        - |  1290 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1291 | `	"   if( isset($previous) ){"\` |
|        - |  1292 | `	"     $this->previous = $previous;"\` |
|        - |  1293 | `	"   }"\` |
|        - |  1294 | `	"}"\` |
|        - |  1295 | `	"public function getSeverity(){"\` |
|        - |  1296 | `	"   return $this->severity;"\` |
|        - |  1297 | `    "}"\` |
|        - |  1298 | `	"}"\` |
|        - |  1299 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1300 | `	"class LogicException extends Exception { }"\` |
|        - |  1301 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1302 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1303 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1304 | `	"class DomainException extends LogicException { }"\` |
|        - |  1305 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1306 | `	"class LengthException extends LogicException { }"\` |
|        - |  1307 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1308 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1309 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1310 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1311 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1312 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1313 | `	"interface Iterator extends Traversable {"\` |
|        - |  1314 | `	"public function current();"\` |
|        - |  1315 | `	"public function key();"\` |
|        - |  1316 | `	"public function next();"\` |
|        - |  1317 | `	"public function rewind();"\` |
|        - |  1318 | `	"public function valid();"\` |
|        - |  1319 | `	"}"\` |
|        - |  1320 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1321 | `	"public function getIterator();"\` |
|        - |  1322 | `	"}"\` |
|        - |  1323 | `	"interface Serializable {"\` |
|        - |  1324 | `	"public function serialize();"\` |
|        - |  1325 | `	"public function unserialize(string $serialized);"\` |
|        - |  1326 | `	"}"\` |
|        - |  1327 | `	"/* Directory releated IO */"\` |
|        - |  1328 | `	"class Directory {"\` |
|        - |  1329 | `	"public $handle = null;"\` |
|        - |  1330 | `	"public $path  = null;"\` |
|        - |  1331 | `	"public function __construct(string $path)"\` |
|        - |  1332 | `	"{"\` |
|        - |  1333 | `	"   $this->handle = opendir($path);"\` |
|        - |  1334 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1335 | `	"      $this->path = $path;"\` |
|        - |  1336 | `	"   }"\` |
|        - |  1337 | `	"}"\` |
|        - |  1338 | `	"public function __destruct()"\` |
|        - |  1339 | `	"{"\` |
|        - |  1340 | `	"  if( $this->handle != null ){"\` |
|        - |  1341 | `	"       closedir($this->handle);"\` |
|        - |  1342 | `	"  }"\` |
|        - |  1343 | `	"}"\` |
|        - |  1344 | `	"public function read()"\` |
|        - |  1345 | `	"{"\` |
|        - |  1346 | `	"    return readdir($this->handle);"\` |
|        - |  1347 | `	"}"\` |
|        - |  1348 | `	"public function rewind()"\` |
|        - |  1349 | `	"{"\` |
|        - |  1350 | `	"    rewinddir($this->handle);"\` |
|        - |  1351 | `	"}"\` |
|        - |  1352 | `	"public function close()"\` |
|        - |  1353 | `	"{"\` |
|        - |  1354 | `	"    closedir($this->handle);"\` |
|        - |  1355 | `	"    $this->handle = null;"\` |
|        - |  1356 | `	"}"\` |
|        - |  1357 | `	"}"\` |
|        - |  1358 | `	"class Fiber {"\` |
|        - |  1359 | `	"  private $__ctx;"\` |
|        - |  1360 | `	"  private $__callable;"\` |
|        - |  1361 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1362 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1363 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1364 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1365 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1366 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1367 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1368 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1369 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1370 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1371 | `	"}"\` |
|        - |  1372 | `	"class Generator implements Iterator {"\` |
|        - |  1373 | `	"  private $__ctx;"\` |
|        - |  1374 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1375 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1376 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1377 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1378 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1379 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1380 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1381 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1382 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1383 | `	"}"\` |
|        - |  1384 | `	"class stdClass{"\` |
|        - |  1385 | `	"  public $value;"\` |
|        - |  1386 | `	" /* Magic methods */"\` |
|        - |  1387 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1388 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1389 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1390 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1391 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1392 | `	"}"\` |
|        - |  1393 | `	"function dir(string $path){"\` |
|        - |  1394 | `	"   return new Directory($path);"\` |
|        - |  1395 | `	"}"\` |
|        - |  1396 | `	"function Dir(string $path){"\` |
|        - |  1397 | `	"   return new Directory($path);"\` |
|        - |  1398 | `	"}"\` |
|        - |  1399 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1400 | `    "{"\` |
|        - |  1401 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1402 | `	"  $aDir = array();"\` |
|        - |  1403 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1404 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1405 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1406 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1407 | `	"   }"\` |
|        - |  1408 | `	"  closedir($pHandle);"\` |
|        - |  1409 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1410 | `	"      rsort($aDir);"\` |
|        - |  1411 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1412 | `	"      sort($aDir);"\` |
|        - |  1413 | `	"  }"\` |
|        - |  1414 | `	"  return $aDir;"\` |
|        - |  1415 | `	"}"\` |
|        - |  1416 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1417 | `	"/* Open the target directory */"\` |
|        - |  1418 | `	"$zDir = dirname($pattern);"\` |
|        - |  1419 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1420 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1421 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1422 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1423 | `	"	return FALSE;"\` |
|        - |  1424 | `	"}"\` |
|        - |  1425 | `	"$pattern = basename($pattern);"\` |
|        - |  1426 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1427 | `	"/* Loop throw available entries */"\` |
|        - |  1428 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1429 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1430 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1431 | `	"	if( $rc ){"\` |
|        - |  1432 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1433 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1434 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1435 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1436 | `	"		  }"\` |
|        - |  1437 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1438 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1439 | `	"		 continue;"\` |
|        - |  1440 | `	"	   }"\` |
|        - |  1441 | `	"	   /* Add the entry */"\` |
|        - |  1442 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1443 | `	"	}"\` |
|        - |  1444 | `	" }"\` |
|        - |  1445 | `	"/* Close the handle */"\` |
|        - |  1446 | `	"closedir($pHandle);"\` |
|        - |  1447 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1448 | `	"  /* Sort the array */"\` |
|        - |  1449 | `	"  sort($pArray);"\` |
|        - |  1450 | `	"}"\` |
|        - |  1451 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1452 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1453 | `	"  $pArray[] = $pattern;"\` |
|        - |  1454 | `	"}"\` |
|        - |  1455 | `	"/* Return the created array */"\` |
|        - |  1456 | `	"return $pArray;"\` |
|        - |  1457 | `   "}"\` |
|        - |  1458 | `   "/* Creates a temporary file */"\` |
|        - |  1459 | `   "function tmpfile(){"\` |
|        - |  1460 | `   "  /* Extract the temp directory */"\` |
|        - |  1461 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1462 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1463 | `   "    /* Use the current dir */"\` |
|        - |  1464 | `   "    $zTempDir = '.';"\` |
|        - |  1465 | `   "  }"\` |
|        - |  1466 | `   "  /* Create the file */"\` |
|        - |  1467 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1468 | `   "  return $pHandle;"\` |
|        - |  1469 | `   "}"\` |
|        - |  1470 | `   "/* Creates a temporary filename */"\` |
|        - |  1471 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1472 | `   "{"\` |
|        - |  1473 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1474 | `   "}"\` |
|        - |  1475 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1476 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1477 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1478 | `   "/* Copy arguments */"\` |
|        - |  1479 | `   "$nArgs = func_num_args();"\` |
|        - |  1480 | `   "$pNew = array();"\` |
|        - |  1481 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1482 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1483 | `    "}"\` |
|        - |  1484 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1485 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1486 | `	"/* Erase */"\` |
|        - |  1487 | `	"array_erase($pArray);"\` |
|        - |  1488 | `	"/* Unshift */"\` |
|        - |  1489 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1490 | `	"return sizeof($pArray);"\` |
|        - |  1491 | `    "}"\` |
|        - |  1492 | `	"function array_merge_recursive(){"\` |
|        - |  1493 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1494 | `    "$arrays = func_get_args();"\` |
|        - |  1495 | `    "$narrays = count($arrays);"\` |
|        - |  1496 | `    "$ret = array();"\` |
|        - |  1497 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1498 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1499 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1500 | `	 " }"\` |
|        - |  1501 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1502 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1503 | `     "  if( $keyIsInt ) {"\` |
|        - |  1504 | `     "   $ret[] = $value;"\` |
|        - |  1505 | `     "  } else {"\` |
|        - |  1506 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1507 | `     "    $cur = $ret[$key];"\` |
|        - |  1508 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1509 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1510 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1511 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1512 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1513 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1514 | `     "    } else {"\` |
|        - |  1515 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1516 | `     "    }"\` |
|        - |  1517 | `     "   } else {"\` |
|        - |  1518 | `     "    $ret[$key] = $value;"\` |
|        - |  1519 | `     "   }"\` |
|        - |  1520 | `     "  }"\` |
|        - |  1521 | `     " }"\` |
|        - |  1522 | `	 " }"\` |
|        - |  1523 | `	 " return $ret;"\` |
|        - |  1524 | `    "}"\` |
|        - |  1525 | `	"function max(){"\` |
|        - |  1526 | `    "  $pArgs = func_get_args();"\` |
|        - |  1527 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1528 | `	"  return null;"\` |
|        - |  1529 | `    " }"\` |
|        - |  1530 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1531 | `    " $pArg = $pArgs[0];"\` |
|        - |  1532 | `	" if( !is_array($pArg) ){"\` |
|        - |  1533 | `	"   return $pArg; "\` |
|        - |  1534 | `	" }"\` |
|        - |  1535 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1536 | `	"   return null;"\` |
|        - |  1537 | `	" }"\` |
|        - |  1538 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1539 | `	" reset($pArg);"\` |
|        - |  1540 | `	" $max = current($pArg);"\` |
|        - |  1541 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1542 | `	"   if( $val > $max ){"\` |
|        - |  1543 | `	"     $max = $val;"\` |
|        - |  1544 | `    " }"\` |
|        - |  1545 | `	" }"\` |
|        - |  1546 | `	" return $max;"\` |
|        - |  1547 | `    " }"\` |
|        - |  1548 | `    " $max = $pArgs[0];"\` |
|        - |  1549 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1550 | `    " $val = $pArgs[$i];"\` |
|        - |  1551 | `	"if( $val > $max ){"\` |
|        - |  1552 | `	" $max = $val;"\` |
|        - |  1553 | `	"}"\` |
|        - |  1554 | `    " }"\` |
|        - |  1555 | `	" return $max;"\` |
|        - |  1556 | `    "}"\` |
|        - |  1557 | `	"function min(){"\` |
|        - |  1558 | `    "  $pArgs = func_get_args();"\` |
|        - |  1559 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1560 | `	"  return null;"\` |
|        - |  1561 | `    " }"\` |
|        - |  1562 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1563 | `    " $pArg = $pArgs[0];"\` |
|        - |  1564 | `	" if( !is_array($pArg) ){"\` |
|        - |  1565 | `	"   return $pArg; "\` |
|        - |  1566 | `	" }"\` |
|        - |  1567 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1568 | `	"   return null;"\` |
|        - |  1569 | `	" }"\` |
|        - |  1570 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1571 | `	" reset($pArg);"\` |
|        - |  1572 | `	" $min = current($pArg);"\` |
|        - |  1573 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1574 | `	"   if( $val < $min ){"\` |
|        - |  1575 | `	"     $min = $val;"\` |
|        - |  1576 | `    " }"\` |
|        - |  1577 | `	" }"\` |
|        - |  1578 | `	" return $min;"\` |
|        - |  1579 | `    " }"\` |
|        - |  1580 | `    " $min = $pArgs[0];"\` |
|        - |  1581 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1582 | `    " $val = $pArgs[$i];"\` |
|        - |  1583 | `	"if( $val < $min ){"\` |
|        - |  1584 | `	" $min = $val;"\` |
|        - |  1585 | `	" }"\` |
|        - |  1586 | `    " }"\` |
|        - |  1587 | `	" return $min;"\` |
|        - |  1588 | `	"}"\` |
|        - |  1589 | `	"function fileowner(string $file){"\` |
|        - |  1590 | `    " $a = stat($file);"\` |
|        - |  1591 | `	" if( !is_array($a) ){"\` |
|        - |  1592 | `	"	return false;"\` |
|        - |  1593 | `	" }"\` |
|        - |  1594 | `	" return $a['uid'];"\` |
|        - |  1595 | `    "}"\` |
|        - |  1596 | `    "function filegroup(string $file){"\` |
|        - |  1597 | `	" $a = stat($file);"\` |
|        - |  1598 | `	" if( !is_array($a) ){"\` |
|        - |  1599 | `	"	return false;"\` |
|        - |  1600 | `	" }"\` |
|        - |  1601 | `	" return $a['gid'];"\` |
|        - |  1602 | `    "}"\` |
|        - |  1603 | `	 "function fileinode(string $file){"\` |
|        - |  1604 | `	" $a = stat($file);"\` |
|        - |  1605 | `	" if( !is_array($a) ){"\` |
|        - |  1606 | `	"	return false;"\` |
|        - |  1607 | `	" }"\` |
|        - |  1608 | `	" return $a['ino'];"\` |
|        - |  1609 | `    "}"` |
|        - |  1610 |  |
|        - |  1611 | `/*` |
|        - |  1612 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1613 | ` * start compiling the target PHP program.` |
|        - |  1614 | ` */` |
|     3164 |  1615 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1616 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1617 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1618 | `	 )` |
|        5 |  1619 |  |
|        - |  1620 | `	SyString sBuiltin;` |
|        - |  1621 | `	ph7_value *pObj;` |
|        - |  1622 | `	sxi32 rc;` |
|        - |  1623 | `	/* Zero the structure */` |
|     3169 |  1624 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1625 | `	/* Initialize VM fields */` |
|     3169 |  1626 | `	pVm->pEngine = &(*pEngine);` |
|     3169 |  1627 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1628 | `	/* Instructions containers */` |
|     3169 |  1629 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3169 |  1630 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3169 |  1631 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1632 | `	/* Object containers */` |
|     3169 |  1633 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3169 |  1634 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1635 | `	/* Virtual machine internal containers */` |
|     3169 |  1636 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3169 |  1637 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3169 |  1638 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3169 |  1639 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3169 |  1640 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3169 |  1641 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3169 |  1642 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3169 |  1643 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3169 |  1644 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3169 |  1645 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3169 |  1646 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3169 |  1647 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3169 |  1648 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3169 |  1649 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3169 |  1650 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3169 |  1651 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3169 |  1652 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3169 |  1653 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3169 |  1654 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3169 |  1655 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3169 |  1656 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3169 |  1657 | `	pVm->pPendingException = 0;` |
|        - |  1658 | `	/* Configuration containers */` |
|     3169 |  1659 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3169 |  1660 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3169 |  1661 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3169 |  1662 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3169 |  1663 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3169 |  1664 | `	pVm->iResponseStatus = 200;` |
|     3169 |  1665 | `	pVm->bHeadersSent = 0;` |
|     3169 |  1666 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1667 | `	/* Error callbacks containers */` |
|     3169 |  1668 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3169 |  1669 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3169 |  1670 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3169 |  1671 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3169 |  1672 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1673 | `	/* Set a default recursion limit */` |
|        - |  1674 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3169 |  1675 | `	pVm->nMaxDepth = 32;` |
|        - |  1676 | `#else` |
|        - |  1677 | `	pVm->nMaxDepth = 16;` |
|        - |  1678 | `#endif` |
|        - |  1679 | `	/* Default assertion flags */` |
|     3169 |  1680 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1681 | `	/* JSON return status */` |
|     3169 |  1682 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1683 | `	/* PRNG context */` |
|     3169 |  1684 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1685 | `	/* Install the null constant */` |
|     3169 |  1686 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3169 |  1687 | `	if( pObj == 0 ){` |
|      ! 0 |  1688 | `		rc = SXERR_MEM;` |
|      ! 0 |  1689 | `		goto Err;` |
|        - |  1690 | `	}` |
|     3169 |  1691 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1692 | `	/* Install the boolean TRUE constant */` |
|     3169 |  1693 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3169 |  1694 | `	if( pObj == 0 ){` |
|      ! 0 |  1695 | `		rc = SXERR_MEM;` |
|      ! 0 |  1696 | `		goto Err;` |
|        - |  1697 | `	}` |
|     3169 |  1698 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1699 | `	/* Install the boolean FALSE constant */` |
|     3169 |  1700 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3169 |  1701 | `	if( pObj == 0 ){` |
|      ! 0 |  1702 | `		rc = SXERR_MEM;` |
|      ! 0 |  1703 | `		goto Err;` |
|        - |  1704 | `	}` |
|     3169 |  1705 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1706 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1707 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1708 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3169 |  1709 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3169 |  1710 | `	if( pObj == 0 ){` |
|      ! 0 |  1711 | `		rc = SXERR_MEM;` |
|      ! 0 |  1712 | `		goto Err;` |
|        - |  1713 | `	}` |
|     3169 |  1714 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1715 | `	/* Create the global frame */` |
|     3169 |  1716 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3169 |  1717 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1718 | `		goto Err;` |
|        - |  1719 | `	}` |
|        - |  1720 | `	/* Initialize the code generator */` |
|     3169 |  1721 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3169 |  1722 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1723 | `		goto Err;` |
|        - |  1724 | `	}` |
|        - |  1725 | `	/* VM correctly initialized,set the magic number */` |
|     3169 |  1726 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3169 |  1727 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1728 | `	/* Compile the built-in library */` |
|     3169 |  1729 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1730 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3169 |  1731 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1732 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3169 |  1733 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3169 |  1734 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3169 |  1735 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3169 |  1736 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3169 |  1737 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - |  1738 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3169 |  1739 | `	pVm->pCoalesceObj = 0;` |
|     3169 |  1740 | `	pVm->bCoalesceArmed = 0;` |
|     3169 |  1741 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1742 | `	/* Register Fiber internal C functions */` |
|     3169 |  1743 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3169 |  1744 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3169 |  1745 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3169 |  1746 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3169 |  1747 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3169 |  1748 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3169 |  1749 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3169 |  1750 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3169 |  1751 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3169 |  1752 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1753 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3169 |  1754 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3169 |  1755 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3169 |  1756 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3169 |  1757 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3169 |  1758 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3169 |  1759 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3169 |  1760 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3169 |  1761 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3169 |  1762 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3169 |  1763 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1764 | `	/* Reset the code generator */` |
|     3169 |  1765 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3169 |  1766 | `	return SXRET_OK;` |
|      ! 0 |  1767 | `Err:` |
|      ! 0 |  1768 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1769 | `	return rc;` |
|     1587 |  1770 |  |
|        - |  1771 | `/*` |
|        - |  1772 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1773 | ` * routine which store the output in an internal blob.` |
|        - |  1774 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1775 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1776 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1777 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1778 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1779 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1780 | ` * to finish executing and extracting the output.` |
|        - |  1781 | ` */` |
|       56 |  1782 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1783 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1784 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1785 | `	void *pUserData     /* User private data */` |
|        - |  1786 | `	)` |
|      ! 0 |  1787 |  |
|        - |  1788 | `	 sxi32 rc;` |
|        - |  1789 | `	 /* Store the output in an internal BLOB */` |
|       56 |  1790 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       56 |  1791 | `	 return rc;` |
|      ! 0 |  1792 |  |
|        - |  1793 | `/*` |
|        - |  1794 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1795 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1796 | ` */` |
|    21006 |  1797 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 |  1798 |  |
|    21011 |  1799 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    21011 |  1800 | `	if( xCons != VmObConsumer ){` |
|     8325 |  1801 | `		pVm->nOutputLen += nLen;` |
|     8325 |  1802 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1033 |  1803 | `			pVm->bHeadersSent = 1;` |
|      514 |  1804 | `		}` |
|     4160 |  1805 | `	}` |
|    21011 |  1806 |  |
|        - |  1807 | `#define VM_STACK_GUARD 16` |
|        - |  1808 | `/*` |
|        - |  1809 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1810 | ` * our compiled PHP program.` |
|        - |  1811 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1812 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1813 | ` */` |
|    49150 |  1814 | `static ph7_value * VmNewOperandStack(` |
|        - |  1815 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1816 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1817 | `	)` |
|        5 |  1818 |  |
|        - |  1819 | `	ph7_value *pStack;` |
|        - |  1820 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1821 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1822 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1823 | `  ** on the maximum stack depth required.` |
|        - |  1824 | `  **` |
|        - |  1825 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1826 | `  */` |
|    49155 |  1827 | `	nInstr += VM_STACK_GUARD;` |
|    49155 |  1828 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    49155 |  1829 | `	if( pStack == 0 ){` |
|      ! 0 |  1830 | `		return 0;` |
|        - |  1831 | `	}` |
|        - |  1832 | `	/* Initialize the operand stack */` |
|  3196331 |  1833 | `	while( nInstr > 0 ){` |
|  3147181 |  1834 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  3147181 |  1835 | `		--nInstr;` |
|        5 |  1836 | `	}` |
|        - |  1837 | `	/* Ready for bytecode execution */` |
|    49155 |  1838 | `	return pStack;` |
|    24580 |  1839 |  |
|        - |  1840 | `/* Forward declaration */` |
|        - |  1841 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1842 | `/*` |
|        - |  1843 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1844 | ` * This routine gets called by the PH7 engine after` |
|        - |  1845 | ` * successful compilation of the target PHP program.` |
|        - |  1846 | ` */` |
|     2846 |  1847 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1848 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1849 | `	)` |
|        5 |  1850 |  |
|        - |  1851 | `	SyHashEntry *pEntry;` |
|        - |  1852 | `	sxi32 rc;` |
|     2851 |  1853 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1854 | `		/* Initialize your VM first */` |
|      ! 0 |  1855 | `		return SXERR_CORRUPT;` |
|        - |  1856 | `	}` |
|        - |  1857 | `	/* Mark the VM ready for byte-code execution */` |
|     2851 |  1858 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1859 | `	/* Release the code generator now we have compiled our program, but keep its` |
|        - |  1860 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|        - |  1861 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|        - |  1862 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|     2851 |  1863 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|        - |  1864 | `	/* Emit the DONE instruction */` |
|     2851 |  1865 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2851 |  1866 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1867 | `		return SXERR_MEM;` |
|        - |  1868 | `	}` |
|        - |  1869 | `	/* Script return value */` |
|     2851 |  1870 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1871 | `	/* Pending return value from a catch/finally block (see VmThrowException) */` |
|     2851 |  1872 | `	PH7_MemObjInit(&(*pVm),&pVm->sCatchReturn);` |
|     2851 |  1873 | `	pVm->bReturnRequested = 0;` |
|        - |  1874 | `	/* Allocate a new operand stack */` |
|     2851 |  1875 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2851 |  1876 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1877 | `		return SXERR_MEM;` |
|        - |  1878 | `	}` |
|        - |  1879 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1880 | `	 * private data. */` |
|     2851 |  1881 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2851 |  1882 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1883 | `	/* Allocate the reference table */` |
|     2851 |  1884 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2851 |  1885 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2851 |  1886 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1887 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1888 | `		return SXERR_MEM;` |
|        - |  1889 | `	}` |
|        - |  1890 | `	/* Zero the reference table */` |
|     2851 |  1891 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1892 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2851 |  1893 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2851 |  1894 | `	if( rc != SXRET_OK ){` |
|        - |  1895 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1896 | `		return rc;` |
|        - |  1897 | `	}` |
|        - |  1898 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  1899 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  1900 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  1901 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  1902 | `	 * below it is compile-time/init state that survives a reset. */` |
|     2851 |  1903 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - |  1904 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2851 |  1905 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2851 |  1906 | `	if( rc != SXRET_OK ){` |
|        - |  1907 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1908 | `		return rc;` |
|        - |  1909 | `	}` |
|        - |  1910 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2851 |  1911 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1912 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2851 |  1913 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1914 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2851 |  1915 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1916 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1917 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2851 |  1918 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2851 |  1919 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1920 | `#endif` |
|        - |  1921 | `	/* Initialize and install static and constants class attributes.` |
|        - |  1922 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  1923 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  1924 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  1925 | `	 * that function in sync when changing what is reserved here. */` |
|     2851 |  1926 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   111269 |  1927 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   108425 |  1928 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   108425 |  1929 | `		if( rc != SXRET_OK ){` |
|        3 |  1930 | `			return rc;` |
|        - |  1931 | `		}` |
|        5 |  1932 | `	}` |
|        - |  1933 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2849 |  1934 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1935 | `	/* VM is ready for bytecode execution */` |
|     2849 |  1936 | `	return SXRET_OK;` |
|     1428 |  1937 |  |
|        - |  1938 | `/*` |
|        - |  1939 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  1940 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  1941 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  1942 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  1943 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  1944 | ` */` |
|        6 |  1945 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  1946 |  |
|        - |  1947 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - |  1948 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - |  1949 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      204 |  1950 | `	while( pVm->pRefList ){` |
|      198 |  1951 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 |  1952 | `	}` |
|        6 |  1953 |  |
|        - |  1954 | `/*` |
|        - |  1955 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  1956 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  1957 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  1958 | ` */` |
|       48 |  1959 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  1960 |  |
|       48 |  1961 | `	PH7_MemObjRelease(pObj);` |
|       48 |  1962 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       48 |  1963 |  |
|        - |  1964 | `/*` |
|        - |  1965 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  1966 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  1967 | ` * of statics).` |
|        - |  1968 | ` */` |
|      380 |  1969 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 |  1970 |  |
|      380 |  1971 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  1972 | `	sxu32 k;` |
|      384 |  1973 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        4 |  1974 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        2 |  1975 | `	}` |
|      380 |  1976 |  |
|        - |  1977 | `/*` |
|        - |  1978 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  1979 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  1980 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  1981 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  1982 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  1983 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  1984 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  1985 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  1986 | ` *    has its static sentinels reset.` |
|        - |  1987 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  1988 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  1989 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  1990 | ` */` |
|        6 |  1991 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  1992 |  |
|        - |  1993 | `	SyHashEntry *pEntry;` |
|        6 |  1994 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      386 |  1995 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      380 |  1996 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      380 |  1997 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  1998 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - |  1999 | `			 * release its captured-by-value environment, then free the entry,` |
|        - |  2000 | `			 * name buffer and structure. */` |
|        4 |  2001 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 |  2002 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - |  2003 | `			sxu32 k;` |
|        4 |  2004 | `			VmResetFuncStatics(pFunc);` |
|        8 |  2005 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 |  2006 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 |  2007 | `			}` |
|        4 |  2008 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - |  2009 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  2010 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  2011 | `			if( zName ){` |
|        4 |  2012 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 |  2013 | `			}` |
|        4 |  2014 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 |  2015 | `			continue;` |
|        - |  2016 | `		}` |
|        - |  2017 | `		/* Named function: reset statics for every overload sharing this name. */` |
|      752 |  2018 | `		while( pFunc ){` |
|      376 |  2019 | `			VmResetFuncStatics(pFunc);` |
|      376 |  2020 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  2021 | `		}` |
|      ! 0 |  2022 | `	}` |
|        6 |  2023 | `	pVm->closure_cnt = 0;` |
|        6 |  2024 |  |
|        - |  2025 | `/*` |
|        - |  2026 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  2027 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  2028 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  2029 | ` * the class re-mount registers fresh ones.` |
|        - |  2030 | ` */` |
|        6 |  2031 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  2032 |  |
|        - |  2033 | `	SyHashEntry *pEntry;` |
|        - |  2034 | `	/* Common case: no class static typed properties — table already empty. */` |
|        6 |  2035 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        2 |  2036 | `		return;` |
|        - |  2037 | `	}` |
|        - |  2038 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  2039 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  2040 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  2041 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  2042 | `		if( pEntry->pUserData ){` |
|        4 |  2043 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  2044 | `		}` |
|      ! 0 |  2045 | `	}` |
|        4 |  2046 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  2047 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        3 |  2048 |  |
|        - |  2049 | `/*` |
|        - |  2050 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  2051 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  2052 | ` *` |
|        - |  2053 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  2054 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  2055 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  2056 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  2057 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  2058 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  2059 | ` *` |
|        - |  2060 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  2061 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  2062 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  2063 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  2064 | ` * reserved during the run.` |
|        - |  2065 | ` *` |
|        - |  2066 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  2067 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  2068 | ` * global-scope destructors never fired.` |
|        - |  2069 | ` */` |
|        6 |  2070 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  2071 |  |
|        - |  2072 | `	sxu32 nWater,n;` |
|        6 |  2073 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  2074 | `		return SXERR_CORRUPT;` |
|        - |  2075 | `	}` |
|        6 |  2076 | `	nWater = pVm->nSuperBaseline;` |
|        - |  2077 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  2078 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        6 |  2079 | `	pVm->pGlobal = 0;` |
|        - |  2080 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  2081 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  2082 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - |  2083 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        6 |  2084 | `	pVm->bInReset = 1;` |
|        - |  2085 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        6 |  2086 | `	VmResetRefTable(&(*pVm));` |
|        - |  2087 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  2088 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  2089 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  2090 | `	 * re-run define() overwrites the value in place). */` |
|        6 |  2091 | `	VmResetFunctionState(&(*pVm));` |
|        - |  2092 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  2093 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      218 |  2094 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      212 |  2095 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      212 |  2096 | `		if( pObj ){` |
|      212 |  2097 | `			PH7_MemObjRelease(pObj);` |
|      106 |  2098 | `		}` |
|      106 |  2099 | `	}` |
|        - |  2100 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  2101 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        6 |  2102 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  2103 | `	/* (5) Unwind any active frames back to none. */` |
|       12 |  2104 | `	while( pVm->pFrame ){` |
|        6 |  2105 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2106 | `	}` |
|        - |  2107 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        6 |  2108 | `	pVm->bInReset = 0;` |
|        - |  2109 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  2110 | `	 * slots (their indices no longer exist). */` |
|        6 |  2111 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        6 |  2112 | `	SySetReset(&pVm->aFreeObj);` |
|        - |  2113 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        6 |  2114 | `	SyHashRelease(&pVm->hSuper);` |
|        6 |  2115 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - |  2116 | `	/* (8) Drain remaining per-exec containers. */` |
|        6 |  2117 | `	SySetReset(&pVm->aSelf);` |
|        - |  2118 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  2119 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  2120 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        6 |  2121 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  2122 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  2123 | `		if( pCB ){` |
|        - |  2124 | `			int iArg;` |
|      ! 0 |  2125 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2126 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  2127 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  2128 | `			}` |
|      ! 0 |  2129 | `		}` |
|      ! 0 |  2130 | `	}` |
|        6 |  2131 | `	SySetReset(&pVm->aShutdown);` |
|        6 |  2132 | `	SySetReset(&pVm->aException);` |
|        6 |  2133 | `	pVm->pPendingException = 0;` |
|        6 |  2134 | `	pVm->nExceptDepth = 0;` |
|        - |  2135 | `	/* spl_autoload_register() callbacks are per request */` |
|        6 |  2136 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  2137 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  2138 | `		if( pCB ){` |
|      ! 0 |  2139 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2140 | `		}` |
|      ! 0 |  2141 | `	}` |
|        6 |  2142 | `	SySetReset(&pVm->aAutoload);` |
|        - |  2143 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  2144 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        6 |  2145 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  2146 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  2147 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  2148 | `	}` |
|        - |  2149 | `	/* Output buffers */` |
|        6 |  2150 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  2151 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  2152 | `		if( pOb ){` |
|      ! 0 |  2153 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  2154 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  2155 | `		}` |
|      ! 0 |  2156 | `	}` |
|        6 |  2157 | `	SySetReset(&pVm->aOB);` |
|        6 |  2158 | `	pVm->nObDepth = 0;` |
|        - |  2159 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  2160 | `	{` |
|        6 |  2161 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        6 |  2162 | `		if( rc == SXRET_OK ){` |
|        6 |  2163 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        3 |  2164 | `		}` |
|        6 |  2165 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  2166 | `			return rc;` |
|        - |  2167 | `		}` |
|        - |  2168 | `	}` |
|        - |  2169 | `	/* (10) Re-mount the static/const attribute slots of every class. */` |
|        - |  2170 | `	{` |
|        - |  2171 | `		SyHashEntry *pEntry;` |
|        6 |  2172 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      238 |  2173 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      232 |  2174 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      232 |  2175 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2176 | `				return rc;` |
|        - |  2177 | `			}` |
|      ! 0 |  2178 | `		}` |
|        - |  2179 | `	}` |
|        - |  2180 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        6 |  2181 | `	SyBlobReset(&pVm->sConsumer);` |
|        6 |  2182 | `	pVm->nOutputLen = 0;` |
|        6 |  2183 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        6 |  2184 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        6 |  2185 | `	pVm->iResponseStatus = 200;` |
|        6 |  2186 | `	pVm->bHeadersSent = 0;` |
|        6 |  2187 | `	pVm->bHttpContext = 0;` |
|        6 |  2188 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        6 |  2189 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        6 |  2190 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        6 |  2191 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        6 |  2192 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        6 |  2193 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  2194 | `#ifdef PH7_ENABLE_PCRE` |
|        6 |  2195 | `	pVm->iPcreLastError = 0;` |
|        - |  2196 | `#endif` |
|        6 |  2197 | `	pVm->iCmpCallbackExc = 0;` |
|        6 |  2198 | `	pVm->bReturnRequested = 0;` |
|        6 |  2199 | `	VmReinitMemObj(&(*pVm),&pVm->sCatchReturn);` |
|        6 |  2200 | `	pVm->bHaltRequested = 0;` |
|        6 |  2201 | `	pVm->iExitStatus = 0;` |
|        6 |  2202 | `	pVm->iSpreadExtra = 0;` |
|        6 |  2203 | `	pVm->nRecursionDepth = 0;` |
|        6 |  2204 | `	pVm->pActiveCtx = 0;` |
|        6 |  2205 | `	pVm->pCoalesceObj = 0;` |
|        6 |  2206 | `	pVm->bCoalesceArmed = 0;` |
|        6 |  2207 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  2208 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        6 |  2209 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  2210 | `	/* Set the ready flag */` |
|        6 |  2211 | `	pVm->nMagic = PH7_VM_RUN;` |
|        6 |  2212 | `	return SXRET_OK;` |
|        3 |  2213 |  |
|        - |  2214 | `/*` |
|        - |  2215 | ` * Release a Virtual Machine.` |
|        - |  2216 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  2217 | ` */` |
|     2844 |  2218 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 |  2219 |  |
|        - |  2220 | `	/* Set the stale magic number */` |
|     2849 |  2221 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  2222 | `	/* Release the private memory subsystem */` |
|     2849 |  2223 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2849 |  2224 | `	return SXRET_OK;` |
|        5 |  2225 |  |
|        - |  2226 | `/*` |
|        - |  2227 | ` * Initialize a foreign function call context.` |
|        - |  2228 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  2229 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  2230 | ` * functions.` |
|        - |  2231 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  2232 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  2233 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  2234 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  2235 | ` */` |
|   707704 |  2236 | `static sxi32 VmInitCallContext(` |
|        - |  2237 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2238 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2239 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2240 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2241 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2242 | `	)` |
|        5 |  2243 |  |
|   707709 |  2244 | `	pOut->pFunc = pFunc;` |
|   707709 |  2245 | `	pOut->pVm   = pVm;` |
|   707709 |  2246 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   707709 |  2247 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2248 | `	/* Assume a null return value */` |
|   707709 |  2249 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   707709 |  2250 | `	pOut->pRet = pRet;` |
|   707709 |  2251 | `	pOut->iFlags = iFlags;` |
|   707709 |  2252 | `	return SXRET_OK;` |
|        5 |  2253 |  |
|        - |  2254 | `/*` |
|        - |  2255 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2256 | ` * left behind.` |
|        - |  2257 | ` */` |
|   707704 |  2258 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 |  2259 |  |
|        - |  2260 | `	sxu32 n;` |
|   707709 |  2261 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8817 |  2262 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25815 |  2263 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    17003 |  2264 | `			if( apObj[n] == 0 ){` |
|        - |  2265 | `				/* Already released */` |
|      387 |  2266 | `				continue;` |
|        - |  2267 | `			}` |
|    16621 |  2268 | `			PH7_MemObjRelease(apObj[n]);` |
|    16621 |  2269 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8313 |  2270 | `		}` |
|     8817 |  2271 | `		SySetRelease(&pCtx->sVar);` |
|     4406 |  2272 | `	}` |
|   707709 |  2273 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  2274 | `		ph7_aux_data *aAux;` |
|        - |  2275 | `		void *pChunk;` |
|        - |  2276 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  2277 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  2278 | `		 */` |
|        9 |  2279 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  2280 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  2281 | `			pChunk = aAux[n].pAuxData;` |
|        - |  2282 | `			/* Release the chunk */` |
|       25 |  2283 | `			if( pChunk ){` |
|       25 |  2284 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  2285 | `			}` |
|       13 |  2286 | `		}` |
|        9 |  2287 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  2288 | `	}` |
|   707709 |  2289 |  |
|        - |  2290 | `/*` |
|        - |  2291 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  2292 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  2293 | ` */` |
|      382 |  2294 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  2295 | `	ph7_context *pCtx, /* Call context */` |
|        - |  2296 | `	ph7_value *pValue  /* Release this value */` |
|        - |  2297 | `	)` |
|        5 |  2298 |  |
|      387 |  2299 | `	if( pValue == 0 ){` |
|        - |  2300 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  2301 | `		return;` |
|        - |  2302 | `	}` |
|      387 |  2303 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      387 |  2304 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  2305 | `		sxu32 n;` |
|     1285 |  2306 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1285 |  2307 | `			if( apObj[n] == pValue ){` |
|      387 |  2308 | `				PH7_MemObjRelease(pValue);` |
|      387 |  2309 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  2310 | `				/* Mark as released */` |
|      387 |  2311 | `				apObj[n] = 0;` |
|      387 |  2312 | `				break;` |
|        - |  2313 | `			}` |
|      454 |  2314 | `		}` |
|      191 |  2315 | `	}` |
|      196 |  2316 |  |
|        - |  2317 | `/*` |
|        - |  2318 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  2319 | ` */` |
|  4003218 |  2320 | `static void VmPopOperand(` |
|        - |  2321 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2322 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2323 | `	)` |
|        5 |  2324 |  |
|  4003223 |  2325 | `	ph7_value *pTos = *ppTos;` |
|  8530651 |  2326 | `	while( nPop > 0 ){` |
|  4527433 |  2327 | `		PH7_MemObjRelease(pTos);` |
|  4527433 |  2328 | `		pTos--;` |
|  4527433 |  2329 | `		nPop--;` |
|        5 |  2330 | `	}` |
|        - |  2331 | `	/* Top of the stack */` |
|  4003223 |  2332 | `	*ppTos = pTos;` |
|  4003223 |  2333 |  |
|        - |  2334 | `/*` |
|        - |  2335 | ` * Reserve a memory object.` |
|        - |  2336 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2337 | ` */` |
|  3223588 |  2338 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 |  2339 |  |
|  3223593 |  2340 | `	ph7_value *pObj = 0;` |
|        - |  2341 | `	VmSlot *pSlot;` |
|        - |  2342 | `	sxu32 nIdx;` |
|        - |  2343 | `	/* Check for a free slot */` |
|  3223593 |  2344 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3223593 |  2345 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3223593 |  2346 | `	if( pSlot ){` |
|  1071217 |  2347 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1071217 |  2348 | `		nIdx = pSlot->nIdx;` |
|   535606 |  2349 | `	}` |
|  3223593 |  2350 | `	if( pObj == 0 ){` |
|        - |  2351 | `		/* Reserve a new memory object */` |
|  2152381 |  2352 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2152381 |  2353 | `		if( pObj == 0 ){` |
|      ! 0 |  2354 | `			return 0;` |
|        - |  2355 | `		}` |
|  1076188 |  2356 | `	}` |
|        - |  2357 | `	/* Set a null default value */` |
|  3223593 |  2358 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3223593 |  2359 | `	pObj->nIdx = nIdx;` |
|  3223593 |  2360 | `	return pObj;` |
|  1611799 |  2361 |  |
|        - |  2362 | `/*` |
|        - |  2363 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2364 | ` */` |
|    35674 |  2365 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2366 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2367 | `	const char *zKey,  /* Entry key */` |
|        - |  2368 | `	sxu32 nByte,       /* Key length */` |
|        - |  2369 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2370 | `	)` |
|        5 |  2371 |  |
|        - |  2372 | `	ph7_value sKey;` |
|        - |  2373 | `	sxi32 rc;` |
|    35679 |  2374 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35679 |  2375 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2376 | `	/* Perform the insertion */` |
|    35679 |  2377 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35679 |  2378 | `	PH7_MemObjRelease(&sKey);` |
|    35679 |  2379 | `	return rc;` |
|        5 |  2380 |  |
|        - |  2381 | `/*` |
|        - |  2382 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2383 | ` * Return a pointer to the variable value on success.` |
|        - |  2384 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2385 | ` */` |
|  3715076 |  2386 | `static ph7_value * VmExtractMemObj(` |
|        - |  2387 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2388 | `	const SyString *pName, /* Variable name */` |
|        - |  2389 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2390 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2391 | `	)` |
|        5 |  2392 |  |
|  3715081 |  2393 | `	int bNullify = FALSE;` |
|        - |  2394 | `	SyHashEntry *pEntry;` |
|        - |  2395 | `	VmFrame *pFrame;` |
|        - |  2396 | `	ph7_value *pObj;` |
|        - |  2397 | `	sxu32 nIdx;` |
|        - |  2398 | `	sxi32 rc;` |
|        - |  2399 | `	/* Point to the top active frame */` |
|  3715081 |  2400 | `	pFrame = pVm->pFrame;` |
|  3715081 |  2401 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2402 | `	/* Perform the lookup */` |
|  3715081 |  2403 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2404 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2405 | `		pName = &sAnnon;` |
|        - |  2406 | `		/* Always nullify the object */` |
|      ! 0 |  2407 | `		bNullify = TRUE;` |
|      ! 0 |  2408 | `		bDup = FALSE;` |
|      ! 0 |  2409 | `	}` |
|        - |  2410 | `	/* Check the superglobals table first */` |
|  3715081 |  2411 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3715081 |  2412 | `	if( pEntry == 0 ){` |
|        - |  2413 | `		/* Query the top active frame */` |
|  3715035 |  2414 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3715035 |  2415 | `		if( pEntry == 0 ){` |
|   116093 |  2416 | `			char *zName = (char *)pName->zString;` |
|        - |  2417 | `			VmSlot sLocal;` |
|   116093 |  2418 | `			if( !bCreate ){` |
|        - |  2419 | `				/* Do not create the variable,return NULL instead */` |
|      987 |  2420 | `				return 0;` |
|        - |  2421 | `			}` |
|        - |  2422 | `			/* No such variable,automatically create a new one and install` |
|        - |  2423 | `			 * it in the current frame.` |
|        - |  2424 | `			 */` |
|   115111 |  2425 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   115111 |  2426 | `			if( pObj == 0 ){` |
|      ! 0 |  2427 | `				return 0;` |
|        - |  2428 | `			}` |
|   115111 |  2429 | `			nIdx = pObj->nIdx;` |
|   115111 |  2430 | `			if( bDup ){` |
|        - |  2431 | `				/* Duplicate name */` |
|      232 |  2432 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      232 |  2433 | `				if( zName == 0 ){` |
|      ! 0 |  2434 | `					return 0;` |
|        - |  2435 | `				}` |
|      114 |  2436 | `			}` |
|        - |  2437 | `			/* Link to the top active VM frame */` |
|   115111 |  2438 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   115111 |  2439 | `			if( rc != SXRET_OK ){` |
|        - |  2440 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2441 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2442 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2443 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2444 | `				return 0;` |
|        - |  2445 | `			}` |
|   115111 |  2446 | `			if( pFrame->pParent != 0 ){` |
|        - |  2447 | `				/* Local variable */` |
|   107989 |  2448 | `				sLocal.nIdx = nIdx;` |
|   107989 |  2449 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    53997 |  2450 | `			}else{` |
|        - |  2451 | `				/* Register in the $GLOBALS array */` |
|     7127 |  2452 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2453 | `			}` |
|        - |  2454 | `			/* Install in the reference table */` |
|   115111 |  2455 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2456 | `			/* Save object index */` |
|   115111 |  2457 | `			pObj->nIdx = nIdx;` |
|    57558 |  2458 | `		}else{` |
|        - |  2459 | `			/* Extract variable contents */` |
|  3598947 |  2460 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3598947 |  2461 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3598947 |  2462 | `			if( bNullify && pObj ){` |
|      ! 0 |  2463 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2464 | `			}` |
|        - |  2465 | `		}` |
|  1857139 |  2466 | `	}else{` |
|        - |  2467 | `		/* Superglobal */` |
|       51 |  2468 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       51 |  2469 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2470 | `	}` |
|  3714099 |  2471 | `	return pObj;` |
|  1857653 |  2472 |  |
|        - |  2473 | `/*` |
|        - |  2474 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2475 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2476 | ` */` |
|     3276 |  2477 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2478 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2479 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2480 | `	sxu32 nByte        /* zName length */` |
|        - |  2481 | `	)` |
|        5 |  2482 |  |
|        - |  2483 | `	SyHashEntry *pEntry;` |
|        - |  2484 | `	ph7_value *pValue;` |
|        - |  2485 | `	sxu32 nIdx;` |
|        - |  2486 | `	/* Query the superglobal table */` |
|     3281 |  2487 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3281 |  2488 | `	if( pEntry == 0 ){` |
|        - |  2489 | `		/* No such entry */` |
|      ! 0 |  2490 | `		return 0;` |
|        - |  2491 | `	}` |
|        - |  2492 | `	/* Extract the superglobal index in the global object pool */` |
|     3281 |  2493 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2494 | `	/* Extract the variable value  */` |
|     3281 |  2495 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3281 |  2496 | `	return pValue;` |
|     1643 |  2497 |  |
|        - |  2498 | `/*` |
|        - |  2499 | ` * Perform a raw hashmap insertion.` |
|        - |  2500 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2501 | ` */` |
|     3318 |  2502 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2503 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2504 | `	const char *zKey,   /* Entry key */` |
|        - |  2505 | `	int nKeylen,        /* zKey length*/` |
|        - |  2506 | `	const char *zData,  /* Entry data */` |
|        - |  2507 | `	int nLen            /* zData length */` |
|        - |  2508 | `	)` |
|        5 |  2509 |  |
|        - |  2510 | `	ph7_value sKey,sValue;` |
|        - |  2511 | `	sxi32 rc;` |
|     3323 |  2512 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3323 |  2513 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3323 |  2514 | `	if( zKey ){` |
|     3301 |  2515 | `		if( nKeylen < 0 ){` |
|     3219 |  2516 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1607 |  2517 | `		}` |
|     3301 |  2518 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1648 |  2519 | `	}` |
|     3323 |  2520 | `	if( zData ){` |
|     3323 |  2521 | `		if( nLen < 0 ){` |
|        - |  2522 | `			/* Compute length automatically */` |
|      198 |  2523 | `			nLen = (int)SyStrlen(zData);` |
|       99 |  2524 | `		}` |
|     3323 |  2525 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1659 |  2526 | `	}` |
|        - |  2527 | `	/* Perform the insertion */` |
|     3323 |  2528 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3323 |  2529 | `	PH7_MemObjRelease(&sKey);` |
|     3323 |  2530 | `	PH7_MemObjRelease(&sValue);` |
|     3323 |  2531 | `	return rc;` |
|        5 |  2532 |  |
|        - |  2533 | `/*` |
|        - |  2534 | ` * Configure a working virtual machine instance.` |
|        - |  2535 | ` *` |
|        - |  2536 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2537 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2538 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2539 | ` * The second argument to this function is an integer configuration option` |
|        - |  2540 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2541 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2542 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2543 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2544 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2545 | ` */` |
|    46058 |  2546 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2547 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2548 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2549 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2550 | `	)` |
|        5 |  2551 |  |
|    46063 |  2552 | `	sxi32 rc = SXRET_OK;` |
|    46063 |  2553 | `	switch(nOp){` |
|     1414 |  2554 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2833 |  2555 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2833 |  2556 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2557 | `		/* VM output consumer callback */` |
|        - |  2558 | `#ifdef UNTRUST` |
|        - |  2559 | `		if( xConsumer == 0 ){` |
|        - |  2560 | `			rc = SXERR_CORRUPT;` |
|        - |  2561 | `			break;` |
|        - |  2562 | `		}` |
|        - |  2563 | `#endif` |
|        - |  2564 | `		/* Install the output consumer */` |
|     2833 |  2565 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2833 |  2566 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2833 |  2567 | `		break;` |
|        - |  2568 | `							   }` |
|     1422 |  2569 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2570 | `		/* Import path */` |
|        - |  2571 | `		  const char *zPath;` |
|        - |  2572 | `		  SyString sPath;` |
|     2849 |  2573 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2574 | `#if defined(UNTRUST)` |
|        - |  2575 | `		  if( zPath == 0 ){` |
|        - |  2576 | `			  rc = SXERR_EMPTY;` |
|        - |  2577 | `			  break;` |
|        - |  2578 | `		  }` |
|        - |  2579 | `#endif` |
|     2849 |  2580 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2581 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2582 | `#ifdef __WINNT__` |
|        5 |  2583 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2584 | `#endif` |
|     5693 |  2585 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2586 | `		  /* Remove leading and trailing white spaces */` |
|     2849 |  2587 | `		  SyStringFullTrim(&sPath);` |
|     2849 |  2588 | `		  if( sPath.nByte > 0 ){` |
|        - |  2589 | `			  /* Store the path in the corresponding conatiner */` |
|     2849 |  2590 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1422 |  2591 | `		  }` |
|     2849 |  2592 | `		  break;` |
|        - |  2593 | `									 }` |
|     1425 |  2594 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2595 | `		/* Run-Time Error report */` |
|     2855 |  2596 | `		pVm->bErrReport = 1;` |
|     2855 |  2597 | `		break;` |
|      ! 0 |  2598 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2599 | `		/* Recursion depth */` |
|      ! 0 |  2600 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2601 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2602 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2603 | `		}` |
|      ! 0 |  2604 | `		break;` |
|        - |  2605 | `									   }` |
|      ! 0 |  2606 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2607 | `		/* VM output length in bytes */` |
|      ! 0 |  2608 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2609 | `#ifdef UNTRUST` |
|        - |  2610 | `		if( pOut == 0 ){` |
|        - |  2611 | `			rc = SXERR_CORRUPT;` |
|        - |  2612 | `			break;` |
|        - |  2613 | `		}` |
|        - |  2614 | `#endif` |
|      ! 0 |  2615 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2616 | `		break;` |
|        - |  2617 | `							   }` |
|        - |  2618 |  |
|    14260 |  2619 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2620 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2621 | `		/* Create a new superglobal/global variable */` |
|    28525 |  2622 | `		const char *zName = va_arg(ap,const char *);` |
|    28525 |  2623 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2624 | `		SyHashEntry *pEntry;` |
|        - |  2625 | `		ph7_value *pObj;` |
|        - |  2626 | `		sxu32 nByte;` |
|        - |  2627 | `		sxu32 nIdx;` |
|        - |  2628 | `#ifdef UNTRUST` |
|        - |  2629 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2630 | `			rc = SXERR_CORRUPT;` |
|        - |  2631 | `			break;` |
|        - |  2632 | `		}` |
|        - |  2633 | `#endif` |
|    28525 |  2634 | `		nByte = SyStrlen(zName);` |
|    28525 |  2635 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2636 | `			/* Check if the superglobal is already installed */` |
|    28525 |  2637 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14265 |  2638 | `		}else{` |
|        - |  2639 | `			/* Query the top active VM frame */` |
|      ! 0 |  2640 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2641 | `		}` |
|    28525 |  2642 | `		if( pEntry ){` |
|        - |  2643 | `			/* Variable already installed */` |
|      ! 0 |  2644 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2645 | `			/* Extract contents */` |
|      ! 0 |  2646 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2647 | `			if( pObj ){` |
|        - |  2648 | `				/* Overwrite old contents */` |
|      ! 0 |  2649 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2650 | `			}` |
|      ! 0 |  2651 | `		}else{` |
|        - |  2652 | `			/* Install a new variable */` |
|    28525 |  2653 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28525 |  2654 | `			if( pObj == 0 ){` |
|      ! 0 |  2655 | `				rc = SXERR_MEM;` |
|      ! 0 |  2656 | `				break;` |
|        - |  2657 | `			}` |
|    28525 |  2658 | `			nIdx = pObj->nIdx;` |
|        - |  2659 | `			/* Copy value */` |
|    28525 |  2660 | `			PH7_MemObjStore(pValue,pObj);` |
|    28525 |  2661 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2662 | `				/* Install the superglobal */` |
|    28525 |  2663 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14265 |  2664 | `			}else{` |
|        - |  2665 | `				/* Install in the current frame */` |
|      ! 0 |  2666 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2667 | `			}` |
|    28525 |  2668 | `			if( rc == SXRET_OK ){` |
|        - |  2669 | `				SyHashEntry *pRef;` |
|    28525 |  2670 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28525 |  2671 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14265 |  2672 | `				}else{` |
|      ! 0 |  2673 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2674 | `				}` |
|        - |  2675 | `				/* Install in the reference table */` |
|    28525 |  2676 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28525 |  2677 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2678 | `					/* Register in the $GLOBALS array */` |
|    28525 |  2679 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14260 |  2680 | `				}` |
|    14260 |  2681 | `			}` |
|        - |  2682 | `		}` |
|    28525 |  2683 | `		break;` |
|        - |  2684 | `									}` |
|     1607 |  2685 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2686 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2687 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2688 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2689 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2690 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2691 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3219 |  2692 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3219 |  2693 | `		const char *zValue = va_arg(ap,const char *);` |
|     3219 |  2694 | `		int nLen = va_arg(ap,int);` |
|        - |  2695 | `		ph7_hashmap *pMap;` |
|        - |  2696 | `		ph7_value *pValue;` |
|     3219 |  2697 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2698 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2699 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3218 |  2700 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2701 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2702 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3217 |  2703 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2704 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2705 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3217 |  2706 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2707 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2708 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3217 |  2709 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2710 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2711 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3217 |  2712 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2713 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2714 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2715 | `		}else{` |
|        - |  2716 | `			/* Extract the $_SERVER superglobal */` |
|     3217 |  2717 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2718 | `		}` |
|     3219 |  2719 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2720 | `			/* No such entry */` |
|      ! 0 |  2721 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2722 | `			break;` |
|        - |  2723 | `		}` |
|        - |  2724 | `		/* Point to the hashmap */` |
|     3219 |  2725 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2726 | `		/* Perform the insertion */` |
|     3219 |  2727 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3219 |  2728 | `		break;` |
|        - |  2729 | `								   }` |
|       11 |  2730 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2731 | `		/* Script arguments */` |
|       27 |  2732 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2733 | `		ph7_hashmap *pMap;` |
|        - |  2734 | `		ph7_value *pValue;` |
|        - |  2735 | `		sxu32 n;` |
|       27 |  2736 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2737 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2738 | `			break;` |
|        - |  2739 | `		}` |
|        - |  2740 | `		/* Extract the $argv array */` |
|       27 |  2741 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       27 |  2742 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2743 | `			/* No such entry */` |
|      ! 0 |  2744 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2745 | `			break;` |
|        - |  2746 | `		}` |
|        - |  2747 | `		/* Point to the hashmap */` |
|       27 |  2748 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2749 | `		/* Perform the insertion */` |
|       27 |  2750 | `		n = (sxu32)SyStrlen(zValue);` |
|       27 |  2751 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       27 |  2752 | `		if( rc == SXRET_OK ){` |
|       27 |  2753 | `			if( pMap->nEntry > 1 ){` |
|        - |  2754 | `				/* Append space separator first */` |
|       21 |  2755 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2756 | `			}` |
|       27 |  2757 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2758 | `		}` |
|       27 |  2759 | `		break;` |
|        - |  2760 | `								  }` |
|      ! 0 |  2761 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2762 | `		/* error_log() consumer */` |
|      ! 0 |  2763 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2764 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2765 | `		break;` |
|        - |  2766 | `										}` |
|      ! 0 |  2767 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2768 | `		/* Script return value */` |
|      ! 0 |  2769 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2770 | `#ifdef UNTRUST` |
|        - |  2771 | `		if( ppValue == 0 ){` |
|        - |  2772 | `			rc = SXERR_CORRUPT;` |
|        - |  2773 | `			break;` |
|        - |  2774 | `		}` |
|        - |  2775 | `#endif` |
|      ! 0 |  2776 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2777 | `		break;` |
|        - |  2778 | `								   }` |
|     2846 |  2779 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2780 | `		/* Register an IO stream device */` |
|     5697 |  2781 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2782 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8538 |  2783 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5697 |  2784 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2785 | `				/* Invalid stream */` |
|      ! 0 |  2786 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2787 | `				break;` |
|        - |  2788 | `		}` |
|     5697 |  2789 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2790 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2851 |  2791 | `			pVm->pDefStream = pStream;` |
|     1423 |  2792 | `		}` |
|        - |  2793 | `		/* Insert in the appropriate container */` |
|     5697 |  2794 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5697 |  2795 | `		break;` |
|        - |  2796 | `								  }` |
|       11 |  2797 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2798 | `		/* Point to the VM internal output consumer buffer */` |
|       22 |  2799 | `		const void **ppOut = va_arg(ap,const void **);` |
|       22 |  2800 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2801 | `#ifdef UNTRUST` |
|        - |  2802 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2803 | `			rc = SXERR_CORRUPT;` |
|        - |  2804 | `			break;` |
|        - |  2805 | `		}` |
|        - |  2806 | `#endif` |
|       22 |  2807 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       22 |  2808 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       22 |  2809 | `		break;` |
|        - |  2810 | `									   }` |
|       11 |  2811 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2812 | `		/* Raw HTTP request*/` |
|       22 |  2813 | `		const char *zRequest = va_arg(ap,const char *);` |
|       22 |  2814 | `		int nByte = va_arg(ap,int);` |
|       22 |  2815 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2816 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2817 | `			break;` |
|        - |  2818 | `		}` |
|       22 |  2819 | `		if( nByte < 0 ){` |
|        - |  2820 | `			/* Compute length automatically */` |
|      ! 0 |  2821 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2822 | `		}` |
|        - |  2823 | `		/* Process the request */` |
|       22 |  2824 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2825 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       22 |  2826 | `		if( rc == SXRET_OK ){` |
|       22 |  2827 | `			pVm->bHttpContext = 1;` |
|       11 |  2828 | `		}` |
|       22 |  2829 | `		break;` |
|        - |  2830 | `									}` |
|       11 |  2831 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2832 | `		/* Extract HTTP response status code */` |
|       22 |  2833 | `		int *pStatus = va_arg(ap, int *);` |
|       22 |  2834 | `		if( pStatus ){` |
|       22 |  2835 | `			*pStatus = pVm->iResponseStatus;` |
|       11 |  2836 | `		}` |
|       22 |  2837 | `		break;` |
|        - |  2838 | `										}` |
|       11 |  2839 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2840 | `		/* Iterate response headers via callback */` |
|        - |  2841 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       22 |  2842 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       22 |  2843 | `		void *pUserData = va_arg(ap, void *);` |
|       22 |  2844 | `		if( xCallback ){` |
|       22 |  2845 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       22 |  2846 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       34 |  2847 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2848 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2849 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2850 | `							   pUserData);` |
|       12 |  2851 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2852 | `					break;` |
|        - |  2853 | `				}` |
|        6 |  2854 | `			}` |
|       11 |  2855 | `		}` |
|       22 |  2856 | `		break;` |
|        - |  2857 | `										 }` |
|      ! 0 |  2858 | `	default:` |
|        - |  2859 | `		/* Unknown configuration option */` |
|      ! 0 |  2860 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2861 | `		break;` |
|        - |  2862 | `	}` |
|    46063 |  2863 | `	return rc;` |
|        5 |  2864 |  |
|        - |  2865 | `/* Forward declaration */` |
|        - |  2866 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2867 | `/*` |
|        - |  2868 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2869 | ` * format.` |
|        - |  2870 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2871 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2872 | ` * (STDOUT).` |
|        - |  2873 | ` */` |
|        2 |  2874 | `static sxi32 VmByteCodeDump(` |
|        - |  2875 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2876 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2877 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2878 | `	)` |
|        1 |  2879 |  |
|        - |  2880 | `	static const char zDump[] = {` |
|        - |  2881 | `		"====================================================\n"` |
|        - |  2882 | `		"PH7 VM Dump\n"` |
|        - |  2883 | `		"====================================================\n"` |
|        - |  2884 | `	};` |
|        - |  2885 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2886 | `	sxi32 rc = SXRET_OK;` |
|        - |  2887 | `	sxu32 n;` |
|        - |  2888 | `	/* Point to the PH7 instructions */` |
|        3 |  2889 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2890 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2891 | `	n = 0;` |
|        3 |  2892 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2893 | `	/* Dump instructions */` |
|        7 |  2894 | `	for(;;){` |
|       15 |  2895 | `		if( pInstr >= pEnd ){` |
|        - |  2896 | `			/* No more instructions */` |
|        3 |  2897 | `			break;` |
|        - |  2898 | `		}` |
|        - |  2899 | `		/* Format and call the consumer callback */` |
|       19 |  2900 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2901 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2902 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2903 | `		if( rc != SXRET_OK ){` |
|        - |  2904 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2905 | `			return rc;` |
|        - |  2906 | `		}` |
|       13 |  2907 | `		++n;` |
|       13 |  2908 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2909 | `	}` |
|        3 |  2910 | `	return rc;` |
|        2 |  2911 |  |
|        - |  2912 | `/* Forward declaration */` |
|        - |  2913 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2914 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2915 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2916 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2917 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2918 | `/*` |
|        - |  2919 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2920 | ` * consumer callback.` |
|        - |  2921 | ` */` |
|      606 |  2922 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        4 |  2923 |  |
|      610 |  2924 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      610 |  2925 | `	sxi32 rc = SXRET_OK;` |
|        - |  2926 | `	/* Append a new line */` |
|        - |  2927 | `#ifdef __WINNT__` |
|        4 |  2928 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2929 | `#else` |
|      606 |  2930 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2931 | `#endif` |
|        - |  2932 | `	/* Invoke the output consumer callback */` |
|      610 |  2933 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      610 |  2934 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      610 |  2935 | `	return rc;` |
|        4 |  2936 |  |
|        - |  2937 | `/*` |
|        - |  2938 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2939 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2940 | ` * information.` |
|        - |  2941 | ` */` |
|      154 |  2942 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |  2943 |  |
|      159 |  2944 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2945 | `		ph7_value apArg[4];` |
|        - |  2946 | `		ph7_value *apArgPtr[4];` |
|        - |  2947 | `		ph7_value sResult;` |
|        - |  2948 | `		SyString sErr;` |
|        - |  2949 | `		/* Prepare arguments */` |
|       76 |  2950 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2951 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2952 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2953 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2954 | `		if( pFile ){` |
|       76 |  2955 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2956 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2957 | `		}else{` |
|      ! 0 |  2958 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2959 | `		}` |
|       76 |  2960 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2961 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2962 | `		/* Set up pointer array */` |
|       76 |  2963 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2964 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2965 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2966 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2967 | `		/* Call the handler */` |
|       76 |  2968 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2969 | `		/* Check return value */` |
|       76 |  2970 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2971 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2972 | `		}` |
|        - |  2973 | `		/* Release */` |
|       76 |  2974 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2975 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2976 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2977 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2978 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2979 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2980 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2981 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2982 | `	}` |
|        - |  2983 | `	/* No handler, always call error handler */` |
|       84 |  2984 | `	return TRUE;` |
|       82 |  2985 |  |
|      110 |  2986 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2987 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2988 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2989 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2990 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2991 | `	)` |
|        5 |  2992 |  |
|      115 |  2993 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2994 | `	SyString *pFile;` |
|        - |  2995 | `	char *zErr;` |
|      115 |  2996 | `	sxi32 rc = SXRET_OK;` |
|      115 |  2997 | `	if( !pVm->bErrReport ){` |
|        - |  2998 | `		/* Don't bother reporting errors */` |
|        3 |  2999 | `		return SXRET_OK;` |
|        - |  3000 | `	}` |
|        - |  3001 | `	/* Reset the working buffer */` |
|      113 |  3002 | `	SyBlobReset(pWorker);` |
|        - |  3003 | `	/* Peek the processed file if available */` |
|      113 |  3004 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      113 |  3005 | `	if( pFile ){` |
|        - |  3006 | `		/* Append file name */` |
|      113 |  3007 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      113 |  3008 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  3009 | `	}` |
|        - |  3010 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  3011 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  3012 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  3013 | `	 * E_DEPRECATED). */` |
|      113 |  3014 | `	zErr = "Error:  ";` |
|      113 |  3015 | `	switch(iErr){` |
|       19 |  3016 | `	case PH7_CTX_WARNING:` |
|       42 |  3017 | `		zErr = "Warning:  ";` |
|       42 |  3018 | `		break;` |
|        6 |  3019 | `	case PH7_CTX_NOTICE:` |
|       15 |  3020 | `		zErr = "Notice:  ";` |
|       12 |  3021 | `		break;` |
|       29 |  3022 | `	default:` |
|        - |  3023 | `		/* keep iErr unchanged */` |
|       58 |  3024 | `		break;` |
|        - |  3025 | `	}` |
|      113 |  3026 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      113 |  3027 | `	if( pFuncName ){` |
|        - |  3028 | `		/* Append function name first */` |
|       24 |  3029 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       24 |  3030 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  3031 | `	}` |
|      113 |  3032 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  3033 | `	/* Check for user error handler.  compute length of C string */` |
|      113 |  3034 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       52 |  3035 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  3036 | `	}` |
|      113 |  3037 | `	return rc;` |
|       60 |  3038 |  |
|        - |  3039 | `/*` |
|        - |  3040 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  3041 | ` *` |
|        - |  3042 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  3043 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  3044 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  3045 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  3046 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  3047 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  3048 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  3049 | ` * calling it from a VM op.` |
|        - |  3050 | ` */` |
|      ! 0 |  3051 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  3052 |  |
|      ! 0 |  3053 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  3054 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  3055 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  3056 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  3057 | `	return PH7_ABORT;` |
|      ! 0 |  3058 |  |
|        - |  3059 | `/*` |
|        - |  3060 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  3061 | ` */` |
|      ! 0 |  3062 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  3063 |  |
|      ! 0 |  3064 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  3065 |  |
|        - |  3066 | `/*` |
|        - |  3067 | ` * Single source of truth for the call-recursion cap policy. Each recursion` |
|        - |  3068 | ` * entry point (OP_CALL, eval/include, fibers/generators) tests this before` |
|        - |  3069 | ` * descending another native C frame; the control flow on a hit differs per` |
|        - |  3070 | ` * site, but the rule itself lives here.` |
|        - |  3071 | ` */` |
|    32658 |  3072 | `static int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  3073 |  |
|    32663 |  3074 | `	return pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  3075 |  |
|        - |  3076 | `/*` |
|        - |  3077 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  3078 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  3079 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  3080 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  3081 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  3082 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  3083 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  3084 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  3085 | ` * register_shutdown_function() callbacks. Used by every recursion path —` |
|        - |  3086 | ` * OP_CALL, eval()/include/require (VmEvalChunk) and fibers/generators` |
|        - |  3087 | ` * (VmStartCtx/VmResumeCtx).` |
|        - |  3088 | ` *` |
|        - |  3089 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  3090 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  3091 | ` * re-enter and loop.` |
|        - |  3092 | ` */` |
|        6 |  3093 | `static sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        2 |  3094 |  |
|        8 |  3095 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  3096 | `		return PH7_ABORT;` |
|        - |  3097 | `	}` |
|        8 |  3098 | `	pVm->iExitStatus = 255;` |
|        8 |  3099 | `	pVm->bHaltRequested = 1;` |
|        8 |  3100 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        8 |  3101 | `	return PH7_ABORT;` |
|        5 |  3102 |  |
|        - |  3103 | `/*` |
|        - |  3104 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3105 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3106 | ` * information.` |
|        - |  3107 | ` */` |
|       46 |  3108 | `static sxi32 VmThrowErrorAp(` |
|        - |  3109 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3110 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  3111 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  3112 | `	const char *zFormat, /* Format message */` |
|        - |  3113 | `	va_list ap           /* Variable list of arguments */` |
|        - |  3114 | `	)` |
|        5 |  3115 |  |
|       51 |  3116 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  3117 | `	SyBlob sMsg;` |
|        - |  3118 | `	SyString *pFile;` |
|        - |  3119 | `	char *zErr;` |
|       51 |  3120 | `	sxi32 rc = SXRET_OK;` |
|       51 |  3121 | `	if( !pVm->bErrReport ){` |
|        - |  3122 | `		/* Don't bother reporting errors */` |
|      ! 0 |  3123 | `		return SXRET_OK;` |
|        - |  3124 | `	}` |
|        - |  3125 | `	/* Reset the working buffer */` |
|       51 |  3126 | `	SyBlobReset(pWorker);` |
|        - |  3127 | `	/* Peek the processed file if available */` |
|       51 |  3128 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       51 |  3129 | `	if( pFile ){` |
|        - |  3130 | `		/* Append file name */` |
|       51 |  3131 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       51 |  3132 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       23 |  3133 | `	}` |
|        - |  3134 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  3135 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  3136 | `	 * the correct errno value. */` |
|       51 |  3137 | `	zErr = "Error:  ";` |
|       51 |  3138 | `	switch(iErr){` |
|        4 |  3139 | `	case PH7_CTX_WARNING:` |
|       11 |  3140 | `		zErr = "Warning:  ";` |
|       11 |  3141 | `		break;` |
|        3 |  3142 | `	case PH7_CTX_NOTICE:` |
|        8 |  3143 | `		zErr = "Notice:  ";` |
|        6 |  3144 | `		break;` |
|       16 |  3145 | `	default:` |
|        - |  3146 | `		/* do not change iErr */` |
|       32 |  3147 | `		break;` |
|        - |  3148 | `	}` |
|       51 |  3149 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       51 |  3150 | `	if( pFuncName ){` |
|        - |  3151 | `		/* Append function name first */` |
|       28 |  3152 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       28 |  3153 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  3154 | `	}` |
|        - |  3155 | `	/* Format the raw message */` |
|       51 |  3156 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       51 |  3157 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  3158 | `	/* Check if a user error handler is installed */` |
|       51 |  3159 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  3160 | `		/* No handler or handler returned TRUE, normal processing */` |
|       36 |  3161 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       36 |  3162 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       16 |  3163 | `	}` |
|       51 |  3164 | `	SyBlobRelease(&sMsg);` |
|       51 |  3165 | `	return rc;` |
|       28 |  3166 |  |
|        - |  3167 | `/*` |
|        - |  3168 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  3169 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  3170 | ` * possible.` |
|        - |  3171 | ` */` |
|       42 |  3172 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        5 |  3173 |  |
|        - |  3174 | `	ph7_class *pClass;` |
|       47 |  3175 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  3176 | `	ph7_class_instance *pThis;` |
|        - |  3177 | `	ph7_class_method *pCons;` |
|        - |  3178 | `	ph7_value sArg;` |
|        - |  3179 | `	ph7_value *apArg[1];` |
|        - |  3180 | `	SyBlob sMsg;` |
|        - |  3181 | `	SyString sMsgStr;` |
|        - |  3182 | `	VmFrame *pFrame;` |
|        - |  3183 | `	sxi32 rc;` |
|       47 |  3184 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       47 |  3185 | `	if( pClass == 0 ){` |
|      ! 0 |  3186 | `		return PH7_ABORT;` |
|        - |  3187 | `	}` |
|       47 |  3188 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       47 |  3189 | `	if( pThis == 0 ){` |
|      ! 0 |  3190 | `		return PH7_ABORT;` |
|        - |  3191 | `	}` |
|       47 |  3192 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3193 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3194 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  3195 | `	{` |
|       47 |  3196 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       47 |  3197 | `		if( pOwner ){` |
|       47 |  3198 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       21 |  3199 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       26 |  3200 | `		}else{` |
|      ! 0 |  3201 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3202 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3203 | `		}` |
|        - |  3204 | `	}` |
|       47 |  3205 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       47 |  3206 | `	if( pCons ){` |
|       47 |  3207 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       47 |  3208 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       47 |  3209 | `		apArg[0] = &sArg;` |
|       47 |  3210 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       47 |  3211 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3212 | `	}` |
|       47 |  3213 | `	SyBlobRelease(&sMsg);` |
|       47 |  3214 | `	pFrame = pVm->pFrame;` |
|       47 |  3215 | `	if( pFrame ){` |
|       47 |  3216 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       47 |  3217 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3218 | `	}` |
|       47 |  3219 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       47 |  3220 | `	PH7_ClassInstanceUnref(pThis);` |
|       47 |  3221 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3222 | `		return PH7_ABORT;` |
|        - |  3223 | `	}` |
|       47 |  3224 | `	return PH7_EXCEPTION;` |
|       26 |  3225 |  |
|        - |  3226 |  |
|        - |  3227 | `/*` |
|        - |  3228 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  3229 | ` */` |
|        4 |  3230 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 |  3231 |  |
|        - |  3232 | `	ph7_class *pErrClass;` |
|        - |  3233 | `	ph7_class_instance *pThis;` |
|        - |  3234 | `	ph7_class_method *pCons;` |
|        - |  3235 | `	ph7_value sArg;` |
|        - |  3236 | `	ph7_value *apArg[1];` |
|        - |  3237 | `	SyBlob sMsg;` |
|        - |  3238 | `	SyString sMsgStr;` |
|        - |  3239 | `	VmFrame *pFrame;` |
|        - |  3240 | `	sxi32 rc;` |
|        6 |  3241 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        6 |  3242 | `	if( pErrClass == 0 ){` |
|      ! 0 |  3243 | `		return PH7_ABORT;` |
|        - |  3244 | `	}` |
|        6 |  3245 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        6 |  3246 | `	if( pThis == 0 ){` |
|      ! 0 |  3247 | `		return PH7_ABORT;` |
|        - |  3248 | `	}` |
|        6 |  3249 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3250 | `	{` |
|        6 |  3251 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        6 |  3252 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        6 |  3253 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  3254 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  3255 | `	}` |
|        6 |  3256 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        6 |  3257 | `	if( pCons ){` |
|        6 |  3258 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        6 |  3259 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        6 |  3260 | `		apArg[0] = &sArg;` |
|        6 |  3261 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        6 |  3262 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3263 | `	}` |
|        6 |  3264 | `	SyBlobRelease(&sMsg);` |
|        6 |  3265 | `	pFrame = pVm->pFrame;` |
|        6 |  3266 | `	if( pFrame ){` |
|        6 |  3267 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        6 |  3268 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3269 | `	}` |
|        6 |  3270 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        6 |  3271 | `	PH7_ClassInstanceUnref(pThis);` |
|        6 |  3272 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3273 | `		return PH7_ABORT;` |
|        - |  3274 | `	}` |
|        6 |  3275 | `	return PH7_EXCEPTION;` |
|        4 |  3276 |  |
|        - |  3277 |  |
|        - |  3278 | `/*` |
|        - |  3279 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  3280 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  3281 | ` * For class types, instanceof is verified.` |
|        - |  3282 | ` *` |
|        - |  3283 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  3284 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  3285 | ` */` |
|        - |  3286 | `/*` |
|        - |  3287 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  3288 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  3289 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  3290 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  3291 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  3292 | ` */` |
|       22 |  3293 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        3 |  3294 |  |
|        - |  3295 | `	const char *z, *zEnd, *zTail;` |
|        - |  3296 | `	sxu32 n;` |
|        - |  3297 | `	sxu8 bReal;` |
|        - |  3298 | `	sxi32 rc;` |
|       25 |  3299 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3300 | `		return 0;` |
|        - |  3301 | `	}` |
|       25 |  3302 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       25 |  3303 | `	n = SyBlobLength(&pValue->sBlob);` |
|       25 |  3304 | `	zEnd = z + n;` |
|       25 |  3305 | `	if( n == 0 ){` |
|      ! 0 |  3306 | `		return 0;` |
|        - |  3307 | `	}` |
|       25 |  3308 | `	zTail = 0;` |
|       25 |  3309 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       25 |  3310 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        8 |  3311 | `		return 0;` |
|        - |  3312 | `	}` |
|        - |  3313 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       19 |  3314 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3315 | `		zTail++;` |
|      ! 0 |  3316 | `	}` |
|       19 |  3317 | `	return zTail == zEnd ? 1 : 0;` |
|       14 |  3318 |  |
|        - |  3319 |  |
|        - |  3320 | `/*` |
|        - |  3321 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  3322 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  3323 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  3324 | ` *   0 if it's not strictly numeric.` |
|        - |  3325 | ` */` |
|       16 |  3326 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  3327 |  |
|        - |  3328 | `	const char *z, *zEnd, *zTail;` |
|        - |  3329 | `	sxu32 n;` |
|       18 |  3330 | `	sxu8 bReal = 0;` |
|        - |  3331 | `	sxi32 rc;` |
|       18 |  3332 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3333 | `		return 0;` |
|        - |  3334 | `	}` |
|       18 |  3335 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  3336 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  3337 | `	zEnd = z + n;` |
|       18 |  3338 | `	if( n == 0 ) return 0;` |
|       18 |  3339 | `	zTail = 0;` |
|       18 |  3340 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  3341 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  3342 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  3343 | `	if( zTail != zEnd ) return 0;` |
|       15 |  3344 | `	return bReal ? 2 : 1;` |
|       10 |  3345 |  |
|        - |  3346 |  |
|        - |  3347 | `/*` |
|        - |  3348 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - |  3349 | `` * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not`` |
|        - |  3350 | ` * scalar keywords), so without this every enforcement site — return, parameter,` |
|        - |  3351 | ` * property, union alternative — would have to string-match the name itself.` |
|        - |  3352 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - |  3353 | ` * to extend when another literal/pseudo type is added.` |
|        - |  3354 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - |  3355 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - |  3356 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - |  3357 | ` */` |
|      160 |  3358 | `static int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        4 |  3359 |  |
|      164 |  3360 | `	const char *z = pClass->zString;` |
|      164 |  3361 | `	sxu32 n = pClass->nByte;` |
|      164 |  3362 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|       51 |  3363 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - |  3364 | `	}` |
|      114 |  3365 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       15 |  3366 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - |  3367 | `	}` |
|      100 |  3368 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|        3 |  3369 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - |  3370 | `	}` |
|       98 |  3371 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - |  3372 | `		/* iterable === array \| Traversable */` |
|       17 |  3373 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3374 | `			return 1;` |
|        - |  3375 | `		}` |
|       11 |  3376 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|        5 |  3377 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        5 |  3378 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|        5 |  3379 | `				return 1;` |
|        - |  3380 | `			}` |
|      ! 0 |  3381 | `		}` |
|        7 |  3382 | `		return 0;` |
|        - |  3383 | `	}` |
|       82 |  3384 | `	return -1;` |
|       84 |  3385 |  |
|        - |  3386 | `/*` |
|        - |  3387 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  3388 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  3389 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  3390 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  3391 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  3392 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  3393 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  3394 | ` * throw.` |
|        - |  3395 | ` *` |
|        - |  3396 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  3397 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  3398 | ` */` |
|      106 |  3399 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        5 |  3400 |  |
|        - |  3401 | `	sxu32 i;` |
|        - |  3402 | `	ph7_type_alt *aAlts;` |
|        - |  3403 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  3404 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      111 |  3405 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       16 |  3406 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  3407 | `	}` |
|       99 |  3408 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - |  3409 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - |  3410 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - |  3411 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - |  3412 | ``	 * `iterable\|Foo`). */`` |
|      283 |  3413 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      186 |  3414 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      112 |  3415 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|        3 |  3416 | `			return SXRET_OK;` |
|        - |  3417 | `		}` |
|       97 |  3418 | `	}` |
|       97 |  3419 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       97 |  3420 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      281 |  3421 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      189 |  3422 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      163 |  3423 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      163 |  3424 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      163 |  3425 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       83 |  3426 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       55 |  3427 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  3428 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       97 |  3429 | `	}` |
|        - |  3430 | `	/* Object handling */` |
|       97 |  3431 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       19 |  3432 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       19 |  3433 | `		if( bHasClassAlt ){` |
|       14 |  3434 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  3435 | `			ph7_class *pSelfNow = 0;` |
|       14 |  3436 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  3437 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  3438 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  3439 | `			}` |
|       26 |  3440 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  3441 | `				ph7_class *pExpected;` |
|        - |  3442 | `				SyString *pCN;` |
|       22 |  3443 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  3444 | `				pCN = &aAlts[i].sClass;` |
|       22 |  3445 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3446 | `					pExpected = pSelfNow;` |
|       22 |  3447 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3448 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3449 | `				}else{` |
|       22 |  3450 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3451 | `				}` |
|       22 |  3452 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  3453 | `					return SXRET_OK;` |
|        - |  3454 | `				}` |
|        8 |  3455 | `			}` |
|        2 |  3456 | `		}` |
|       10 |  3457 | `		return SXERR_INVALID;` |
|        - |  3458 | `	}` |
|        - |  3459 | `	/* Array handling */` |
|       80 |  3460 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        9 |  3461 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3462 | `	}` |
|        - |  3463 | `	/* Scalar handling — exact match first */` |
|       72 |  3464 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       30 |  3465 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3466 | `	}` |
|       44 |  3467 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3468 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3469 | `	}` |
|       40 |  3470 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       40 |  3471 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3472 | `	}` |
|       18 |  3473 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3474 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3475 | `	}` |
|       18 |  3476 | `	if( bStrict ){` |
|        - |  3477 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3478 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3479 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3480 | `			return SXRET_OK;` |
|        - |  3481 | `		}` |
|      ! 0 |  3482 | `		return SXERR_INVALID;` |
|        - |  3483 | `	}` |
|        - |  3484 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3485 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3486 | `	 * to match PHP's union RFC. */` |
|        - |  3487 | `	{` |
|       18 |  3488 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3489 | `		if( bHasInt ){` |
|        - |  3490 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3491 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3492 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3493 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3494 | `				return SXRET_OK;` |
|        - |  3495 | `			}` |
|       18 |  3496 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3497 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3498 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3499 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3500 | `					return SXRET_OK;` |
|        - |  3501 | `				}` |
|      ! 0 |  3502 | `			}` |
|       18 |  3503 | `			if( kind == 1 ){` |
|        9 |  3504 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3505 | `				return SXRET_OK;` |
|        - |  3506 | `			}` |
|        4 |  3507 | `		}` |
|       10 |  3508 | `		if( bHasFloat ){` |
|       10 |  3509 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3510 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3511 | `				return SXRET_OK;` |
|        - |  3512 | `			}` |
|       10 |  3513 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3514 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3515 | `				return SXRET_OK;` |
|        - |  3516 | `			}` |
|        1 |  3517 | `		}` |
|        3 |  3518 | `		if( bHasString ){` |
|      ! 0 |  3519 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3520 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3521 | `				return SXRET_OK;` |
|        - |  3522 | `			}` |
|      ! 0 |  3523 | `		}` |
|        3 |  3524 | `		if( bHasBool ){` |
|      ! 0 |  3525 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3526 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3527 | `				return SXRET_OK;` |
|        - |  3528 | `			}` |
|      ! 0 |  3529 | `		}` |
|        - |  3530 | `	}` |
|        3 |  3531 | `	return SXERR_INVALID;` |
|       58 |  3532 |  |
|        - |  3533 |  |
|        - |  3534 | `/*` |
|        - |  3535 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3536 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3537 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3538 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3539 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3540 | ` */` |
|       38 |  3541 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        3 |  3542 |  |
|        - |  3543 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - |  3544 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - |  3545 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - |  3546 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - |  3547 | `	 * silently swallow any argument. */` |
|       41 |  3548 | `	if( nType == MEMOBJ_NULL ){` |
|        3 |  3549 | `		return SXERR_INVALID;` |
|        - |  3550 | `	}` |
|       39 |  3551 | `	if( bStrict ){` |
|        - |  3552 | `		/* Only int -> float widening is allowed implicitly. */` |
|       13 |  3553 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3554 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3555 | `			return SXRET_OK;` |
|        - |  3556 | `		}` |
|       11 |  3557 | `		return SXERR_INVALID;` |
|        - |  3558 | `	}` |
|        - |  3559 | `	{` |
|       28 |  3560 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3561 | `		if( xCast ) xCast(pVal);` |
|        - |  3562 | `	}` |
|       28 |  3563 | `	return SXRET_OK;` |
|       22 |  3564 |  |
|        - |  3565 |  |
|        - |  3566 | `/*` |
|        - |  3567 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3568 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3569 | ` *` |
|        - |  3570 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3571 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3572 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3573 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3574 | ` */` |
|       12 |  3575 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        4 |  3576 |  |
|       16 |  3577 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       16 |  3578 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       16 |  3579 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       16 |  3580 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       16 |  3581 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        6 |  3582 | `		}` |
|       16 |  3583 | `		zBuf[nCopy] = 0;` |
|       16 |  3584 | `		return zBuf;` |
|        - |  3585 | `	}` |
|      ! 0 |  3586 | `	switch( nType ){` |
|      ! 0 |  3587 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3588 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3589 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3590 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3591 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3592 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3593 | `		default:             return "scalar";` |
|        - |  3594 | `	}` |
|       10 |  3595 |  |
|        - |  3596 |  |
|        - |  3597 | `/*` |
|        - |  3598 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3599 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3600 | ` */` |
|       18 |  3601 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        3 |  3602 |  |
|       21 |  3603 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       30 |  3604 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3605 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       21 |  3606 | `	return zBuf;` |
|        3 |  3607 |  |
|        - |  3608 |  |
|     6782 |  3609 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 |  3610 |  |
|        - |  3611 | `	SyHashEntry *pSlot;` |
|        - |  3612 | `	VmClassAttr *pVmAttr;` |
|        - |  3613 | `	ph7_class_attr *pAttr;` |
|     6787 |  3614 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6787 |  3615 | `	if( pSlot == 0 ){` |
|     6561 |  3616 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3617 | `	}` |
|      231 |  3618 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      231 |  3619 | `	pAttr = pVmAttr->pAttr;` |
|      231 |  3620 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3621 | `		return SXRET_OK;` |
|        - |  3622 | `	}` |
|        - |  3623 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3624 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3625 | `	 * matching PHP's documented behavior. */` |
|      231 |  3626 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       25 |  3627 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3628 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3629 |  |
|       18 |  3630 | `		if( rc == SXRET_OK ){` |
|        9 |  3631 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3632 | `			return SXRET_OK;` |
|        - |  3633 | `		}` |
|        9 |  3634 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3635 | `			char zBuf[128];` |
|        4 |  3636 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3637 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3638 | `		}` |
|        6 |  3639 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3640 | `	}` |
|        - |  3641 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - |  3642 | `	 * includes null). */` |
|      217 |  3643 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       15 |  3644 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       12 |  3645 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 |  3646 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       14 |  3647 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       14 |  3648 | `			return SXRET_OK;` |
|        - |  3649 | `		}` |
|        3 |  3650 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3651 | `	}` |
|        - |  3652 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - |  3653 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - |  3654 | `	 * type error. */` |
|      203 |  3655 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 |  3656 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3657 | `	}` |
|        - |  3658 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3659 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3660 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      203 |  3661 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3662 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3663 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3664 | `			return SXRET_OK;` |
|        - |  3665 | `		}` |
|        7 |  3666 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3667 | `	}` |
|        - |  3668 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - |  3669 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - |  3670 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - |  3671 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - |  3672 | `	 * wrongly accept any object / reject arrays. */` |
|      193 |  3673 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       39 |  3674 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       39 |  3675 | `		if( rcPseudo == 1 ){` |
|       11 |  3676 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       11 |  3677 | `			return SXRET_OK;` |
|        - |  3678 | `		}` |
|       29 |  3679 | `		if( rcPseudo == 0 ){` |
|        3 |  3680 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3681 | `		}` |
|        - |  3682 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       12 |  3683 | `	}` |
|      181 |  3684 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3685 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3686 | `		 * currently active on the self-stack. */` |
|       27 |  3687 | `		ph7_class *pExpected = 0;` |
|       27 |  3688 | `		SyString *pClassName = &pAttr->sClass;` |
|       27 |  3689 | `		ph7_class *pSelfNow = 0;` |
|       27 |  3690 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3691 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3692 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3693 | `		}` |
|       27 |  3694 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3695 | `			pExpected = pSelfNow;` |
|       25 |  3696 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3697 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3698 | `		}else{` |
|       23 |  3699 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3700 | `		}` |
|       27 |  3701 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3702 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3703 | `		}` |
|       27 |  3704 | `		if( pExpected ){` |
|       23 |  3705 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       23 |  3706 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3707 | `				char zBuf[128];` |
|        8 |  3708 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3709 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3710 | `			}` |
|        8 |  3711 | `		}` |
|       23 |  3712 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       23 |  3713 | `		return SXRET_OK;` |
|        - |  3714 | `	}` |
|        - |  3715 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3716 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      157 |  3717 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3718 | `		char zBuf[128];` |
|       11 |  3719 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3720 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3721 | `	}` |
|      151 |  3722 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       31 |  3723 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       31 |  3724 | `		if( xCast ){` |
|        - |  3725 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       31 |  3726 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3727 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3728 | `			}` |
|       29 |  3729 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        6 |  3730 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3731 | `			}` |
|        - |  3732 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3733 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3734 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       29 |  3735 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       19 |  3736 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       22 |  3737 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|       13 |  3738 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3739 | `			}` |
|       12 |  3740 | `			xCast(pValue);` |
|        5 |  3741 | `		}` |
|        5 |  3742 | `	}` |
|      134 |  3743 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      134 |  3744 | `	return SXRET_OK;` |
|     3396 |  3745 |  |
|        - |  3746 | `/*` |
|        - |  3747 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - |  3748 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - |  3749 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - |  3750 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - |  3751 | ` */` |
|        4 |  3752 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        2 |  3753 |  |
|        6 |  3754 | `	ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        - |  3755 | `	char zBuf[128];` |
|        - |  3756 | `	const char *zGiven;` |
|        6 |  3757 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3758 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3759 | `	}else{` |
|        6 |  3760 | `		zGiven = ph7_type_name(pValue);` |
|        - |  3761 | `	}` |
|        - |  3762 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - |  3763 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - |  3764 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - |  3765 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - |  3766 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - |  3767 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - |  3768 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|        6 |  3769 | `	if( pVm->sCodeGen.xErr ){` |
|        4 |  3770 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - |  3771 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|        1 |  3772 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|        2 |  3773 | `	}else{` |
|        4 |  3774 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3775 | `			"Cannot use %s as value for class constant %z::%z of type %z",` |
|        1 |  3776 | `			zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3777 | `	}` |
|        6 |  3778 | `	pVm->iExitStatus = 255;` |
|        6 |  3779 | `	pVm->bHaltRequested = 1;` |
|        6 |  3780 | `	return SXERR_ABORT;` |
|        2 |  3781 |  |
|        - |  3782 | `/*` |
|        - |  3783 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - |  3784 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - |  3785 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - |  3786 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - |  3787 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - |  3788 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - |  3789 | ` */` |
|     1068 |  3790 | `static sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        3 |  3791 |  |
|     1071 |  3792 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - |  3793 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|     1071 |  3794 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  3795 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 |  3796 | `			return SXRET_OK;` |
|        - |  3797 | `		}` |
|      ! 0 |  3798 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 |  3799 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 |  3800 | `			return SXRET_OK;` |
|        - |  3801 | `		}` |
|      ! 0 |  3802 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3803 | `	}` |
|        - |  3804 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|     1069 |  3805 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        5 |  3806 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */) == SXRET_OK ){` |
|        5 |  3807 | `			return SXRET_OK;` |
|        - |  3808 | `		}` |
|      ! 0 |  3809 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3810 | `	}` |
|        - |  3811 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|     1065 |  3812 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 |  3813 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3814 | `	}` |
|        - |  3815 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|     1065 |  3816 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 |  3817 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3818 | `			return SXRET_OK;` |
|        - |  3819 | `		}` |
|      ! 0 |  3820 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3821 | `	}` |
|        - |  3822 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - |  3823 | `	 * a real class/interface verified by instanceof. */` |
|     1065 |  3824 | `	if( pAttr->nType == SXU32_HIGH ){` |
|      ! 0 |  3825 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|      ! 0 |  3826 | `		if( rcPseudo == 1 ){` |
|      ! 0 |  3827 | `			return SXRET_OK;` |
|        - |  3828 | `		}` |
|      ! 0 |  3829 | `		if( rcPseudo == 0 ){` |
|      ! 0 |  3830 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3831 | `		}` |
|        - |  3832 | `		/* rcPseudo == -1: a real class/interface type. */` |
|      ! 0 |  3833 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3834 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3835 | `		}` |
|        - |  3836 | `		{` |
|      ! 0 |  3837 | `			SyString *pCN = &pAttr->sClass;` |
|        - |  3838 | `			ph7_class *pExpected;` |
|      ! 0 |  3839 | `			if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3840 | `				pExpected = pClass;` |
|      ! 0 |  3841 | `			}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3842 | `				pExpected = pClass->pBase;` |
|      ! 0 |  3843 | `			}else{` |
|      ! 0 |  3844 | `				pExpected = PH7_VmExtractClass(&(*pVm),pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3845 | `			}` |
|      ! 0 |  3846 | `			if( pExpected ){` |
|      ! 0 |  3847 | `				ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      ! 0 |  3848 | `				if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3849 | `					return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|        - |  3850 | `				}` |
|      ! 0 |  3851 | `			}` |
|        - |  3852 | `		}` |
|      ! 0 |  3853 | `		return SXRET_OK;` |
|        - |  3854 | `	}` |
|        - |  3855 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - |  3856 | `	 * implicit widening. Everything else is a type error.` |
|        - |  3857 | `	 *` |
|        - |  3858 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - |  3859 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT (a `1.0` literal, and — because `/` always`` |
|        - |  3860 | ``	 * yields a real — an evenly-dividing `4/2`), so such a value satisfies a`` |
|        - |  3861 | ``	 * `: int` constant here. PHP accepts `const int X = 4/2` (its `/` yields a`` |
|        - |  3862 | ``	 * genuine int) but rejects `const int X = 1.0`; PHL cannot tell the two`` |
|        - |  3863 | ``	 * apart by flag, so it accepts both rather than rejecting the valid `4/2`.`` |
|        - |  3864 | ``	 * A fractional real (`1.5`, MEMOBJ_REAL only) carries no MEMOBJ_INT and is`` |
|        - |  3865 | `	 * correctly rejected. Tightening this needs PHL's float-identity/division` |
|        - |  3866 | `	 * model, which is out of scope here. */` |
|     1065 |  3867 | `	if( pValue->iFlags & pAttr->nType ){` |
|     1057 |  3868 | `		return SXRET_OK;` |
|        - |  3869 | `	}` |
|        9 |  3870 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        3 |  3871 | `		PH7_MemObjToReal(pValue);` |
|        3 |  3872 | `		return SXRET_OK;` |
|        - |  3873 | `	}` |
|        6 |  3874 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      537 |  3875 |  |
|        - |  3876 |  |
|        - |  3877 | `/*` |
|        - |  3878 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3879 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3880 | ` * information.` |
|        - |  3881 | ` * ------------------------------------` |
|        - |  3882 | ` * Simple boring wrapper function.` |
|        - |  3883 | ` * ------------------------------------` |
|        - |  3884 | ` */` |
|       22 |  3885 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        3 |  3886 |  |
|        - |  3887 | `	va_list ap;` |
|        - |  3888 | `	sxi32 rc;` |
|       25 |  3889 | `	va_start(ap,zFormat);` |
|       25 |  3890 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       25 |  3891 | `	va_end(ap);` |
|       25 |  3892 | `	return rc;` |
|        3 |  3893 |  |
|        - |  3894 | `/*` |
|        - |  3895 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3896 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3897 | ` */` |
|       42 |  3898 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 |  3899 |  |
|        - |  3900 | `	ph7_class *pClass;` |
|        - |  3901 | `	ph7_class_instance *pThis;` |
|        - |  3902 | `	ph7_class_method *pCons;` |
|        - |  3903 | `	ph7_value sArg;` |
|        - |  3904 | `	ph7_value *apArg[1];` |
|        - |  3905 | `	SyBlob sMsg;` |
|        - |  3906 | `	SyString sMsgStr;` |
|        - |  3907 | `	VmFrame *pFrame;` |
|        - |  3908 | `	sxi32 rc;` |
|       47 |  3909 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       47 |  3910 | `	if( pClass == 0 ){` |
|      ! 0 |  3911 | `		return PH7_ABORT;` |
|        - |  3912 | `	}` |
|       47 |  3913 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       47 |  3914 | `	if( pThis == 0 ){` |
|      ! 0 |  3915 | `		return PH7_ABORT;` |
|        - |  3916 | `	}` |
|       47 |  3917 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       47 |  3918 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       21 |  3919 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       47 |  3920 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       47 |  3921 | `	if( pCons ){` |
|       47 |  3922 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       47 |  3923 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       47 |  3924 | `		apArg[0] = &sArg;` |
|       47 |  3925 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       47 |  3926 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3927 | `	}` |
|       47 |  3928 | `	SyBlobRelease(&sMsg);` |
|       47 |  3929 | `	pFrame = pVm->pFrame;` |
|       47 |  3930 | `	if( pFrame ){` |
|       47 |  3931 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       47 |  3932 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3933 | `	}` |
|       47 |  3934 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       47 |  3935 | `	PH7_ClassInstanceUnref(pThis);` |
|       47 |  3936 | `	if( rc == SXERR_ABORT ){` |
|        6 |  3937 | `		return PH7_ABORT;` |
|        - |  3938 | `	}` |
|       43 |  3939 | `	return PH7_EXCEPTION;` |
|       26 |  3940 |  |
|        - |  3941 | `/*` |
|        - |  3942 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3943 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3944 | ` */` |
|       12 |  3945 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        4 |  3946 |  |
|        - |  3947 | `	ph7_class *pClass;` |
|        - |  3948 | `	ph7_class_instance *pThis;` |
|        - |  3949 | `	ph7_class_method *pCons;` |
|        - |  3950 | `	ph7_value sArg;` |
|        - |  3951 | `	ph7_value *apArg[1];` |
|        - |  3952 | `	SyBlob sMsg;` |
|        - |  3953 | `	SyString sMsgStr;` |
|        - |  3954 | `	VmFrame *pFrame;` |
|        - |  3955 | `	sxi32 rc;` |
|       16 |  3956 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       16 |  3957 | `	if( pClass == 0 ){` |
|      ! 0 |  3958 | `		return PH7_ABORT;` |
|        - |  3959 | `	}` |
|       16 |  3960 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       16 |  3961 | `	if( pThis == 0 ){` |
|      ! 0 |  3962 | `		return PH7_ABORT;` |
|        - |  3963 | `	}` |
|       16 |  3964 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       16 |  3965 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        6 |  3966 | `		pFuncName,zExpected,zGiven);` |
|       16 |  3967 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       16 |  3968 | `	if( pCons ){` |
|       16 |  3969 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       16 |  3970 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       16 |  3971 | `		apArg[0] = &sArg;` |
|       16 |  3972 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       16 |  3973 | `		PH7_MemObjRelease(&sArg);` |
|        6 |  3974 | `	}` |
|       16 |  3975 | `	SyBlobRelease(&sMsg);` |
|       16 |  3976 | `	pFrame = pVm->pFrame;` |
|       16 |  3977 | `	if( pFrame ){` |
|       16 |  3978 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       16 |  3979 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 |  3980 | `	}` |
|       16 |  3981 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       16 |  3982 | `	PH7_ClassInstanceUnref(pThis);` |
|       16 |  3983 | `	if( rc == SXERR_ABORT ){` |
|        9 |  3984 | `		return PH7_ABORT;` |
|        - |  3985 | `	}` |
|        7 |  3986 | `	return PH7_EXCEPTION;` |
|       10 |  3987 |  |
|        - |  3988 | `/*` |
|        - |  3989 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3990 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3991 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3992 | ` */` |
|       28 |  3993 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        3 |  3994 |  |
|       31 |  3995 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       10 |  3996 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3997 | `	}` |
|       23 |  3998 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        9 |  3999 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        9 |  4000 | `		if( pThis && pThis->pClass ){` |
|        9 |  4001 | `			SyString *pName = &pThis->pClass->sName;` |
|        9 |  4002 | `			sxu32 n = pName->nByte;` |
|        9 |  4003 | `			if( n >= nBuf ){` |
|      ! 0 |  4004 | `				n = nBuf - 1;` |
|      ! 0 |  4005 | `			}` |
|        9 |  4006 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        9 |  4007 | `			zBuf[n] = 0;` |
|        9 |  4008 | `			return zBuf;` |
|        - |  4009 | `		}` |
|      ! 0 |  4010 | `		return "object";` |
|        - |  4011 | `	}` |
|       16 |  4012 | `	return ph7_type_name(pVal);` |
|       17 |  4013 |  |
|        - |  4014 | `/*` |
|        - |  4015 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  4016 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  4017 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  4018 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  4019 | ` */` |
|       18 |  4020 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        3 |  4021 |  |
|        - |  4022 | `	ph7_class *pClass;` |
|        - |  4023 | `	ph7_class_instance *pThis;` |
|        - |  4024 | `	ph7_class_method *pCons;` |
|        - |  4025 | `	ph7_value sArg;` |
|        - |  4026 | `	ph7_value *apArg[1];` |
|        - |  4027 | `	SyBlob sMsg;` |
|        - |  4028 | `	SyString sMsgStr;` |
|        - |  4029 | `	VmFrame *pFrame;` |
|        - |  4030 | `	sxi32 rc;` |
|       21 |  4031 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  4032 | `	char zNameBuf[64];` |
|       21 |  4033 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       21 |  4034 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       21 |  4035 | `	if( pClass == 0 ){` |
|      ! 0 |  4036 | `		return PH7_ABORT;` |
|        - |  4037 | `	}` |
|       21 |  4038 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       21 |  4039 | `	if( pThis == 0 ){` |
|      ! 0 |  4040 | `		return PH7_ABORT;` |
|        - |  4041 | `	}` |
|       21 |  4042 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       21 |  4043 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       21 |  4044 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       21 |  4045 | `	if( pCons ){` |
|       21 |  4046 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       21 |  4047 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       21 |  4048 | `		apArg[0] = &sArg;` |
|       21 |  4049 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       21 |  4050 | `		PH7_MemObjRelease(&sArg);` |
|        9 |  4051 | `	}` |
|       21 |  4052 | `	SyBlobRelease(&sMsg);` |
|       21 |  4053 | `	pFrame = pVm->pFrame;` |
|       21 |  4054 | `	if( pFrame ){` |
|       21 |  4055 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       21 |  4056 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        9 |  4057 | `	}` |
|       21 |  4058 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       21 |  4059 | `	PH7_ClassInstanceUnref(pThis);` |
|       21 |  4060 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4061 | `		return PH7_ABORT;` |
|        - |  4062 | `	}` |
|       21 |  4063 | `	return PH7_EXCEPTION;` |
|       12 |  4064 |  |
|        - |  4065 | `/*` |
|        - |  4066 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  4067 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  4068 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  4069 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  4070 | ` */` |
|        - |  4071 | `/*` |
|        - |  4072 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  4073 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  4074 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  4075 | ` */` |
|       34 |  4076 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        5 |  4077 |  |
|        - |  4078 | `	sxu32 nCopy;` |
|       39 |  4079 | `	if( nBuf == 0 ) return "";` |
|       39 |  4080 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  4081 | `		zBuf[0] = 0;` |
|      ! 0 |  4082 | `		return zBuf;` |
|        - |  4083 | `	}` |
|       39 |  4084 | `	nCopy = SyStringLength(pStr);` |
|       39 |  4085 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       39 |  4086 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       39 |  4087 | `	zBuf[nCopy] = 0;` |
|       39 |  4088 | `	return zBuf;` |
|       22 |  4089 |  |
|        - |  4090 |  |
|      474 |  4091 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 |  4092 |  |
|      479 |  4093 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  4094 | `	const char *zGiven;` |
|        - |  4095 | `	char zBuf[128];` |
|        - |  4096 | `	char zTypeBuf[128];` |
|        - |  4097 | `	/* Untyped function: no enforcement. */` |
|      479 |  4098 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  4099 | `		return SXRET_OK;` |
|        - |  4100 | `	}` |
|        - |  4101 | `	/* void return type: the function must not produce a value. */` |
|      479 |  4102 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      156 |  4103 | `		if( pValue == 0 ){` |
|      154 |  4104 | `			return SXRET_OK;` |
|        - |  4105 | `		}` |
|        - |  4106 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  4107 | `		 * still counts as "returned a value" here. */` |
|        3 |  4108 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  4109 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  4110 | `	}` |
|        - |  4111 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  4112 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  4113 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      327 |  4114 | `	if( pValue == 0 ){` |
|      ! 0 |  4115 | `		const char *zExpected = "value";` |
|      ! 0 |  4116 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  4117 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  4118 | `		}` |
|      ! 0 |  4119 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  4120 | `	}` |
|        - |  4121 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - |  4122 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - |  4123 | `	 * matching how every other typed return reports a missing value.) */` |
|      327 |  4124 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 |  4125 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  4126 | `			return SXRET_OK;` |
|        - |  4127 | `		}` |
|        4 |  4128 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|        1 |  4129 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  4130 | `	}` |
|        - |  4131 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - |  4132 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - |  4133 | `	 * Check by value before the real-class instanceof branch below. */` |
|      323 |  4134 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|       64 |  4135 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|       64 |  4136 | `		if( rcPseudo == 1 ){` |
|       53 |  4137 | `			return SXRET_OK;` |
|        - |  4138 | `		}` |
|       12 |  4139 | `		if( rcPseudo == 0 ){` |
|        9 |  4140 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        4 |  4141 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  4142 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  4143 | `		}` |
|        - |  4144 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|        3 |  4145 | `	}` |
|        - |  4146 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  4147 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  4148 | `	 * bNullable=0 here. */` |
|      267 |  4149 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  4150 | `		sxi32 rcU;` |
|      ! 0 |  4151 | `		int bNullable = 0;` |
|      ! 0 |  4152 | `		const char *zExpected = "union";` |
|        - |  4153 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  4154 | `		{` |
|        - |  4155 | `			sxu32 i;` |
|      ! 0 |  4156 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  4157 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  4158 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  4159 | `			}` |
|        - |  4160 | `		}` |
|      ! 0 |  4161 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  4162 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  4163 | `			return SXRET_OK;` |
|        - |  4164 | `		}` |
|      ! 0 |  4165 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4166 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4167 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  4168 | `			zGiven = "null";` |
|      ! 0 |  4169 | `		}else{` |
|      ! 0 |  4170 | `			zGiven = ph7_type_name(pValue);` |
|        - |  4171 | `		}` |
|      ! 0 |  4172 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  4173 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  4174 | `		}` |
|      ! 0 |  4175 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4176 | `	}` |
|        - |  4177 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  4178 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  4179 | `	 * it into the TypeError message. */` |
|      267 |  4180 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        8 |  4181 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  4182 | `		const char *zExpected;` |
|        - |  4183 | `		ph7_class *pExpected;` |
|        8 |  4184 | `		ph7_class *pSelfNow = 0;` |
|        8 |  4185 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        8 |  4186 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        8 |  4187 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        3 |  4188 | `		}` |
|        8 |  4189 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  4190 | `			pExpected = pSelfNow;` |
|        6 |  4191 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  4192 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  4193 | `		}else{` |
|        5 |  4194 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  4195 | `		}` |
|        8 |  4196 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        8 |  4197 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  4198 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  4199 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4200 | `		}` |
|        8 |  4201 | `		if( pExpected ){` |
|        6 |  4202 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  4203 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  4204 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4205 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4206 | `			}` |
|        2 |  4207 | `		}` |
|        8 |  4208 | `		return SXRET_OK;` |
|        - |  4209 | `	}` |
|        - |  4210 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  4211 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  4212 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  4213 | `	 * via the type-text leading '?'. */` |
|      261 |  4214 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  4215 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  4216 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  4217 | `			return SXRET_OK;` |
|        - |  4218 | `		}` |
|      ! 0 |  4219 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4220 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4221 | `			"null");` |
|        - |  4222 | `	}` |
|        - |  4223 | `	/* Exact match? Done. */` |
|      255 |  4224 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      249 |  4225 | `		return SXRET_OK;` |
|        - |  4226 | `	}` |
|        - |  4227 | `	/* Object->scalar is never compatible. */` |
|        9 |  4228 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4229 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4230 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4231 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4232 | `			zGiven);` |
|        - |  4233 | `	}` |
|        - |  4234 | `	/* Array <-> scalar is never compatible. */` |
|        9 |  4235 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  4236 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4237 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4238 | `			ph7_type_name(pValue));` |
|        - |  4239 | `	}` |
|        - |  4240 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  4241 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  4242 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  4243 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  4244 | `	if( !bStrict` |
|        5 |  4245 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  4246 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        7 |  4247 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  4248 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4249 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4250 | `			"string");` |
|        - |  4251 | `	}` |
|        6 |  4252 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  4253 | `		return SXRET_OK;` |
|        - |  4254 | `	}` |
|        4 |  4255 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4256 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  4257 | `		ph7_type_name(pValue));` |
|      242 |  4258 |  |
|        - |  4259 | `/*` |
|        - |  4260 | ` * Report a fatal named-argument error.` |
|        - |  4261 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  4262 | ` */` |
|        6 |  4263 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        2 |  4264 |  |
|        8 |  4265 | `	const char *zFunc = 0;` |
|        8 |  4266 | `	int nFunc = 0;` |
|        8 |  4267 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        8 |  4268 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        2 |  4269 |  |
|        - |  4270 | `/*` |
|        - |  4271 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  4272 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  4273 | ` * information.` |
|        - |  4274 | ` * ------------------------------------` |
|        - |  4275 | ` * Simple boring wrapper function.` |
|        - |  4276 | ` * ------------------------------------` |
|        - |  4277 | ` */` |
|       24 |  4278 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        4 |  4279 |  |
|        - |  4280 | `	sxi32 rc;` |
|       28 |  4281 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       28 |  4282 | `	return rc;` |
|        4 |  4283 |  |
|        - |  4284 | `/*` |
|        - |  4285 | ` * Resolve function context from the current frame.` |
|        - |  4286 | ` */` |
|     1018 |  4287 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 |  4288 |  |
|        - |  4289 | `	VmFrame *pFrame;` |
|        - |  4290 | `	ph7_vm_func *pFunc;` |
|     1022 |  4291 | `	*pzFuncName = 0;` |
|     1022 |  4292 | `	*pnFuncLen = 0;` |
|     1022 |  4293 | `	pFrame = pVm->pFrame;` |
|     1022 |  4294 | `	if( pFrame == 0 ){` |
|      ! 0 |  4295 | `		return;` |
|        - |  4296 | `	}` |
|     1022 |  4297 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1022 |  4298 | `	if( pFrame->pParent == 0 ){` |
|      998 |  4299 | `		return;` |
|        - |  4300 | `	}` |
|       28 |  4301 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       28 |  4302 | `	if( pFunc == 0 ){` |
|      ! 0 |  4303 | `		return;` |
|        - |  4304 | `	}` |
|       28 |  4305 | `	*pzFuncName = pFunc->sName.zString;` |
|       28 |  4306 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      513 |  4307 |  |
|        - |  4308 | `/*` |
|        - |  4309 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  4310 | ` */` |
|      524 |  4311 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        4 |  4312 |  |
|        - |  4313 | `	SyBlob sOut;` |
|        - |  4314 | `	SyString *pFile;` |
|      528 |  4315 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  4316 | `		return PH7_OK;` |
|        - |  4317 | `	}` |
|      528 |  4318 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  4319 | `		zClass = "Exception";` |
|      ! 0 |  4320 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  4321 | `	}` |
|      528 |  4322 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      506 |  4323 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  4324 | `	}` |
|      528 |  4325 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      528 |  4326 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      528 |  4327 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      528 |  4328 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      528 |  4329 | `	if( zMsg && nMsg > 0 ){` |
|      528 |  4330 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      528 |  4331 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  4332 | `	}` |
|      528 |  4333 | `	if( pFile ){` |
|      528 |  4334 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      528 |  4335 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      528 |  4336 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  4337 | `	}` |
|      528 |  4338 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      528 |  4339 | `	if( pFile ){` |
|      528 |  4340 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      528 |  4341 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      528 |  4342 | `		if( zFuncName && nFuncLen > 0 ){` |
|       28 |  4343 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       16 |  4344 | `		}else{` |
|      504 |  4345 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        4 |  4346 | `		}` |
|      262 |  4347 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  4348 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  4349 | `	}else{` |
|      ! 0 |  4350 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  4351 | `	}` |
|      528 |  4352 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      528 |  4353 | `	if( pFile ){` |
|      528 |  4354 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      528 |  4355 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      528 |  4356 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      528 |  4357 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  4358 | `	}` |
|      528 |  4359 | `	VmCallErrorHandler(pVm,&sOut);` |
|      528 |  4360 | `	SyBlobRelease(&sOut);` |
|      528 |  4361 | `	return PH7_ABORT;` |
|      266 |  4362 |  |
|        - |  4363 | `/*` |
|        - |  4364 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  4365 | ` *` |
|        - |  4366 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  4367 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  4368 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  4369 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  4370 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  4371 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  4372 | ` */` |
|      938 |  4373 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 |  4374 |  |
|      943 |  4375 | `	if( pVm->bCoalesceArmed ){` |
|        8 |  4376 | `		if( pVm->pCoalesceObj ){` |
|        8 |  4377 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4378 | `		}` |
|        8 |  4379 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        8 |  4380 | `		pVm->pCoalesceObj = 0;` |
|        8 |  4381 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4382 | `	}` |
|      943 |  4383 |  |
|        - |  4384 | `/*` |
|        - |  4385 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  4386 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  4387 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  4388 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  4389 | ` *` |
|        - |  4390 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  4391 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  4392 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  4393 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  4394 | ` */` |
|        4 |  4395 | `static sxi32 VmThrowFromVm(` |
|        - |  4396 | `	ph7_vm *pVm,` |
|        - |  4397 | `	const char *zClass,` |
|        - |  4398 | `	const char *zMsg,` |
|        - |  4399 | `	sxu32 nMsg` |
|        2 |  4400 | `){` |
|        - |  4401 | `	ph7_class *pClass;` |
|        - |  4402 | `	ph7_class_instance *pThis;` |
|        - |  4403 | `	ph7_class_method *pCons;` |
|        - |  4404 | `	VmFrame *pFrame;` |
|        - |  4405 | `	sxi32 rc;` |
|        6 |  4406 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        6 |  4407 | `	if( pClass == 0 ){` |
|      ! 0 |  4408 | `		return SXERR_ABORT;` |
|        - |  4409 | `	}` |
|        6 |  4410 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        6 |  4411 | `	if( pThis == 0 ){` |
|      ! 0 |  4412 | `		return SXERR_ABORT;` |
|        - |  4413 | `	}` |
|        6 |  4414 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        6 |  4415 | `	if( pCons ){` |
|        - |  4416 | `		ph7_value sArg;` |
|        - |  4417 | `		ph7_value *apArg[1];` |
|        - |  4418 | `		SyString sMsgStr;` |
|        6 |  4419 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        6 |  4420 | `		PH7_MemObjInit(pVm,&sArg);` |
|        6 |  4421 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        6 |  4422 | `		apArg[0] = &sArg;` |
|        6 |  4423 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        6 |  4424 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  4425 | `	}` |
|        6 |  4426 | `	pFrame = pVm->pFrame;` |
|        6 |  4427 | `	if( pFrame ){` |
|        6 |  4428 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        6 |  4429 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  4430 | `	}` |
|        6 |  4431 | `	rc = VmThrowException(pVm,pThis);` |
|        6 |  4432 | `	PH7_ClassInstanceUnref(pThis);` |
|        6 |  4433 | `	return rc;` |
|        4 |  4434 |  |
|        - |  4435 | `/*` |
|        - |  4436 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  4437 | ` */` |
|      574 |  4438 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 |  4439 |  |
|        - |  4440 | `	ph7_vm *pVm;` |
|        - |  4441 | `	ph7_class *pClass;` |
|        - |  4442 | `	ph7_class_instance *pThis;` |
|        - |  4443 | `	ph7_class_method *pCons;` |
|        - |  4444 | `	ph7_value sArg;` |
|        - |  4445 | `	ph7_value *apArg[1];` |
|        - |  4446 | `	SyBlob sMsg;` |
|        - |  4447 | `	SyString sMsgStr;` |
|        - |  4448 | `	VmFrame *pFrame;` |
|        - |  4449 | `	va_list ap;` |
|        - |  4450 | `	sxi32 rc;` |
|        - |  4451 |  |
|      579 |  4452 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4453 | `		return PH7_ABORT;` |
|        - |  4454 | `	}` |
|      579 |  4455 | `	pVm = pCtx->pVm;` |
|      579 |  4456 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4457 | `		zClass = "Error";` |
|      ! 0 |  4458 | `	}` |
|      579 |  4459 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      579 |  4460 | `	if( pClass == 0 ){` |
|      ! 0 |  4461 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4462 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  4463 | `			zClass` |
|        - |  4464 | `			);` |
|        - |  4465 | `	}` |
|      579 |  4466 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      579 |  4467 | `	if( pThis == 0 ){` |
|      ! 0 |  4468 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4469 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  4470 | `			);` |
|        - |  4471 | `	}` |
|        - |  4472 |  |
|      579 |  4473 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      579 |  4474 | `	va_start(ap,zFormat);` |
|      579 |  4475 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      579 |  4476 | `	va_end(ap);` |
|        - |  4477 |  |
|      579 |  4478 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      579 |  4479 | `	if( pCons ){` |
|      579 |  4480 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      579 |  4481 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      579 |  4482 | `		apArg[0] = &sArg;` |
|      579 |  4483 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      579 |  4484 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  4485 | `	}` |
|      579 |  4486 | `	SyBlobRelease(&sMsg);` |
|        - |  4487 |  |
|      579 |  4488 | `	pFrame = pVm->pFrame;` |
|      579 |  4489 | `	if( pFrame ){` |
|      579 |  4490 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      579 |  4491 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  4492 | `	}` |
|      579 |  4493 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      579 |  4494 | `	PH7_ClassInstanceUnref(pThis);` |
|      579 |  4495 | `	if( rc == SXERR_ABORT ){` |
|      494 |  4496 | `		return PH7_ABORT;` |
|        - |  4497 | `	}` |
|       88 |  4498 | `	return PH7_EXCEPTION;` |
|      292 |  4499 |  |
|        - |  4500 | `/*` |
|        - |  4501 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  4502 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  4503 | ` */` |
|      ! 0 |  4504 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  4505 |  |
|        - |  4506 | `	ph7_vm *pVm;` |
|        - |  4507 | `	SyBlob sMsg;` |
|      ! 0 |  4508 | `	const char *zFuncName = 0;` |
|      ! 0 |  4509 | `	int nFuncLen = 0;` |
|        - |  4510 | `	va_list ap;` |
|        - |  4511 | `	sxi32 rc;` |
|        - |  4512 |  |
|      ! 0 |  4513 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4514 | `		return PH7_OK;` |
|        - |  4515 | `	}` |
|      ! 0 |  4516 | `	pVm = pCtx->pVm;` |
|      ! 0 |  4517 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4518 | `		zClass = "Error";` |
|      ! 0 |  4519 | `	}` |
|        - |  4520 |  |
|      ! 0 |  4521 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  4522 |  |
|      ! 0 |  4523 | `	va_start(ap,zFormat);` |
|      ! 0 |  4524 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  4525 | `	va_end(ap);` |
|        - |  4526 |  |
|      ! 0 |  4527 | `	if( pCtx->pFunc ){` |
|      ! 0 |  4528 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  4529 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  4530 | `	}` |
|      ! 0 |  4531 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  4532 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  4533 | `	}` |
|      ! 0 |  4534 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  4535 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  4536 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  4537 | `	return rc;` |
|      ! 0 |  4538 |  |
|        - |  4539 | `/*` |
|        - |  4540 | ` * Save the execution state of a fiber/generator context.` |
|        - |  4541 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  4542 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  4543 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  4544 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  4545 | ` * when VmByteCodeExec returns.` |
|        - |  4546 | ` */` |
|      200 |  4547 | `static sxi32 VmSuspendCtx(` |
|        - |  4548 | `	ph7_vm *pVm,` |
|        - |  4549 | `	ph7_exec_ctx *pCtx,` |
|        - |  4550 | `	sxi32 pc,` |
|        - |  4551 | `	sxi32 nTos` |
|        - |  4552 | `	)` |
|        5 |  4553 |  |
|      100 |  4554 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      205 |  4555 | `	pCtx->pc = pc;` |
|      205 |  4556 | `	pCtx->nTos = nTos;` |
|      205 |  4557 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      205 |  4558 | `	return PH7_SUSPEND;` |
|        5 |  4559 |  |
|        - |  4560 | `/*` |
|        - |  4561 | ` * Resolve named-argument mapping.` |
|        - |  4562 | ` *` |
|        - |  4563 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  4564 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  4565 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  4566 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  4567 | ` * every formal parameter that received a value.` |
|        - |  4568 | ` *` |
|        - |  4569 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  4570 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  4571 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  4572 | ` */` |
|       98 |  4573 | `static sxi32 VmResolveNamedArgs(` |
|        - |  4574 | `	ph7_vm *pVm,` |
|        - |  4575 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  4576 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  4577 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  4578 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  4579 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  4580 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  4581 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  4582 |  |
|        3 |  4583 |  |
|      101 |  4584 | `	sxi32 posIdx = 0;` |
|        - |  4585 | `	sxu32 i;` |
|        - |  4586 | `	char zErrMsg[256];` |
|      101 |  4587 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      297 |  4588 | `	for( i = 0; i < nActual; i++ ){` |
|      199 |  4589 | `		aSlot[i] = -2;` |
|      101 |  4590 | `	}` |
|      291 |  4591 | `	for( i = 0; i < nActual; i++ ){` |
|      287 |  4592 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  4593 | `			/* Named argument — find formal by name */` |
|      185 |  4594 | `			int found = 0;` |
|        - |  4595 | `			sxu32 k;` |
|      305 |  4596 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  4597 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      282 |  4598 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  4599 | `						pMap->aNames[i].zString,` |
|      402 |  4600 | `						pMap->aNames[i].nByte) == 0 ){` |
|      173 |  4601 | `					if( aUsed[k] ){` |
|        8 |  4602 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4603 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  4604 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        6 |  4605 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        6 |  4606 | `						return PH7_ABORT;` |
|        - |  4607 | `					}` |
|      168 |  4608 | `					aSlot[i] = (sxi32)k;` |
|      168 |  4609 | `					aUsed[k] = 1;` |
|      168 |  4610 | `					found = 1;` |
|      168 |  4611 | `					break;` |
|        - |  4612 | `				}` |
|       62 |  4613 | `			}` |
|      181 |  4614 | `			if( !found ){` |
|       14 |  4615 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  4616 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  4617 | `				}else{` |
|        4 |  4618 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4619 | `						"Unknown named parameter $%.*s",` |
|        2 |  4620 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  4621 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  4622 | `					return PH7_ABORT;` |
|        - |  4623 | `				}` |
|        5 |  4624 | `			}` |
|       90 |  4625 | `		}else{` |
|        - |  4626 | `			/* Positional argument */` |
|       16 |  4627 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  4628 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  4629 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4630 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  4631 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  4632 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4633 | `					return PH7_ABORT;` |
|        - |  4634 | `				}` |
|       16 |  4635 | `				aSlot[i] = posIdx;` |
|       16 |  4636 | `				aUsed[posIdx] = 1;` |
|        7 |  4637 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4638 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4639 | `			}` |
|       16 |  4640 | `			posIdx++;` |
|        - |  4641 | `		}` |
|       98 |  4642 | `	}` |
|       93 |  4643 | `	return SXRET_OK;` |
|       52 |  4644 |  |
|        - |  4645 | `/*` |
|        - |  4646 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - |  4647 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - |  4648 | ` */` |
|       42 |  4649 | `static int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        4 |  4650 |  |
|       46 |  4651 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       33 |  4652 | `		return 0;` |
|        - |  4653 | `	}` |
|       15 |  4654 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       25 |  4655 |  |
|        - |  4656 | `/*` |
|        - |  4657 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - |  4658 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - |  4659 | ` * preserved (later wins), integer keys are renumbered.` |
|        - |  4660 | ` */` |
|       10 |  4661 | `static sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4662 |  |
|       11 |  4663 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 |  4664 | `	(void)pVm;` |
|       11 |  4665 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 |  4666 | `	return SXRET_OK;` |
|        1 |  4667 |  |
|        - |  4668 | `/*` |
|        - |  4669 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - |  4670 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - |  4671 | ` */` |
|        6 |  4672 | `static sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4673 |  |
|        3 |  4674 | `	(void)pVm; (void)pKey;` |
|        7 |  4675 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 |  4676 | `	return SXRET_OK;` |
|        1 |  4677 |  |
|        - |  4678 | `/*` |
|        - |  4679 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4680 | ` *` |
|        - |  4681 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4682 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4683 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4684 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4685 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4686 | ` * then the program execution is halted.` |
|        - |  4687 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4688 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4689 | ` * or to reset the VM to it's initial state.` |
|        - |  4690 | ` */` |
|    49300 |  4691 | `static sxi32 VmByteCodeExec(` |
|        - |  4692 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4693 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4694 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4695 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4696 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4697 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4698 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4699 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4700 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4701 | `	int bReturnPropagates /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value to pVm->sCatchReturn for the enclosing try handler to return. */` |
|        - |  4702 | `	)` |
|        5 |  4703 |  |
|        - |  4704 | `	VmInstr *pInstr;` |
|        - |  4705 | `	ph7_value *pTos;` |
|        - |  4706 | `	SySet aArg;` |
|        - |  4707 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4708 | `	VmFrame *pEntryFrame;  /* Active frame at entry (for return-unwind frame teardown) */` |
|        - |  4709 | `	sxi32 pc;` |
|        - |  4710 | `	sxi32 rc;` |
|        - |  4711 | `	/* Argument container */` |
|    49305 |  4712 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    49305 |  4713 | `	if( nTos < 0 ){` |
|    45653 |  4714 | `		pTos = &pStack[-1];` |
|    22829 |  4715 | `	}else{` |
|     3657 |  4716 | `		pTos = &pStack[nTos];` |
|        - |  4717 | `	}` |
|    49305 |  4718 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    49305 |  4719 | `	pEntryFrame = pVm->pFrame;` |
|    49305 |  4720 | `	pc = nPc;` |
|        - |  4721 | `/*` |
|        - |  4722 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4723 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4724 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4725 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4726 | ` */` |
|        - |  4727 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4728 | `	{ \` |
|        - |  4729 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4730 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4731 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4732 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4733 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4734 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4735 | `				break; \` |
|        - |  4736 | `			} \` |
|        - |  4737 | `			goto Exception; \` |
|        - |  4738 | `		} \` |
|        - |  4739 | `	}` |
|        - |  4740 | `	/* Execute as much as we can */` |
|  5983997 |  4741 | `	for(;;){` |
|        - |  4742 | `		/* Fetch the instruction to execute */` |
| 11967295 |  4743 | `		pInstr = &aInstr[pc];` |
| 11967295 |  4744 | `		rc = SXRET_OK;` |
|        - |  4745 | `/*` |
|        - |  4746 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4747 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4748 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4749 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4750 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4751 | ` */` |
| 11967295 |  4752 | `		switch(pInstr->iOp){` |
|        - |  4753 | `/*` |
|        - |  4754 | ` * DONE: P1 * *` |
|        - |  4755 | ` *` |
|        - |  4756 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4757 | ` * and return immediately.` |
|        - |  4758 | ` */` |
|    24158 |  4759 | `case PH7_OP_DONE:` |
|    48321 |  4760 | `	if( pInstr->iP2 && bReturnPropagates ){` |
|        - |  4761 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - |  4762 | `		 * to pVm->sCatchReturn; the enclosing try's OP_THROW / OP_POP_EXCEPTION` |
|        - |  4763 | `		 * handler materializes it into the function's result and returns. Drain` |
|        - |  4764 | `		 * any finally opened within this body first (nested try/finally inside` |
|        - |  4765 | `		 * the catch), which may itself override sCatchReturn. */` |
|       36 |  4766 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|       34 |  4767 | `			PH7_MemObjStore(pTos,&pVm->sCatchReturn);` |
|       34 |  4768 | `			VmPopOperand(&pTos,1);` |
|       18 |  4769 | `		}else{` |
|        3 |  4770 | ``			PH7_MemObjRelease(&pVm->sCatchReturn); /* bare `return;` -> null */`` |
|        - |  4771 | `		}` |
|       36 |  4772 | `		pVm->bReturnRequested = 1;` |
|       36 |  4773 | `		rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|       36 |  4774 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4775 | `			goto Abort;` |
|        - |  4776 | `		}` |
|       36 |  4777 | `		goto Done;` |
|        - |  4778 | `	}` |
|        - |  4779 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4780 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4781 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4782 | `	 * callback trampolines, and the main script. */` |
|    48282 |  4783 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      485 |  4784 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4785 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4786 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4787 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4788 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4789 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4790 | `		 * exception. */` |
|      479 |  4791 | `		ph7_value *pRetVal = 0;` |
|      479 |  4792 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      329 |  4793 | `			pRetVal = pTos;` |
|      162 |  4794 | `		}` |
|      479 |  4795 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      479 |  4796 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      473 |  4797 | `		if( rc == PH7_EXCEPTION ){` |
|        7 |  4798 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|        7 |  4799 | `				PH7_MemObjRelease(pTos);` |
|        7 |  4800 | `				pTos--;` |
|        3 |  4801 | `			}` |
|        7 |  4802 | `			goto Exception;` |
|        - |  4803 | `		}` |
|        - |  4804 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4805 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4806 | `		 * defensively we clear the pointer after a successful check). */` |
|      467 |  4807 | `		pEnforceRetFunc = 0;` |
|      231 |  4808 | `	}` |
|    48275 |  4809 | `	if( pInstr->iP1 ){` |
|        - |  4810 | `#ifdef UNTRUST` |
|        - |  4811 | `		if( pTos < pStack ){` |
|        - |  4812 | `			goto Abort;` |
|        - |  4813 | `		}` |
|        - |  4814 | `#endif` |
|    30513 |  4815 | `		if( pLastRef ){` |
|    17101 |  4816 | `			*pLastRef = pTos->nIdx;` |
|     8548 |  4817 | `		}` |
|    30513 |  4818 | `		if( pResult ){` |
|        - |  4819 | `			/* Execution result */` |
|    28843 |  4820 | `			PH7_MemObjStore(pTos,pResult);` |
|    14419 |  4821 | `		}` |
|    30513 |  4822 | `		VmPopOperand(&pTos,1);` |
|    33021 |  4823 | `	}else if( pLastRef ){` |
|        - |  4824 | `		/* Nothing referenced */` |
|     2095 |  4825 | `		*pLastRef = SXU32_HIGH;` |
|     1045 |  4826 | `	}` |
|        - |  4827 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4828 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4829 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4830 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4831 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4832 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4833 | `	 * block can override it (the finally writes pVm->sCatchReturn, materialized` |
|        - |  4834 | `	 * below).` |
|        - |  4835 | `	 */` |
|    48275 |  4836 | `	rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|    48275 |  4837 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4838 | `		goto Abort;` |
|        - |  4839 | `	}` |
|    48275 |  4840 | `	if( pVm->bReturnRequested && !bReturnPropagates ){` |
|        - |  4841 | `		/* A drained finally issued a 'return' that overrides this one. */` |
|        8 |  4842 | `		VmMaterializeCatchReturn(&(*pVm),pResult,pEntryFrame);` |
|        3 |  4843 | `	}` |
|    48275 |  4844 | `	goto Done;` |
|        - |  4845 | `/*` |
|        - |  4846 | ` * HALT: P1 * *` |
|        - |  4847 | ` *` |
|        - |  4848 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4849 | ` * and abort immediately.` |
|        - |  4850 | ` */` |
|        7 |  4851 | `case PH7_OP_HALT:` |
|       18 |  4852 | `	if( pInstr->iP1 ){` |
|        - |  4853 | `#ifdef UNTRUST` |
|        - |  4854 | `		if( pTos < pStack ){` |
|        - |  4855 | `			goto Abort;` |
|        - |  4856 | `		}` |
|        - |  4857 | `#endif` |
|       18 |  4858 | `		if( pLastRef ){` |
|        3 |  4859 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4860 | `		}` |
|       18 |  4861 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       13 |  4862 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4863 | `				/* Output the exit message */` |
|       18 |  4864 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4865 | `					pVm->sVmConsumer.pUserData);` |
|       13 |  4866 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        8 |  4867 | `			}` |
|       11 |  4868 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4869 | `			/* Record exit status */` |
|        6 |  4870 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4871 | `		}` |
|       18 |  4872 | `		VmPopOperand(&pTos,1);` |
|        7 |  4873 | `	}else if( pLastRef ){` |
|        - |  4874 | `		/* Nothing referenced */` |
|      ! 0 |  4875 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4876 | `	}` |
|        - |  4877 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4878 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4879 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4880 | `	 */` |
|       18 |  4881 | `	pVm->bHaltRequested = 1;` |
|       18 |  4882 | `	goto Abort;` |
|        - |  4883 | `/*` |
|        - |  4884 | ` * JMP: * P2 *` |
|        - |  4885 | ` *` |
|        - |  4886 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4887 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4888 | ` */` |
|   254574 |  4889 | `case PH7_OP_JMP:` |
|   509197 |  4890 | `	pc = pInstr->iP2 - 1;` |
|   509197 |  4891 | `	break;` |
|        - |  4892 | `/*` |
|        - |  4893 | ` * JZ: P1 P2 *` |
|        - |  4894 | ` *` |
|        - |  4895 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4896 | ` * entry in the stack if P1 is zero.` |
|        - |  4897 | ` */` |
|   605120 |  4898 | `case PH7_OP_JZ:` |
|        - |  4899 | `#ifdef UNTRUST` |
|        - |  4900 | `	if( pTos < pStack ){` |
|        - |  4901 | `		goto Abort;` |
|        - |  4902 | `	}` |
|        - |  4903 | `#endif` |
|        - |  4904 | `	/* Get a boolean value */` |
|  1210333 |  4905 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      175 |  4906 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4907 | `	}` |
|  1210333 |  4908 | `	if( !pTos->x.iVal ){` |
|        - |  4909 | `		/* Take the jump */` |
|   623465 |  4910 | `		pc = pInstr->iP2 - 1;` |
|   311730 |  4911 | `	}` |
|  1210333 |  4912 | `	if( !pInstr->iP1 ){` |
|   958593 |  4913 | `		VmPopOperand(&pTos,1);` |
|   479316 |  4914 | `	}` |
|  1210333 |  4915 | `	break;` |
|        - |  4916 | `/*` |
|        - |  4917 | ` * JNZ: P1 P2 *` |
|        - |  4918 | ` *` |
|        - |  4919 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4920 | ` * entry in the stack if P1 is zero.` |
|        - |  4921 | ` */` |
|    61928 |  4922 | `case PH7_OP_JNZ:` |
|        - |  4923 | `#ifdef UNTRUST` |
|        - |  4924 | `	if( pTos < pStack ){` |
|        - |  4925 | `		goto Abort;` |
|        - |  4926 | `	}` |
|        - |  4927 | `#endif` |
|        - |  4928 | `	/* Get a boolean value */` |
|   123861 |  4929 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4930 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4931 | `	}` |
|   123861 |  4932 | `	if( pTos->x.iVal ){` |
|        - |  4933 | `		/* Take the jump */` |
|     5693 |  4934 | `		pc = pInstr->iP2 - 1;` |
|     2844 |  4935 | `	}` |
|   123861 |  4936 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4937 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4938 | `	}` |
|   123861 |  4939 | `	break;` |
|        - |  4940 | `/*` |
|        - |  4941 | ` * NOOP: * * *` |
|        - |  4942 | ` *` |
|        - |  4943 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4944 | ` * destination.` |
|        - |  4945 | ` */` |
|      ! 0 |  4946 | `case PH7_OP_NOOP:` |
|      ! 0 |  4947 | `	break;` |
|        - |  4948 | `/*` |
|        - |  4949 | ` * POP: P1 * *` |
|        - |  4950 | ` *` |
|        - |  4951 | ` * Pop P1 elements from the operand stack.` |
|        - |  4952 | ` */` |
|   468813 |  4953 | `case PH7_OP_POP: {` |
|   937675 |  4954 | `	sxi32 n = pInstr->iP1;` |
|   937675 |  4955 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4956 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       53 |  4957 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4958 | `	}` |
|   937675 |  4959 | `	VmPopOperand(&pTos,n);` |
|   937675 |  4960 | `	break;` |
|        - |  4961 | `				 }` |
|        - |  4962 | `/*` |
|        - |  4963 | ` * DUP: * * *` |
|        - |  4964 | ` *` |
|        - |  4965 | ` * Duplicate the top of the stack.` |
|        - |  4966 | ` */` |
|       41 |  4967 | `case PH7_OP_DUP:` |
|        - |  4968 | `#ifdef UNTRUST` |
|        - |  4969 | `	if( pTos < pStack ){` |
|        - |  4970 | `		goto Abort;` |
|        - |  4971 | `	}` |
|        - |  4972 | `#endif` |
|       84 |  4973 | `	pTos++;` |
|       84 |  4974 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4975 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4976 | `	break;` |
|        - |  4977 | `/*` |
|        - |  4978 | ` * NSSWITCH: * * P3` |
|        - |  4979 | ` *` |
|        - |  4980 | ` * Switch the active namespace at runtime.` |
|        - |  4981 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4982 | ` */` |
|     7948 |  4983 | `case PH7_OP_NSSWITCH:` |
|    15901 |  4984 | `	SyBlobReset(&pVm->sNamespace);` |
|    15901 |  4985 | `	if( pInstr->p3 ){` |
|      103 |  4986 | `		const char *zNs = (const char *)pInstr->p3;` |
|      103 |  4987 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4988 | `	}` |
|        - |  4989 | `	/* Clear namespace-scoped use-const imports */` |
|    15901 |  4990 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15901 |  4991 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15901 |  4992 | `	break;` |
|        - |  4993 | `/* OP_USECONST P1 * P3` |
|        - |  4994 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4995 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4996 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4997 | ` */` |
|        7 |  4998 | `case PH7_OP_USECONST: {` |
|       16 |  4999 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  5000 | `	if( azPair ){` |
|       16 |  5001 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  5002 | `	}` |
|       16 |  5003 | `	break;` |
|        - |  5004 | `				}` |
|        - |  5005 | `/*` |
|        - |  5006 | ` * CVT_INT: * * *` |
|        - |  5007 | ` *` |
|        - |  5008 | ` * Force the top of the stack to be an integer.` |
|        - |  5009 | ` */` |
|       80 |  5010 | `case PH7_OP_CVT_INT:` |
|        - |  5011 | `#ifdef UNTRUST` |
|        - |  5012 | `	if( pTos < pStack ){` |
|        - |  5013 | `		goto Abort;` |
|        - |  5014 | `	}` |
|        - |  5015 | `#endif` |
|      165 |  5016 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      115 |  5017 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  5018 | `	}` |
|        - |  5019 | `	/* Invalidate any prior representation */` |
|      165 |  5020 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      165 |  5021 | `	break;` |
|        - |  5022 | `/*` |
|        - |  5023 | ` * CVT_REAL: * * *` |
|        - |  5024 | ` *` |
|        - |  5025 | ` * Force the top of the stack to be a real.` |
|        - |  5026 | ` */` |
|        7 |  5027 | `case PH7_OP_CVT_REAL:` |
|        - |  5028 | `#ifdef UNTRUST` |
|        - |  5029 | `	if( pTos < pStack ){` |
|        - |  5030 | `		goto Abort;` |
|        - |  5031 | `	}` |
|        - |  5032 | `#endif` |
|       15 |  5033 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 |  5034 | `		PH7_MemObjToReal(pTos);` |
|        5 |  5035 | `	}` |
|        - |  5036 | `	/* Invalidate any prior representation */` |
|       15 |  5037 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       15 |  5038 | `	break;` |
|        - |  5039 | `/*` |
|        - |  5040 | ` * CVT_STR: * * *` |
|        - |  5041 | ` *` |
|        - |  5042 | ` * Force the top of the stack to be a string.` |
|        - |  5043 | ` */` |
|      163 |  5044 | `case PH7_OP_CVT_STR:` |
|        - |  5045 | `#ifdef UNTRUST` |
|        - |  5046 | `	if( pTos < pStack ){` |
|        - |  5047 | `		goto Abort;` |
|        - |  5048 | `	}` |
|        - |  5049 | `#endif` |
|      330 |  5050 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      310 |  5051 | `		PH7_MemObjToString(pTos);` |
|      153 |  5052 | `	}` |
|      330 |  5053 | `	break;` |
|        - |  5054 | `/*` |
|        - |  5055 | ` * CVT_BOOL: * * *` |
|        - |  5056 | ` *` |
|        - |  5057 | ` * Force the top of the stack to be a boolean.` |
|        - |  5058 | ` */` |
|        5 |  5059 | `case PH7_OP_CVT_BOOL:` |
|        - |  5060 | `#ifdef UNTRUST` |
|        - |  5061 | `	if( pTos < pStack ){` |
|        - |  5062 | `		goto Abort;` |
|        - |  5063 | `	}` |
|        - |  5064 | `#endif` |
|       11 |  5065 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  5066 | `		PH7_MemObjToBool(pTos);` |
|        3 |  5067 | `	}` |
|       11 |  5068 | `	break;` |
|        - |  5069 | `/*` |
|        - |  5070 | ` * CVT_NULL: * * *` |
|        - |  5071 | ` *` |
|        - |  5072 | ` * Nullify the top of the stack.` |
|        - |  5073 | ` */` |
|        3 |  5074 | `case PH7_OP_CVT_NULL:` |
|        - |  5075 | `#ifdef UNTRUST` |
|        - |  5076 | `	if( pTos < pStack ){` |
|        - |  5077 | `		goto Abort;` |
|        - |  5078 | `	}` |
|        - |  5079 | `#endif` |
|        7 |  5080 | `	PH7_MemObjRelease(pTos);` |
|        7 |  5081 | `	break;` |
|        - |  5082 | `/*` |
|        - |  5083 | ` * CVT_NUMC: * * *` |
|        - |  5084 | ` *` |
|        - |  5085 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  5086 | ` */` |
|      ! 0 |  5087 | `case PH7_OP_CVT_NUMC:` |
|        - |  5088 | `#ifdef UNTRUST` |
|        - |  5089 | `	if( pTos < pStack ){` |
|        - |  5090 | `		goto Abort;` |
|        - |  5091 | `	}` |
|        - |  5092 | `#endif` |
|        - |  5093 | `	/* Force a numeric cast */` |
|      ! 0 |  5094 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  5095 | `	break;` |
|        - |  5096 | `/*` |
|        - |  5097 | ` * CVT_ARRAY: * * *` |
|        - |  5098 | ` *` |
|        - |  5099 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  5100 | ` */` |
|       10 |  5101 | `case PH7_OP_CVT_ARRAY:` |
|        - |  5102 | `#ifdef UNTRUST` |
|        - |  5103 | `	if( pTos < pStack ){` |
|        - |  5104 | `		goto Abort;` |
|        - |  5105 | `	}` |
|        - |  5106 | `#endif` |
|        - |  5107 | `	/* Force a hashmap cast */` |
|       21 |  5108 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  5109 | `	if( rc != SXRET_OK ){` |
|        - |  5110 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  5111 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5112 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  5113 | `	}` |
|       21 |  5114 | `	break;` |
|        - |  5115 | `/*` |
|        - |  5116 | ` * CVT_OBJ: * * *` |
|        - |  5117 | ` *` |
|        - |  5118 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  5119 | ` */` |
|        8 |  5120 | `case PH7_OP_CVT_OBJ:` |
|        - |  5121 | `#ifdef UNTRUST` |
|        - |  5122 | `	if( pTos < pStack ){` |
|        - |  5123 | `		goto Abort;` |
|        - |  5124 | `	}` |
|        - |  5125 | `#endif` |
|       17 |  5126 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  5127 | `		/* Force a 'stdClass()' cast */` |
|       17 |  5128 | `		PH7_MemObjToObject(pTos);` |
|        8 |  5129 | `	}` |
|       17 |  5130 | `	break;` |
|        - |  5131 | `/*` |
|        - |  5132 | ` * ERR_CTRL * * *` |
|        - |  5133 | ` *` |
|        - |  5134 | ` * Error control operator.` |
|        - |  5135 | ` */` |
|    16337 |  5136 | `case PH7_OP_ERR_CTRL:` |
|        - |  5137 | `	/*` |
|        - |  5138 | `	 * TICKET 1433-038:` |
|        - |  5139 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  5140 | `	 * use the public API,to control error output.` |
|        - |  5141 | `	 */` |
|    32674 |  5142 | `	break;` |
|        - |  5143 | `/*` |
|        - |  5144 | ` * IS_A * * *` |
|        - |  5145 | ` *` |
|        - |  5146 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  5147 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  5148 | ` * holding a class name or an object).` |
|        - |  5149 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  5150 | ` */` |
|       77 |  5151 | `case PH7_OP_IS_A:{` |
|      159 |  5152 | `	ph7_value *pNos = &pTos[-1];` |
|      159 |  5153 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  5154 | `#ifdef UNTRUST` |
|        - |  5155 | `	if( pNos < pStack ){` |
|        - |  5156 | `		goto Abort;` |
|        - |  5157 | `	}` |
|        - |  5158 | `#endif` |
|      159 |  5159 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      157 |  5160 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      157 |  5161 | `		ph7_class *pClass = 0;` |
|        - |  5162 | `		/* Extract the target class */` |
|      157 |  5163 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5164 | `			/* Instance already loaded */` |
|      ! 0 |  5165 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      157 |  5166 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      157 |  5167 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      157 |  5168 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  5169 | `			/* Handle self/static/parent keywords */` |
|      157 |  5170 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 |  5171 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      155 |  5172 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  5173 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      154 |  5174 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 |  5175 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        6 |  5176 | `				if( pSelf && pSelf->pBase ){` |
|        6 |  5177 | `					pClass = pSelf->pBase;` |
|        2 |  5178 | `				}` |
|        4 |  5179 | `			}else{` |
|      147 |  5180 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5181 | `			}` |
|       76 |  5182 | `		}` |
|      157 |  5183 | `		if( pClass ){` |
|        - |  5184 | `			/* Perform the query */` |
|      157 |  5185 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       76 |  5186 | `		}` |
|       76 |  5187 | `	}` |
|        - |  5188 | `	/* Push result */` |
|      159 |  5189 | `	VmPopOperand(&pTos,1);` |
|      159 |  5190 | `	PH7_MemObjRelease(pTos);` |
|      159 |  5191 | `	pTos->x.iVal = iRes;` |
|      159 |  5192 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      159 |  5193 | `	break;` |
|        - |  5194 | `				 }` |
|        - |  5195 |  |
|        - |  5196 | `/*` |
|        - |  5197 | ` * LOADC P1 P2 *` |
|        - |  5198 | ` *` |
|        - |  5199 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  5200 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  5201 | ` */` |
|  1032955 |  5202 | `case PH7_OP_LOADC: {` |
|        - |  5203 | `	ph7_value *pObj;` |
|        - |  5204 | `	/* Reserve a room */` |
|  2065959 |  5205 | `	pTos++;` |
|  3088928 |  5206 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2065959 |  5207 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  5208 | `			SyHashEntry *pEntry;` |
|        - |  5209 | `			/* Check use const imports first — imports take precedence */` |
|        - |  5210 | `			{` |
|        - |  5211 | `				SyHashEntry *pConstImport;` |
|    30101 |  5212 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    20064 |  5213 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    20069 |  5214 | `				if( pConstImport ){` |
|       11 |  5215 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  5216 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  5217 | `					if( pEntry ){` |
|       11 |  5218 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  5219 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  5220 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  5221 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  5222 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  5223 | `						break;` |
|        - |  5224 | `					}` |
|        - |  5225 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  5226 | `				}` |
|        - |  5227 | `			}` |
|        - |  5228 | `			/* Candidate for expansion via user defined callbacks */` |
|    20059 |  5229 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    20059 |  5230 | `			if( pEntry ){` |
|    20053 |  5231 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  5232 | `				/* Set a NULL default value */` |
|    20053 |  5233 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    20053 |  5234 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  5235 | `				/* Invoke the callback and deal with the expanded value */` |
|    20053 |  5236 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  5237 | `				/* Mark as constant */` |
|    20053 |  5238 | `				pTos->nIdx = SXU32_HIGH;` |
|    20053 |  5239 | `				break;` |
|        - |  5240 | `			}` |
|        - |  5241 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  5242 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  5243 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  5244 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  5245 | `			{` |
|        9 |  5246 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        9 |  5247 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  5248 | `				sxu32 j;` |
|        9 |  5249 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       25 |  5250 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  5251 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  5252 | `				}` |
|        9 |  5253 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  5254 | `					/* Try current_namespace\name */` |
|      ! 0 |  5255 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  5256 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  5257 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  5258 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  5259 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  5260 | `					if( pEntry ){` |
|      ! 0 |  5261 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  5262 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5263 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  5264 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  5265 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5266 | `						break;` |
|        - |  5267 | `					}` |
|        - |  5268 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  5269 | `				}` |
|        9 |  5270 | `				if( isQualified ){` |
|        - |  5271 | `					/* Qualified name: must be a real constant. */` |
|        3 |  5272 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  5273 | `					SyBlob sErr;` |
|        3 |  5274 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  5275 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  5276 | `					if( pErrFile ){` |
|        3 |  5277 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  5278 | `					}` |
|        3 |  5279 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  5280 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  5281 | `					SyBlobRelease(&sErr);` |
|        3 |  5282 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  5283 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  5284 | `					goto LoadC_Done;` |
|        - |  5285 | `				}` |
|        - |  5286 | `			}` |
|        2 |  5287 | `		}` |
|  2045899 |  5288 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1022974 |  5289 | `	}else{` |
|        - |  5290 | `		/* Set a NULL value */` |
|      ! 0 |  5291 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5292 | `	}` |
|  1022926 |  5293 | `LoadC_Done:` |
|        - |  5294 | `	/* Mark as constant */` |
|  2045901 |  5295 | `	pTos->nIdx = SXU32_HIGH;` |
|  2045901 |  5296 | `	break;` |
|        - |  5297 | `				  }` |
|        - |  5298 | `/*` |
|        - |  5299 | ` * LOAD: P1 * P3` |
|        - |  5300 | ` *` |
|        - |  5301 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  5302 | ` * from the P3 operand.` |
|        - |  5303 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  5304 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  5305 | ` */` |
|  1594640 |  5306 | `case PH7_OP_LOAD:{` |
|        - |  5307 | `	ph7_value *pObj;` |
|        - |  5308 | `	SyString sName;` |
|  3189505 |  5309 | `	if( pInstr->p3 == 0 ){` |
|        - |  5310 | `		/* Take the variable name from the top of the stack */` |
|        - |  5311 | `#ifdef UNTRUST` |
|        - |  5312 | `		if( pTos < pStack ){` |
|        - |  5313 | `			goto Abort;` |
|        - |  5314 | `		}` |
|        - |  5315 | `#endif` |
|        - |  5316 | `		/* Force a string cast */` |
|       19 |  5317 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5318 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5319 | `		}` |
|       19 |  5320 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  5321 | `	}else{` |
|  3189487 |  5322 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5323 | `		/* Reserve a room for the target object */` |
|  3189487 |  5324 | `		pTos++;` |
|        - |  5325 | `	}` |
|        - |  5326 | `	/* Extract the requested memory object */` |
|  3189505 |  5327 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3189505 |  5328 | `	if( pObj == 0 ){` |
|      859 |  5329 | `		if( pInstr->iP1 ){` |
|        - |  5330 | `			/* Variable not found,load NULL */` |
|      859 |  5331 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5332 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5333 | `			}else{` |
|      859 |  5334 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5335 | `			}` |
|      859 |  5336 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1595072 |  5337 | `			break;` |
|      ! 0 |  5338 | `		}else{` |
|        - |  5339 | `			/* Fatal error */` |
|      ! 0 |  5340 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5341 | `			goto Abort;` |
|        - |  5342 | `		}` |
|        - |  5343 | `	}` |
|        - |  5344 | `	/* Load variable contents */` |
|  3188651 |  5345 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3188651 |  5346 | `	pTos->nIdx = pObj->nIdx;` |
|  3188651 |  5347 | `	break;` |
|        - |  5348 | `				   }` |
|        - |  5349 | `/*` |
|        - |  5350 | ` * LOAD_MAP P1 * *` |
|        - |  5351 | ` *` |
|        - |  5352 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  5353 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  5354 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  5355 | ` */` |
|    23162 |  5356 | `case PH7_OP_LOAD_MAP: {` |
|        - |  5357 | `	ph7_hashmap *pMap;` |
|        - |  5358 | `	/* Allocate a new hashmap instance */` |
|    46329 |  5359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    46329 |  5360 | `	if( pMap == 0 ){` |
|      ! 0 |  5361 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5362 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  5363 | `		goto Abort;` |
|        - |  5364 | `	}` |
|    46329 |  5365 | `	if( pInstr->iP1 > 0 ){` |
|     2815 |  5366 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2815 |  5367 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  5368 | `		/* Perform the insertion */` |
|     8585 |  5369 | `		while( pEntry < pTos ){` |
|     5793 |  5370 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  5371 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  5372 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  5373 | `				 * renumbered. Same routine that backs array_merge. */` |
|       77 |  5374 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  5375 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  5376 | `					if( rcMerge != SXRET_OK ){` |
|        - |  5377 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  5378 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  5379 | `						 * map dangling. */` |
|      ! 0 |  5380 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5381 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  5382 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  5383 | `						break;` |
|        1 |  5384 | `					}` |
|       51 |  5385 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|        - |  5386 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|        - |  5387 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|        5 |  5388 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|        5 |  5389 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  5390 | `						rcSpread = rcW;` |
|      ! 0 |  5391 | `						break;` |
|        - |  5392 | `					}` |
|        3 |  5393 | `				}else{` |
|        - |  5394 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       21 |  5395 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       21 |  5396 | `					break;` |
|        1 |  5397 | `				}` |
|     5747 |  5398 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5399 | `				/* Insertion by reference */` |
|      151 |  5400 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5401 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5402 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5403 | `					);` |
|       51 |  5404 | `			}else{` |
|        - |  5405 | `				/* Standard insertion */` |
|     8426 |  5406 | `				PH7_HashmapInsert(pMap,` |
|     5614 |  5407 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2807 |  5408 | `					&pEntry[1]` |
|        - |  5409 | `				);` |
|        - |  5410 | `			}` |
|        - |  5411 | `			/* Next pair on the stack */` |
|     5775 |  5412 | `			pEntry += 2;` |
|        5 |  5413 | `		}` |
|        - |  5414 | `		/* Pop P1 elements */` |
|     2815 |  5415 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2815 |  5416 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5417 | `			/* Discard the partially-built map and propagate the exception. */` |
|       21 |  5418 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       21 |  5419 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5420 | `				goto Abort;` |
|        - |  5421 | `			}` |
|        - |  5422 | `			{` |
|       21 |  5423 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       21 |  5424 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        6 |  5425 | `					pc = pFrm2->iExceptionJump - 1;` |
|        6 |  5426 | `					break;` |
|        - |  5427 | `				}` |
|        - |  5428 | `			}` |
|       15 |  5429 | `			goto Exception;` |
|        - |  5430 | `		}` |
|     1396 |  5431 | `	}` |
|        - |  5432 | `	/* Push the hashmap */` |
|    46311 |  5433 | `	pTos++;` |
|    46311 |  5434 | `	pTos->nIdx = SXU32_HIGH;` |
|    46311 |  5435 | `	pTos->x.pOther = pMap;` |
|    46311 |  5436 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    46311 |  5437 | `	break;` |
|        - |  5438 | `					  }` |
|        - |  5439 | `/*` |
|        - |  5440 | ` * LOAD_LIST: P1 * *` |
|        - |  5441 | ` *` |
|        - |  5442 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5443 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5444 | ` * Caveats:` |
|        - |  5445 | ` *  This implementation support only a single nesting level.` |
|        - |  5446 | ` */` |
|       48 |  5447 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5448 | `	ph7_value *pEntry;` |
|       98 |  5449 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5450 | `		/* Empty list,break immediately */` |
|      ! 0 |  5451 | `		break;` |
|        - |  5452 | `	}` |
|       98 |  5453 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5454 | `#ifdef UNTRUST` |
|        - |  5455 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5456 | `		goto Abort;` |
|        - |  5457 | `	}` |
|        - |  5458 | `#endif` |
|       98 |  5459 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5460 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5461 | `		ph7_hashmap_node *pNode;` |
|        - |  5462 | `		ph7_value sKey,*pObj;` |
|        - |  5463 | `		/* Start Copying */` |
|       91 |  5464 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5465 | `		while( pEntry <= pTos ){` |
|      193 |  5466 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5467 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5468 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5469 | `					if( rc == SXRET_OK ){` |
|        - |  5470 | `						/* Store node value */` |
|      165 |  5471 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5472 | `					}else{` |
|        - |  5473 | `						/* Undefined array key */` |
|        - |  5474 | `						char zMsg[128];` |
|      ! 0 |  5475 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5476 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5477 | `						PH7_MemObjRelease(pObj);` |
|        - |  5478 | `					}` |
|       82 |  5479 | `				}` |
|       82 |  5480 | `			}` |
|      193 |  5481 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5482 | `			pEntry++;` |
|        1 |  5483 | `		}` |
|       46 |  5484 | `	}else{` |
|        - |  5485 | `		/* Source is not an array */` |
|        - |  5486 | `		ph7_value *pObj;` |
|       18 |  5487 | `		while( pEntry <= pTos ){` |
|       12 |  5488 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5489 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5490 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5491 | `				}` |
|        5 |  5492 | `			}` |
|       12 |  5493 | `			pEntry++;` |
|        2 |  5494 | `		}` |
|        8 |  5495 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5496 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5497 | `			const char *zType = "unknown";` |
|        3 |  5498 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5499 | `			char zMsg[256];` |
|        3 |  5500 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5501 | `				zType = "string";` |
|        1 |  5502 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5503 | `				zType = "int";` |
|      ! 0 |  5504 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5505 | `				zType = "float";` |
|      ! 0 |  5506 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5507 | `				zType = "object";` |
|      ! 0 |  5508 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5509 | `				zType = "resource";` |
|      ! 0 |  5510 | `			}` |
|        3 |  5511 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5512 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5513 | `		}` |
|        - |  5514 | `	}` |
|       98 |  5515 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5516 | `	break;` |
|        - |  5517 | `					   }` |
|        - |  5518 | `/*` |
|        - |  5519 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5520 | ` *` |
|        - |  5521 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5522 | ` * from the stack.` |
|        - |  5523 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5524 | ` * instead.` |
|        - |  5525 | ` */` |
|   253200 |  5526 | `case PH7_OP_LOAD_IDX: {` |
|   506449 |  5527 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   506449 |  5528 | `	ph7_hashmap *pMap = 0;` |
|        - |  5529 | `	ph7_value *pIdx;` |
|   506449 |  5530 | `	pIdx = 0;` |
|   506449 |  5531 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5532 | `		if( !pInstr->iP2){` |
|        - |  5533 | `			/* No available index,load NULL */` |
|      ! 0 |  5534 | `			if( pTos >= pStack ){` |
|      ! 0 |  5535 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5536 | `			}else{` |
|        - |  5537 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5538 | `				pTos++;` |
|      ! 0 |  5539 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5540 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5541 | `			}` |
|        - |  5542 | `			/* Emit a notice */` |
|      ! 0 |  5543 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5544 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5545 | `			break;` |
|        - |  5546 | `		}` |
|      ! 0 |  5547 | `	}else{` |
|   506449 |  5548 | `		pIdx = pTos;` |
|   506449 |  5549 | `		pTos--;` |
|        - |  5550 | `	}` |
|   506449 |  5551 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5552 | `		/* String access */` |
|   390869 |  5553 | `		if( pIdx ){` |
|        - |  5554 | `			sxu32 nOfft;` |
|   390869 |  5555 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5556 | `				/* Force an int cast */` |
|      ! 0 |  5557 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5558 | `			}` |
|   390869 |  5559 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   390869 |  5560 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5561 | `				/* Invalid offset,load null */` |
|      ! 0 |  5562 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5563 | `			}else{` |
|   390869 |  5564 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   390869 |  5565 | `				int c = zData[nOfft];` |
|   390869 |  5566 | `				PH7_MemObjRelease(pTos);` |
|   390869 |  5567 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   390869 |  5568 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5569 | `			}` |
|   195459 |  5570 | `		}else{` |
|        - |  5571 | `			/* No available index,load NULL */` |
|      ! 0 |  5572 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5573 | `		}` |
|   390869 |  5574 | `		break;` |
|        - |  5575 | `	}` |
|   115585 |  5576 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5577 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5578 | `		 * iP2 codes:` |
|        - |  5579 | `		 *   0 = read       → offsetGet` |
|        - |  5580 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5581 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5582 | `		 *   4 = isset()    → offsetExists` |
|        - |  5583 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5584 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      129 |  5585 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      129 |  5586 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      129 |  5587 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5588 | `			ph7_class_method *pMeth;` |
|        - |  5589 | `			ph7_value sResult;` |
|        - |  5590 | `			ph7_value *apArg[1];` |
|      127 |  5591 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5592 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5593 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5594 | `					"Cannot use [] for reading");` |
|      ! 0 |  5595 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5596 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5597 | `				break;` |
|        - |  5598 | `			}` |
|      127 |  5599 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      127 |  5600 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5601 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       54 |  5602 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5603 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       54 |  5604 | `				apArg[0] = pIdx;` |
|       54 |  5605 | `				if( pMeth ){` |
|       54 |  5606 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       29 |  5607 | `				}` |
|      102 |  5608 | `			}else if( pInstr->iP2 == 5 ){` |
|       11 |  5609 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5610 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|       11 |  5611 | `				apArg[0] = pIdx;` |
|       11 |  5612 | `				if( pMeth ){` |
|       11 |  5613 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5614 | `				}` |
|        7 |  5615 | `			}else{` |
|       69 |  5616 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5617 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       69 |  5618 | `				apArg[0] = pIdx;` |
|       69 |  5619 | `				if( pMeth ){` |
|       69 |  5620 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5621 | `				}` |
|        - |  5622 | `			}` |
|      127 |  5623 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5624 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5625 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5626 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       36 |  5627 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       36 |  5628 | `				PH7_MemObjRelease(pTos);` |
|       36 |  5629 | `				pTos->nIdx = SXU32_HIGH;` |
|       36 |  5630 | `				if( bExists ){` |
|       19 |  5631 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       19 |  5632 | `					pTos->x.iVal = 1;` |
|       11 |  5633 | `				}else{` |
|       20 |  5634 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        4 |  5635 | `				}` |
|      111 |  5636 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5637 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5638 | `				 * vm_builtin_unset is a harmless no-op. */` |
|       11 |  5639 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5640 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5641 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       91 |  5642 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5643 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5644 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5645 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5646 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5647 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5648 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5649 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5650 | `				if( !bExists ){` |
|        3 |  5651 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5652 | `				}else{` |
|        9 |  5653 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5654 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5655 | `					ph7_value sValue;` |
|        9 |  5656 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5657 | `					apArg[0] = pIdx;` |
|        9 |  5658 | `					if( pGet ){` |
|        9 |  5659 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5660 | `					}` |
|        9 |  5661 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5662 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5663 | `				}` |
|       11 |  5664 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5665 | `				break; /* skip the duplicate sResult release below */` |
|       77 |  5666 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5667 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5668 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5669 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5670 | `				 *     and push NULL.` |
|        - |  5671 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|       10 |  5672 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       10 |  5673 | `				int bShouldArm = !bExists;` |
|        - |  5674 | `				ph7_value sValue;` |
|       10 |  5675 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5676 | `				/* Reset any prior arming defensively */` |
|       10 |  5677 | `				VmCoalesceDisarm(pVm);` |
|       10 |  5678 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|       10 |  5679 | `				if( bExists ){` |
|        5 |  5680 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5681 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5682 | `					apArg[0] = pIdx;` |
|        5 |  5683 | `					if( pGet ){` |
|        5 |  5684 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5685 | `					}` |
|        5 |  5686 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5687 | `						bShouldArm = 1;` |
|        1 |  5688 | `					}` |
|        2 |  5689 | `				}` |
|       10 |  5690 | `				PH7_MemObjRelease(pTos);` |
|       10 |  5691 | `				pTos->nIdx = SXU32_HIGH;` |
|       10 |  5692 | `				if( bShouldArm ){` |
|        - |  5693 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5694 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5695 | `					 * intervening expression evaluation. */` |
|        8 |  5696 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        8 |  5697 | `					if( pIdx ){` |
|        8 |  5698 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5699 | `					}` |
|        8 |  5700 | `					pVm->pCoalesceObj = pInst;` |
|        8 |  5701 | `					pInst->iRef++;` |
|        8 |  5702 | `					pVm->bCoalesceArmed = 1;` |
|        5 |  5703 | `				}else{` |
|        3 |  5704 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5705 | `				}` |
|       10 |  5706 | `				PH7_MemObjRelease(&sValue);` |
|       10 |  5707 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       10 |  5708 | `				break;` |
|      ! 0 |  5709 | `			}else{` |
|        - |  5710 | `				/* offsetGet: replace pTos with the returned value. */` |
|       69 |  5711 | `				PH7_MemObjRelease(pTos);` |
|       69 |  5712 | `				PH7_MemObjStore(&sResult,pTos);` |
|       69 |  5713 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5714 | `			}` |
|      109 |  5715 | `			PH7_MemObjRelease(&sResult);` |
|      109 |  5716 | `			if( pIdx ){` |
|      109 |  5717 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5718 | `			}` |
|      109 |  5719 | `			break;` |
|        - |  5720 | `		}` |
|        - |  5721 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5722 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5723 | `		if( pInst ){` |
|        - |  5724 | `			char zMsg[256];` |
|        3 |  5725 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5726 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5727 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5728 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5729 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5730 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5731 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5732 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5733 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5734 | `			break;` |
|        - |  5735 | `		}` |
|      ! 0 |  5736 | `	}` |
|   115461 |  5737 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5738 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5739 | `			ph7_value *pObj;` |
|        3 |  5740 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5741 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5742 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5743 | `			}` |
|        1 |  5744 | `		}` |
|        1 |  5745 | `	}` |
|   115461 |  5746 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   115461 |  5747 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   115461 |  5748 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5749 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5750 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5751 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5752 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5753 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5754 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      898 |  5755 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5756 | `		}` |
|        - |  5757 | `		/* Point to the hashmap */` |
|   115461 |  5758 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   115461 |  5759 | `		if( pIdx ){` |
|        - |  5760 | `			/* Load the desired entry */` |
|   115461 |  5761 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    57728 |  5762 | `		}` |
|   115461 |  5763 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5764 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5765 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5766 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5767 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5768 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5769 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5770 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5771 | `			 * correct for the outermost write. */` |
|       19 |  5772 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5773 | `			if( !needWrite && pNode ){` |
|       13 |  5774 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5775 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5776 | `					needWrite = 1;` |
|        3 |  5777 | `				}` |
|        6 |  5778 | `			}` |
|       19 |  5779 | `			if( needWrite ){` |
|       13 |  5780 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5781 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5782 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5783 | `					 * into the new map's storage. */` |
|        7 |  5784 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5785 | `					if( pIdx ){` |
|        7 |  5786 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5787 | `					}` |
|        3 |  5788 | `				}` |
|        6 |  5789 | `			}` |
|        9 |  5790 | `		}` |
|   115461 |  5791 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5792 | `			/* Create a new empty entry */` |
|      273 |  5793 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5794 | `			if( rc == SXRET_OK ){` |
|        - |  5795 | `				/* Point to the last inserted entry */` |
|      273 |  5796 | `				pNode = pMap->pLast;` |
|      136 |  5797 | `			}` |
|      136 |  5798 | `		}` |
|    57728 |  5799 | `	}` |
|   115461 |  5800 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5801 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5802 | `		char zMsg[128];` |
|      ! 0 |  5803 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5804 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5805 | `		}` |
|      ! 0 |  5806 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5807 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5808 | `	}` |
|   115461 |  5809 | `	if( pIdx ){` |
|   115461 |  5810 | `		PH7_MemObjRelease(pIdx);` |
|    57728 |  5811 | `	}` |
|   115461 |  5812 | `	if( rc == SXRET_OK ){` |
|        - |  5813 | `		/* Load entry contents */` |
|    51083 |  5814 | `		if( pMap->iRef < 2 ){` |
|        - |  5815 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5816 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5817 | `			 */` |
|       28 |  5818 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5819 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5820 | `		}else{` |
|    51057 |  5821 | `			pTos->nIdx = pNode->nValIdx;` |
|    51057 |  5822 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    51057 |  5823 | `			PH7_HashmapUnref(pMap);` |
|        - |  5824 | `		}` |
|    25544 |  5825 | `	}else{` |
|        - |  5826 | `		/* No such entry,load NULL */` |
|    64383 |  5827 | `		PH7_MemObjRelease(pTos);` |
|    64383 |  5828 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5829 | `	}` |
|   115461 |  5830 | `	break;` |
|        - |  5831 | `					  }` |
|        - |  5832 | `/*` |
|        - |  5833 | ` * LOAD_CLOSURE * * P3` |
|        - |  5834 | ` *` |
|        - |  5835 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5836 | ` * name in the stack.` |
|        - |  5837 | ` */` |
|       64 |  5838 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5839 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5840 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5841 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5842 | `		ph7_vm_func *pClosure;` |
|        - |  5843 | `		char *zName;` |
|        - |  5844 | `		sxu32 mLen;` |
|        - |  5845 | `		sxu32 n;` |
|        - |  5846 | `		/* Create a new VM function */` |
|      130 |  5847 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5848 | `		/* Generate an unique closure name */` |
|      130 |  5849 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5850 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5851 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5852 | `			goto Abort;` |
|        - |  5853 | `		}` |
|      130 |  5854 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5855 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5856 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5857 | `		}` |
|        - |  5858 | `		/* Zero the stucture */` |
|      130 |  5859 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5860 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5861 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5862 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5863 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5864 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5865 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5866 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5867 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5868 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5869 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5870 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5871 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5872 | `		/* Register the closure */` |
|      130 |  5873 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5874 | `		/* Set up closure environment */` |
|      130 |  5875 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5876 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5877 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5878 | `			ph7_value *pValue;` |
|      196 |  5879 | `			pEnv = &aEnv[n];` |
|      196 |  5880 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5881 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5882 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5883 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5884 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5885 | `				/* Pass by reference */` |
|      ! 0 |  5886 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5887 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5888 | `					);` |
|      ! 0 |  5889 | `			}` |
|        - |  5890 | `			/* Standard pass by value */` |
|      196 |  5891 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5892 | `			if( pValue ){` |
|        - |  5893 | `				/* Copy imported value */` |
|       72 |  5894 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5895 | `			}` |
|        - |  5896 | `			/* Insert the imported variable */` |
|      196 |  5897 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5898 | `		}` |
|        - |  5899 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5900 | `		pTos++;` |
|      130 |  5901 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5902 | `	}` |
|      130 |  5903 | `	break;` |
|        - |  5904 | `						 }` |
|        - |  5905 | `/*` |
|        - |  5906 | ` * STORE * P2 P3` |
|        - |  5907 | ` *` |
|        - |  5908 | ` * Perform a store (Assignment) operation.` |
|        - |  5909 | ` */` |
|   148835 |  5910 | `case PH7_OP_STORE: {` |
|        - |  5911 | `	ph7_value *pObj;` |
|        - |  5912 | `	SyString sName;` |
|        - |  5913 | `#ifdef UNTRUST` |
|        - |  5914 | `	if( pTos < pStack ){` |
|        - |  5915 | `		goto Abort;` |
|        - |  5916 | `	}` |
|        - |  5917 | `#endif` |
|   297675 |  5918 | `	if( pInstr->iP2 ){` |
|        - |  5919 | `		sxu32 nIdx;` |
|        - |  5920 | `		sxi32 rcT;` |
|        - |  5921 | `		/* Member store operation */` |
|     5639 |  5922 | `		nIdx = pTos->nIdx;` |
|     5639 |  5923 | `		VmPopOperand(&pTos,1);` |
|     5639 |  5924 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5925 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5926 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5927 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5928 | `		}else{` |
|        - |  5929 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5930 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5635 |  5931 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5635 |  5932 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5933 | `				goto Abort;` |
|        - |  5934 | `			}` |
|     5635 |  5935 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5936 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5937 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5938 | `				 * propagate out of the VM loop. */` |
|       43 |  5939 | `				VmPopOperand(&pTos,1);` |
|        - |  5940 | `				{` |
|       43 |  5941 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       43 |  5942 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       43 |  5943 | `						pc = pFrm2->iExceptionJump - 1;` |
|   148859 |  5944 | `						break;` |
|        - |  5945 | `					}` |
|        - |  5946 | `				}` |
|      ! 0 |  5947 | `				goto Exception;` |
|        - |  5948 | `			}` |
|        - |  5949 | `			/* Point to the desired memory object */` |
|     5597 |  5950 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5597 |  5951 | `			if( pObj ){` |
|        - |  5952 | `				/* Perform the store operation */` |
|     5597 |  5953 | `				PH7_MemObjStore(pTos,pObj);` |
|     2796 |  5954 | `			}` |
|        - |  5955 | `		}` |
|     5601 |  5956 | `		break;` |
|   292041 |  5957 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5958 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5959 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5960 | `			/* Force a string cast */` |
|      ! 0 |  5961 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5962 | `		}` |
|        7 |  5963 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5964 | `		pTos--;` |
|        - |  5965 | `#ifdef UNTRUST` |
|        - |  5966 | `		if( pTos < pStack  ){` |
|        - |  5967 | `			goto Abort;` |
|        - |  5968 | `		}` |
|        - |  5969 | `#endif` |
|        4 |  5970 | `	}else{` |
|   292035 |  5971 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5972 | `	}` |
|        - |  5973 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   292041 |  5974 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   292041 |  5975 | `	if( pObj == 0 ){` |
|      ! 0 |  5976 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5977 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5978 | `		goto Abort;` |
|        - |  5979 | `	}` |
|   292041 |  5980 | `	if( !pInstr->p3 ){` |
|        7 |  5981 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5982 | `	}` |
|        - |  5983 | `	/* Perform the store operation */` |
|   292041 |  5984 | `	PH7_MemObjStore(pTos,pObj);` |
|   292041 |  5985 | `	break;` |
|        - |  5986 | `				   }` |
|        - |  5987 | `/*` |
|        - |  5988 | ` * STORE_IDX:   P1 * P3` |
|        - |  5989 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5990 | ` *` |
|        - |  5991 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5992 | ` */` |
|    98364 |  5993 | `case PH7_OP_STORE_IDX:` |
|        - |  5994 | `case PH7_OP_STORE_IDX_REF: {` |
|   196733 |  5995 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5996 | `	ph7_value *pKey;` |
|        - |  5997 | `	sxu32 nIdx;` |
|   196733 |  5998 | `	if( pInstr->iP1 ){` |
|        - |  5999 | `		/* Key is next on stack */` |
|    63817 |  6000 | `		pKey = pTos;` |
|    63817 |  6001 | `		pTos--;` |
|    31911 |  6002 | `	}else{` |
|   132921 |  6003 | `		pKey = 0;` |
|        - |  6004 | `	}` |
|   196733 |  6005 | `	nIdx = pTos->nIdx;` |
|        - |  6006 | `	{` |
|        - |  6007 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  6008 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  6009 | `		 * the backing variable slot at nIdx. */` |
|   196733 |  6010 | `		ph7_class_instance *pInst = 0;` |
|   196733 |  6011 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       35 |  6012 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   196717 |  6013 | `		}else if( nIdx != SXU32_HIGH ){` |
|   196701 |  6014 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   196701 |  6015 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  6016 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  6017 | `			}` |
|    98348 |  6018 | `		}` |
|   196733 |  6019 | `		if( pInst ){` |
|       35 |  6020 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       35 |  6021 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  6022 | `				ph7_class_method *pMeth;` |
|        - |  6023 | `				ph7_value sNullKey;` |
|        - |  6024 | `				ph7_value *apArg[2];` |
|       33 |  6025 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  6026 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6027 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  6028 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  6029 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  6030 | `					break;` |
|        - |  6031 | `				}` |
|       33 |  6032 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6033 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6034 | `				/* Pop container; pTos now points to the value */` |
|       33 |  6035 | `				VmPopOperand(&pTos,1);` |
|       33 |  6036 | `				if( pKey == 0 ){` |
|        7 |  6037 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  6038 | `					apArg[0] = &sNullKey;` |
|        4 |  6039 | `				}else{` |
|       27 |  6040 | `					apArg[0] = pKey;` |
|        - |  6041 | `				}` |
|       33 |  6042 | `				apArg[1] = pTos;` |
|       33 |  6043 | `				if( pMeth ){` |
|       33 |  6044 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  6045 | `				}` |
|       33 |  6046 | `				if( pKey ){` |
|       27 |  6047 | `					PH7_MemObjRelease(pKey);` |
|       15 |  6048 | `				}else{` |
|        7 |  6049 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  6050 | `				}` |
|        - |  6051 | `				/* Pop the value */` |
|       33 |  6052 | `				VmPopOperand(&pTos,1);` |
|       33 |  6053 | `				break;` |
|        - |  6054 | `			}` |
|        - |  6055 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  6056 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  6057 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  6058 | `			 * a few lines below). Match PHP. */` |
|        - |  6059 | `			{` |
|        - |  6060 | `				char zMsg[256];` |
|        3 |  6061 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  6062 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  6063 | `					"Cannot use object of type %.*s as array",` |
|        2 |  6064 | `					(int)pName->nByte,pName->zString);` |
|        3 |  6065 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  6066 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  6067 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  6068 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  6069 | `				break;` |
|        - |  6070 | `			}` |
|        - |  6071 | `		}` |
|        - |  6072 | `	}` |
|   196701 |  6073 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6074 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  6075 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  6076 | `		 * checking true sharing count, then re-add after separation. */` |
|   196649 |  6077 | `		if( nIdx != SXU32_HIGH ){` |
|   196649 |  6078 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   294971 |  6079 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   196649 |  6080 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6081 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  6082 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  6083 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  6084 | `				 * refcounts if the backing array was already separated. */` |
|   196649 |  6085 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   196649 |  6086 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   196649 |  6087 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   196649 |  6088 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   196649 |  6089 | `					pTos->x.pOther = pMap;` |
|    98327 |  6090 | `				}else{` |
|        - |  6091 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  6092 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  6093 | `					pMap = pCur;` |
|        - |  6094 | `				}` |
|    98327 |  6095 | `			}else{` |
|      ! 0 |  6096 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6097 | `			}` |
|    98327 |  6098 | `		}else{` |
|      ! 0 |  6099 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6100 | `		}` |
|   196649 |  6101 | `		if( pMap->iRef < 2 ){` |
|        - |  6102 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  6103 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  6104 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  6105 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  6106 | `			pMap->iRef = 2;` |
|      ! 0 |  6107 | `		}` |
|    98327 |  6108 | `	}else{` |
|        - |  6109 | `		ph7_value *pObj;` |
|       53 |  6110 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  6111 | `		if( pObj == 0 ){` |
|      ! 0 |  6112 | `			if( pKey ){` |
|      ! 0 |  6113 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  6114 | `			}` |
|      ! 0 |  6115 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6116 | `			break;` |
|        - |  6117 | `		}` |
|        - |  6118 | `		/* Phase#1: Load the array */` |
|       53 |  6119 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  6120 | `			VmPopOperand(&pTos,1);` |
|       53 |  6121 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  6122 | `				/* Force a string cast */` |
|      ! 0 |  6123 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  6124 | `			}` |
|       53 |  6125 | `			if( pKey == 0 ){` |
|        - |  6126 | `				/* Append string */` |
|        3 |  6127 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  6128 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  6129 | `				}` |
|        2 |  6130 | `			}else{` |
|        - |  6131 | `				sxu32 nOfft;` |
|       51 |  6132 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  6133 | `					/* Force an int cast */` |
|       51 |  6134 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  6135 | `				}` |
|       51 |  6136 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  6137 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  6138 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  6139 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  6140 | `					zData[nOfft] = zBlob[0];` |
|       26 |  6141 | `				}else{` |
|      ! 0 |  6142 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  6143 | `						/* Perform an append operation */` |
|      ! 0 |  6144 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  6145 | `					}` |
|        - |  6146 | `				}` |
|        - |  6147 | `			}` |
|       53 |  6148 | `			if( pKey ){` |
|       51 |  6149 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  6150 | `			}` |
|       53 |  6151 | `			break;` |
|      ! 0 |  6152 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  6153 | `			/* Force a hashmap cast  */` |
|      ! 0 |  6154 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  6155 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6156 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  6157 | `				goto Abort;` |
|        - |  6158 | `			}` |
|      ! 0 |  6159 | `		}` |
|        - |  6160 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  6161 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  6162 | `	}` |
|   196649 |  6163 | `	VmPopOperand(&pTos,1);` |
|        - |  6164 | `	/* Phase#2: Perform the insertion */` |
|   196649 |  6165 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  6166 | `		/* Insertion by reference */` |
|       15 |  6167 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  6168 | `	}else{` |
|   196635 |  6169 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  6170 | `	}` |
|   196649 |  6171 | `	if( pKey ){` |
|    63741 |  6172 | `		PH7_MemObjRelease(pKey);` |
|    31868 |  6173 | `	}` |
|   196649 |  6174 | `	break;` |
|        - |  6175 | `					   }` |
|        - |  6176 | `/*` |
|        - |  6177 | ` * INCR: P1 * *` |
|        - |  6178 | ` *` |
|        - |  6179 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  6180 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  6181 | ` * the stack and increment after that.` |
|        - |  6182 | ` */` |
|   168804 |  6183 | `case PH7_OP_INCR:` |
|        - |  6184 | `#ifdef UNTRUST` |
|        - |  6185 | `	if( pTos < pStack ){` |
|        - |  6186 | `		goto Abort;` |
|        - |  6187 | `	}` |
|        - |  6188 | `#endif` |
|   337657 |  6189 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   337657 |  6190 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6191 | `			ph7_value *pObj;` |
|   337657 |  6192 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   337657 |  6193 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6194 | `					/* Perl-style string increment.` |
|        - |  6195 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  6196 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  6197 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  6198 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  6199 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  6200 | `					}` |
|       49 |  6201 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  6202 | `					if( pInstr->iP1 ){` |
|        - |  6203 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  6204 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  6205 | `					}` |
|       25 |  6206 | `				}else{` |
|        - |  6207 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  6208 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  6209 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  6210 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  6211 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  6212 | `					 * so its old-value view survives the coercion. */` |
|   337609 |  6213 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  6214 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  6215 | `					}` |
|        - |  6216 | `					/* Force a numeric cast on the variable */` |
|   337609 |  6217 | `					PH7_MemObjToNumeric(pObj);` |
|   337609 |  6218 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6219 | `						pObj->rVal++;` |
|        - |  6220 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6221 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6222 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6223 | `						 * integer-valued real. */` |
|        9 |  6224 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6225 | `					}else{` |
|   337601 |  6226 | `						pObj->x.iVal++;` |
|        - |  6227 | `					}` |
|   337609 |  6228 | `					if( pInstr->iP1 ){` |
|        - |  6229 | `						/* Pre-increment: result is the new value. */` |
|       83 |  6230 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  6231 | `					}` |
|        - |  6232 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  6233 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  6234 | `				}` |
|   168848 |  6235 | `			}` |
|   168853 |  6236 | `		}else{` |
|      ! 0 |  6237 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6238 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  6239 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  6240 | `				}else{` |
|        - |  6241 | `					/* Force a numeric cast */` |
|      ! 0 |  6242 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6243 | `					/* Pre-increment */` |
|      ! 0 |  6244 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6245 | `						pTos->rVal++;` |
|        - |  6246 | `						/* Try to get an integer representation */` |
|      ! 0 |  6247 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6248 | `					}else{` |
|      ! 0 |  6249 | `						pTos->x.iVal++;` |
|      ! 0 |  6250 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6251 | `					}` |
|        - |  6252 | `				}` |
|      ! 0 |  6253 | `			}` |
|        - |  6254 | `		}` |
|   168848 |  6255 | `	}` |
|   337657 |  6256 | `	break;` |
|        - |  6257 | `/*` |
|        - |  6258 | ` * DECR: P1 * *` |
|        - |  6259 | ` *` |
|        - |  6260 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  6261 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  6262 | ` * and decrement after that.` |
|        - |  6263 | ` */` |
|       14 |  6264 | `case PH7_OP_DECR:` |
|        - |  6265 | `#ifdef UNTRUST` |
|        - |  6266 | `	if( pTos < pStack ){` |
|        - |  6267 | `		goto Abort;` |
|        - |  6268 | `	}` |
|        - |  6269 | `#endif` |
|        - |  6270 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  6271 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  6272 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6273 | `			ph7_value *pObj;` |
|       27 |  6274 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  6275 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6276 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  6277 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  6278 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  6279 | `					if( pInstr->iP1 ){` |
|        - |  6280 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  6281 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  6282 | `					}` |
|        - |  6283 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  6284 | `				}else{` |
|        - |  6285 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  6286 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  6287 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  6288 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  6289 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  6290 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  6291 | `					}` |
|       21 |  6292 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  6293 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6294 | `						pObj->rVal--;` |
|        - |  6295 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6296 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6297 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6298 | `						 * integer-valued real. */` |
|        9 |  6299 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6300 | `					}else{` |
|       13 |  6301 | `						pObj->x.iVal--;` |
|        - |  6302 | `					}` |
|       21 |  6303 | `					if( pInstr->iP1 ){` |
|        - |  6304 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  6305 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  6306 | `					}` |
|        - |  6307 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  6308 | `				}` |
|       13 |  6309 | `			}` |
|       14 |  6310 | `		}else{` |
|      ! 0 |  6311 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6312 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  6313 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  6314 | `				}else{` |
|        - |  6315 | `					/* Force a numeric cast */` |
|      ! 0 |  6316 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6317 | `					/* Pre-decrement */` |
|      ! 0 |  6318 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6319 | `						pTos->rVal--;` |
|        - |  6320 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  6321 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6322 | `					}else{` |
|      ! 0 |  6323 | `						pTos->x.iVal--;` |
|      ! 0 |  6324 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6325 | `					}` |
|        - |  6326 | `				}` |
|      ! 0 |  6327 | `			}` |
|        - |  6328 | `		}` |
|       13 |  6329 | `	}` |
|       29 |  6330 | `	break;` |
|        - |  6331 | `/*` |
|        - |  6332 | ` * UMINUS: * * *` |
|        - |  6333 | ` *` |
|        - |  6334 | ` * Perform a unary minus operation.` |
|        - |  6335 | ` */` |
|    30209 |  6336 | `case PH7_OP_UMINUS:` |
|        - |  6337 | `#ifdef UNTRUST` |
|        - |  6338 | `	if( pTos < pStack ){` |
|        - |  6339 | `		goto Abort;` |
|        - |  6340 | `	}` |
|        - |  6341 | `#endif` |
|        - |  6342 | `	/* Force a numeric (integer,real or both) cast */` |
|    60423 |  6343 | `	PH7_MemObjToNumeric(pTos);` |
|    60423 |  6344 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  6345 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  6346 | `	}` |
|    60423 |  6347 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    60393 |  6348 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    30194 |  6349 | `	}` |
|    60423 |  6350 | `	break;` |
|        - |  6351 | `/*` |
|        - |  6352 | ` * UPLUS: * * *` |
|        - |  6353 | ` *` |
|        - |  6354 | ` * Perform a unary plus operation.` |
|        - |  6355 | ` */` |
|       18 |  6356 | `case PH7_OP_UPLUS:` |
|        - |  6357 | `#ifdef UNTRUST` |
|        - |  6358 | `	if( pTos < pStack ){` |
|        - |  6359 | `		goto Abort;` |
|        - |  6360 | `	}` |
|        - |  6361 | `#endif` |
|        - |  6362 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  6363 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  6364 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6365 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  6366 | `	}` |
|       37 |  6367 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  6368 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  6369 | `	}` |
|       37 |  6370 | `	break;` |
|        - |  6371 | `/*` |
|        - |  6372 | ` * OP_LNOT: * * *` |
|        - |  6373 | ` *` |
|        - |  6374 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  6375 | ` * with its complement.` |
|        - |  6376 | ` */` |
|    45179 |  6377 | `case PH7_OP_LNOT:` |
|        - |  6378 | `#ifdef UNTRUST` |
|        - |  6379 | `	if( pTos < pStack ){` |
|        - |  6380 | `		goto Abort;` |
|        - |  6381 | `	}` |
|        - |  6382 | `#endif` |
|        - |  6383 | `	/* Force a boolean cast */` |
|    90407 |  6384 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       27 |  6385 | `		PH7_MemObjToBool(pTos);` |
|       11 |  6386 | `	}` |
|    90407 |  6387 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    90407 |  6388 | `	break;` |
|        - |  6389 | `/*` |
|        - |  6390 | ` * OP_BITNOT: * * *` |
|        - |  6391 | ` *` |
|        - |  6392 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6393 | ` * with its ones-complement.` |
|        - |  6394 | ` */` |
|       14 |  6395 | `case PH7_OP_BITNOT:` |
|        - |  6396 | `#ifdef UNTRUST` |
|        - |  6397 | `	if( pTos < pStack ){` |
|        - |  6398 | `		goto Abort;` |
|        - |  6399 | `	}` |
|        - |  6400 | `#endif` |
|        - |  6401 | `	/* Force an integer cast */` |
|       33 |  6402 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6403 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6404 | `	}` |
|       33 |  6405 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       33 |  6406 | `	break;` |
|        - |  6407 | `/* OP_MUL * * *` |
|        - |  6408 | ` * OP_MUL_STORE * * *` |
|        - |  6409 | ` *` |
|        - |  6410 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6411 | ` * and push the result back onto the stack.` |
|        - |  6412 | ` */` |
|     1297 |  6413 | `case PH7_OP_MUL:` |
|        - |  6414 | `case PH7_OP_MUL_STORE: {` |
|     2598 |  6415 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6416 | `	/* Force the operand to be numeric */` |
|        - |  6417 | `#ifdef UNTRUST` |
|        - |  6418 | `	if( pNos < pStack ){` |
|        - |  6419 | `		goto Abort;` |
|        - |  6420 | `	}` |
|        - |  6421 | `#endif` |
|     2598 |  6422 | `	PH7_MemObjToNumeric(pTos);` |
|     2598 |  6423 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6424 | `	/* Perform the requested operation */` |
|     2598 |  6425 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6426 | `		/* Floating point arithemic */` |
|        - |  6427 | `		ph7_real a,b,r;` |
|       21 |  6428 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6429 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6430 | `		}` |
|       21 |  6431 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6432 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6433 | `		}` |
|       21 |  6434 | `		a = pNos->rVal;` |
|       21 |  6435 | `		b = pTos->rVal;` |
|       21 |  6436 | `		r = a * b;` |
|        - |  6437 | `		/* Push the result */` |
|       21 |  6438 | `		pNos->rVal = r;` |
|       21 |  6439 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6440 | `		/* Try to get an integer representation */` |
|       21 |  6441 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6442 | `	}else{` |
|        - |  6443 | `		/* Integer arithmetic */` |
|        - |  6444 | `		sxi64 a,b,r;` |
|     2578 |  6445 | `		a = pNos->x.iVal;` |
|     2578 |  6446 | `		b = pTos->x.iVal;` |
|     2578 |  6447 | `		r = a * b;` |
|        - |  6448 | `		/* Push the result */` |
|     2578 |  6449 | `		pNos->x.iVal = r;` |
|     2578 |  6450 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6451 | `	}` |
|     2598 |  6452 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6453 | `		ph7_value *pObj;` |
|       32 |  6454 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6455 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6456 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6457 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6458 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6459 | `		}` |
|       15 |  6460 | `	}` |
|     2598 |  6461 | `	VmPopOperand(&pTos,1);` |
|     2598 |  6462 | `	break;` |
|        - |  6463 | `				 }` |
|        - |  6464 | `/* OP_POW * * *` |
|        - |  6465 | ` * OP_POW_STORE * * *` |
|        - |  6466 | ` *` |
|        - |  6467 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6468 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6469 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6470 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6471 | ` */` |
|       67 |  6472 | `case PH7_OP_POW:` |
|        - |  6473 | `case PH7_OP_POW_STORE: {` |
|      135 |  6474 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6475 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6476 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6477 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6478 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6479 | `	 */` |
|      135 |  6480 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6481 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6482 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6483 | `	int bBothInt;` |
|      135 |  6484 | `	int usedInt = 0;` |
|        - |  6485 | `	ph7_real a, b, r;` |
|        - |  6486 | `#endif` |
|      135 |  6487 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6488 | `#ifdef UNTRUST` |
|        - |  6489 | `	if( pNos < pStack ){` |
|        - |  6490 | `		goto Abort;` |
|        - |  6491 | `	}` |
|        - |  6492 | `#endif` |
|      135 |  6493 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6494 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6495 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6496 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6497 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6498 | `	if( bBothInt ){` |
|      123 |  6499 | `		base_i = pBase->x.iVal;` |
|      123 |  6500 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6501 | `	}` |
|      135 |  6502 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6503 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6504 | `	}` |
|      135 |  6505 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6506 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6507 | `	}` |
|      135 |  6508 | `	a = pBase->rVal;` |
|      135 |  6509 | `	b = pExp->rVal;` |
|      135 |  6510 | `	r = pow(a, b);` |
|        - |  6511 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6512 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6513 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6514 | `	 * representable as double but not as signed int64. */` |
|      135 |  6515 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6516 | `		sxi64 result_i = 1;` |
|      117 |  6517 | `		sxi64 cur_base = base_i;` |
|      117 |  6518 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6519 | `		int overflow = 0;` |
|      401 |  6520 | `		while( cur_exp > 0 ){` |
|      289 |  6521 | `			if( cur_exp & 1 ){` |
|      189 |  6522 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6523 | `					overflow = 1;` |
|        3 |  6524 | `					break;` |
|        - |  6525 | `				}` |
|       93 |  6526 | `			}` |
|      287 |  6527 | `			cur_exp >>= 1;` |
|      287 |  6528 | `			if( cur_exp > 0 ){` |
|      181 |  6529 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6530 | `					overflow = 1;` |
|        3 |  6531 | `					break;` |
|        - |  6532 | `				}` |
|       89 |  6533 | `			}` |
|        1 |  6534 | `		}` |
|      117 |  6535 | `		if( !overflow ){` |
|      113 |  6536 | `			pNos->x.iVal = result_i;` |
|      113 |  6537 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6538 | `			usedInt = 1;` |
|       56 |  6539 | `		}` |
|       58 |  6540 | `	}` |
|      135 |  6541 | `	if( !usedInt ){` |
|       23 |  6542 | `		pNos->rVal = r;` |
|       23 |  6543 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6544 | `	}` |
|        - |  6545 | `#else` |
|        - |  6546 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6547 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6548 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6549 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6550 | `	 * represented. */` |
|        - |  6551 | `	base_i = pBase->x.iVal;` |
|        - |  6552 | `	exp_i  = pExp->x.iVal;` |
|        - |  6553 | `	{` |
|        - |  6554 | `		sxi64 result_i = 1;` |
|        - |  6555 | `		sxi64 cur_base = base_i;` |
|        - |  6556 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6557 | `		if( cur_exp < 0 ){` |
|        - |  6558 | `			result_i = 0;` |
|        - |  6559 | `		}else{` |
|        - |  6560 | `			while( cur_exp > 0 ){` |
|        - |  6561 | `				if( cur_exp & 1 ){` |
|        - |  6562 | `					result_i *= cur_base;` |
|        - |  6563 | `				}` |
|        - |  6564 | `				cur_exp >>= 1;` |
|        - |  6565 | `				if( cur_exp > 0 ){` |
|        - |  6566 | `					cur_base *= cur_base;` |
|        - |  6567 | `				}` |
|        - |  6568 | `			}` |
|        - |  6569 | `		}` |
|        - |  6570 | `		pNos->x.iVal = result_i;` |
|        - |  6571 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6572 | `	}` |
|        - |  6573 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6574 | `	if( bStore ){` |
|        - |  6575 | `		ph7_value *pObj;` |
|       23 |  6576 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6577 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6578 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6579 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6580 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6581 | `		}` |
|       11 |  6582 | `	}` |
|      135 |  6583 | `	VmPopOperand(&pTos,1);` |
|      135 |  6584 | `	break;` |
|        - |  6585 | `				 }` |
|        - |  6586 | `/* OP_ADD * * *` |
|        - |  6587 | ` *` |
|        - |  6588 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6589 | ` * and push the result back onto the stack.` |
|        - |  6590 | ` */` |
|      538 |  6591 | `case PH7_OP_ADD:{` |
|     1081 |  6592 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6593 | `#ifdef UNTRUST` |
|        - |  6594 | `	if( pNos < pStack ){` |
|        - |  6595 | `		goto Abort;` |
|        - |  6596 | `	}` |
|        - |  6597 | `#endif` |
|        - |  6598 | `	/* Perform the addition */` |
|     1081 |  6599 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1081 |  6600 | `	VmPopOperand(&pTos,1);` |
|     1081 |  6601 | `	break;` |
|        - |  6602 | `				}` |
|        - |  6603 | `/*` |
|        - |  6604 | ` * OP_ADD_STORE * * *` |
|        - |  6605 | ` *` |
|        - |  6606 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6607 | ` * and push the result back onto the stack.` |
|        - |  6608 | ` */` |
|      502 |  6609 | `case PH7_OP_ADD_STORE:{` |
|     1008 |  6610 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6611 | `	ph7_value *pObj;` |
|        - |  6612 | `	sxu32 nIdx;` |
|        - |  6613 | `#ifdef UNTRUST` |
|        - |  6614 | `	if( pNos < pStack ){` |
|        - |  6615 | `		goto Abort;` |
|        - |  6616 | `	}` |
|        - |  6617 | `#endif` |
|        - |  6618 | `	/* Perform the addition */` |
|     1008 |  6619 | `	nIdx = pTos->nIdx;` |
|     1008 |  6620 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6621 | `	/* Peform the store operation */` |
|     1008 |  6622 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6623 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1008 |  6624 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1008 |  6625 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1008 |  6626 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6627 | `	}` |
|        - |  6628 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1008 |  6629 | `	PH7_MemObjStore(pTos,pNos);` |
|     1008 |  6630 | `	VmPopOperand(&pTos,1);` |
|     1008 |  6631 | `	break;` |
|        - |  6632 | `				}` |
|        - |  6633 | `/* OP_SUB * * *` |
|        - |  6634 | ` *` |
|        - |  6635 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6636 | ` * first (what was next on the stack) from the second (the` |
|        - |  6637 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6638 | ` */` |
|      352 |  6639 | `case PH7_OP_SUB: {` |
|      709 |  6640 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6641 | `#ifdef UNTRUST` |
|        - |  6642 | `	if( pNos < pStack ){` |
|        - |  6643 | `		goto Abort;` |
|        - |  6644 | `	}` |
|        - |  6645 | `#endif` |
|      709 |  6646 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6647 | `		/* Floating point arithemic */` |
|        - |  6648 | `		ph7_real a,b,r;` |
|      103 |  6649 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6650 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6651 | `		}` |
|      103 |  6652 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6653 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6654 | `		}` |
|      103 |  6655 | `		a = pNos->rVal;` |
|      103 |  6656 | `		b = pTos->rVal;` |
|      103 |  6657 | `		r = a - b;` |
|        - |  6658 | `		/* Push the result */` |
|      103 |  6659 | `		pNos->rVal = r;` |
|      103 |  6660 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6661 | `		/* Try to get an integer representation */` |
|      103 |  6662 | `		PH7_MemObjTryInteger(pNos);` |
|       52 |  6663 | `	}else{` |
|        - |  6664 | `		/* Integer arithmetic */` |
|        - |  6665 | `		sxi64 a,b,r;` |
|      607 |  6666 | `		a = pNos->x.iVal;` |
|      607 |  6667 | `		b = pTos->x.iVal;` |
|      607 |  6668 | `		r = a - b;` |
|        - |  6669 | `		/* Push the result */` |
|      607 |  6670 | `		pNos->x.iVal = r;` |
|      607 |  6671 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6672 | `	}` |
|      709 |  6673 | `	VmPopOperand(&pTos,1);` |
|      709 |  6674 | `	break;` |
|        - |  6675 | `				 }` |
|        - |  6676 | `/* OP_SUB_STORE * * *` |
|        - |  6677 | ` *` |
|        - |  6678 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6679 | ` * first (what was next on the stack) from the second (the` |
|        - |  6680 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6681 | ` */` |
|        4 |  6682 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6683 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6684 | `	ph7_value *pObj;` |
|        - |  6685 | `#ifdef UNTRUST` |
|        - |  6686 | `	if( pNos < pStack ){` |
|        - |  6687 | `		goto Abort;` |
|        - |  6688 | `	}` |
|        - |  6689 | `#endif` |
|       10 |  6690 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6691 | `		/* Floating point arithemic */` |
|        - |  6692 | `		ph7_real a,b,r;` |
|      ! 0 |  6693 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6694 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6695 | `		}` |
|      ! 0 |  6696 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6697 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6698 | `		}` |
|      ! 0 |  6699 | `		a = pTos->rVal;` |
|      ! 0 |  6700 | `		b = pNos->rVal;` |
|      ! 0 |  6701 | `		r = a - b;` |
|        - |  6702 | `		/* Push the result */` |
|      ! 0 |  6703 | `		pNos->rVal = r;` |
|      ! 0 |  6704 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6705 | `		/* Try to get an integer representation */` |
|      ! 0 |  6706 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6707 | `	}else{` |
|        - |  6708 | `		/* Integer arithmetic */` |
|        - |  6709 | `		sxi64 a,b,r;` |
|       10 |  6710 | `		a = pTos->x.iVal;` |
|       10 |  6711 | `		b = pNos->x.iVal;` |
|       10 |  6712 | `		r = a - b;` |
|        - |  6713 | `		/* Push the result */` |
|       10 |  6714 | `		pNos->x.iVal = r;` |
|       10 |  6715 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6716 | `	}` |
|       10 |  6717 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6718 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6719 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6720 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6721 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6722 | `	}` |
|       10 |  6723 | `	VmPopOperand(&pTos,1);` |
|       10 |  6724 | `	break;` |
|        - |  6725 | `				 }` |
|        - |  6726 |  |
|        - |  6727 | `/*` |
|        - |  6728 | ` * OP_MOD * * *` |
|        - |  6729 | ` *` |
|        - |  6730 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6731 | ` * first (what was next on the stack) from the second (the` |
|        - |  6732 | ` * top of the stack) and push the remainder after division` |
|        - |  6733 | ` * onto the stack.` |
|        - |  6734 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6735 | ` */` |
|      309 |  6736 | `case PH7_OP_MOD:{` |
|      623 |  6737 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6738 | `	sxi64 a,b,r;` |
|        - |  6739 | `#ifdef UNTRUST` |
|        - |  6740 | `	if( pNos < pStack ){` |
|        - |  6741 | `		goto Abort;` |
|        - |  6742 | `	}` |
|        - |  6743 | `#endif` |
|        - |  6744 | `	/* Force the operands to be integer */` |
|      623 |  6745 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6746 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6747 | `	}` |
|      623 |  6748 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6749 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6750 | `	}` |
|        - |  6751 | `	/* Perform the requested operation */` |
|      623 |  6752 | `	a = pNos->x.iVal;` |
|      623 |  6753 | `	b = pTos->x.iVal;` |
|      623 |  6754 | `	if( b == 0 ){` |
|        3 |  6755 | `		r = 0;` |
|        3 |  6756 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6757 | `		/* goto Abort; */` |
|        2 |  6758 | `	}else{` |
|      621 |  6759 | `		r = a%b;` |
|        - |  6760 | `	}` |
|        - |  6761 | `	/* Push the result */` |
|      623 |  6762 | `	pNos->x.iVal = r;` |
|      623 |  6763 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      623 |  6764 | `	VmPopOperand(&pTos,1);` |
|      623 |  6765 | `	break;` |
|        - |  6766 | `				}` |
|        - |  6767 | `/*` |
|        - |  6768 | ` * OP_MOD_STORE * * *` |
|        - |  6769 | ` *` |
|        - |  6770 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6771 | ` * first (what was next on the stack) from the second (the` |
|        - |  6772 | ` * top of the stack) and push the remainder after division` |
|        - |  6773 | ` * onto the stack.` |
|        - |  6774 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6775 | ` */` |
|        1 |  6776 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6777 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6778 | `	ph7_value *pObj;` |
|        - |  6779 | `	sxi64 a,b,r;` |
|        - |  6780 | `#ifdef UNTRUST` |
|        - |  6781 | `	if( pNos < pStack ){` |
|        - |  6782 | `		goto Abort;` |
|        - |  6783 | `	}` |
|        - |  6784 | `#endif` |
|        - |  6785 | `	/* Force the operands to be integer */` |
|        3 |  6786 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6787 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6788 | `	}` |
|        3 |  6789 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6790 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6791 | `	}` |
|        - |  6792 | `	/* Perform the requested operation */` |
|        3 |  6793 | `	a = pTos->x.iVal;` |
|        3 |  6794 | `	b = pNos->x.iVal;` |
|        3 |  6795 | `	if( b == 0 ){` |
|      ! 0 |  6796 | `		r = 0;` |
|      ! 0 |  6797 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6798 | `		/* goto Abort; */` |
|      ! 0 |  6799 | `	}else{` |
|        3 |  6800 | `		r = a%b;` |
|        - |  6801 | `	}` |
|        - |  6802 | `	/* Push the result */` |
|        3 |  6803 | `	pNos->x.iVal = r;` |
|        3 |  6804 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6805 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6806 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6807 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6808 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6809 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6810 | `	}` |
|        3 |  6811 | `	VmPopOperand(&pTos,1);` |
|        3 |  6812 | `	break;` |
|        - |  6813 | `				}` |
|        - |  6814 | `/*` |
|        - |  6815 | ` * OP_DIV * * *` |
|        - |  6816 | ` *` |
|        - |  6817 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6818 | ` * first (what was next on the stack) from the second (the` |
|        - |  6819 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6820 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6821 | ` */` |
|       33 |  6822 | `case PH7_OP_DIV:{` |
|       68 |  6823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6824 | `	ph7_real a,b,r;` |
|        - |  6825 | `#ifdef UNTRUST` |
|        - |  6826 | `	if( pNos < pStack ){` |
|        - |  6827 | `		goto Abort;` |
|        - |  6828 | `	}` |
|        - |  6829 | `#endif` |
|        - |  6830 | `	/* Force the operands to be real */` |
|       68 |  6831 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6832 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6833 | `	}` |
|       68 |  6834 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6835 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6836 | `	}` |
|        - |  6837 | `	/* Perform the requested operation */` |
|       68 |  6838 | `	a = pNos->rVal;` |
|       68 |  6839 | `	b = pTos->rVal;` |
|       68 |  6840 | `	if( b == 0 ){` |
|        - |  6841 | `		/* Division by zero */` |
|        3 |  6842 | `		pNos->rVal = 0;` |
|        3 |  6843 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6844 | `		/* goto Abort; */` |
|        2 |  6845 | `	}else{` |
|       65 |  6846 | `		r = a/b;` |
|        - |  6847 | `		/* Push the result */` |
|       65 |  6848 | `		pNos->rVal = r;` |
|       65 |  6849 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6850 | `		/* Try to get an integer representation */` |
|       65 |  6851 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6852 | `	}` |
|       68 |  6853 | `	VmPopOperand(&pTos,1);` |
|       68 |  6854 | `	break;` |
|        - |  6855 | `				}` |
|        - |  6856 | `/*` |
|        - |  6857 | ` * OP_DIV_STORE * * *` |
|        - |  6858 | ` *` |
|        - |  6859 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6860 | ` * first (what was next on the stack) from the second (the` |
|        - |  6861 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6862 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6863 | ` */` |
|        2 |  6864 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6865 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6866 | `	ph7_value *pObj;` |
|        - |  6867 | `	ph7_real a,b,r;` |
|        - |  6868 | `#ifdef UNTRUST` |
|        - |  6869 | `	if( pNos < pStack ){` |
|        - |  6870 | `		goto Abort;` |
|        - |  6871 | `	}` |
|        - |  6872 | `#endif` |
|        - |  6873 | `	/* Force the operands to be real */` |
|        5 |  6874 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6875 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6876 | `	}` |
|        5 |  6877 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6878 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6879 | `	}` |
|        - |  6880 | `	/* Perform the requested operation */` |
|        5 |  6881 | `	a = pTos->rVal;` |
|        5 |  6882 | `	b = pNos->rVal;` |
|        5 |  6883 | `	if( b == 0 ){` |
|        - |  6884 | `		/* Division by zero */` |
|      ! 0 |  6885 | `		r = 0;` |
|      ! 0 |  6886 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6887 | `		/* goto Abort; */` |
|      ! 0 |  6888 | `	}else{` |
|        5 |  6889 | `		r = a/b;` |
|        - |  6890 | `		/* Push the result */` |
|        5 |  6891 | `		pNos->rVal = r;` |
|        5 |  6892 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6893 | `		/* Try to get an integer representation */` |
|        5 |  6894 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6895 | `	}` |
|        5 |  6896 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6897 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6898 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6899 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6900 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6901 | `	}` |
|        5 |  6902 | `	VmPopOperand(&pTos,1);` |
|        5 |  6903 | `	break;` |
|        - |  6904 | `				}` |
|        - |  6905 | `/* OP_BAND * * *` |
|        - |  6906 | ` *` |
|        - |  6907 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6908 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6909 | ` * two elements.` |
|        - |  6910 | `*/` |
|        - |  6911 | `/* OP_BOR * * *` |
|        - |  6912 | ` *` |
|        - |  6913 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6914 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6915 | ` * two elements.` |
|        - |  6916 | ` */` |
|        - |  6917 | `/* OP_BXOR * * *` |
|        - |  6918 | ` *` |
|        - |  6919 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6920 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6921 | ` * two elements.` |
|        - |  6922 | ` */` |
|       43 |  6923 | `case PH7_OP_BAND:` |
|        - |  6924 | `case PH7_OP_BOR:` |
|        - |  6925 | `case PH7_OP_BXOR:{` |
|       91 |  6926 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6927 | `	sxi64 a,b,r;` |
|        - |  6928 | `#ifdef UNTRUST` |
|        - |  6929 | `	if( pNos < pStack ){` |
|        - |  6930 | `		goto Abort;` |
|        - |  6931 | `	}` |
|        - |  6932 | `#endif` |
|        - |  6933 | `	/* Force the operands to be integer */` |
|       91 |  6934 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6935 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6936 | `	}` |
|       91 |  6937 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6938 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6939 | `	}` |
|        - |  6940 | `	/* Perform the requested operation */` |
|       91 |  6941 | `	a = pNos->x.iVal;` |
|       91 |  6942 | `	b = pTos->x.iVal;` |
|       91 |  6943 | `	switch(pInstr->iOp){` |
|        7 |  6944 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6945 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6946 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6947 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6948 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6949 | `	case PH7_OP_BAND:` |
|       63 |  6950 | `	default:          r = a&b; break;` |
|        - |  6951 | `	}` |
|        - |  6952 | `	/* Push the result */` |
|       91 |  6953 | `	pNos->x.iVal = r;` |
|       91 |  6954 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       91 |  6955 | `	VmPopOperand(&pTos,1);` |
|       91 |  6956 | `	break;` |
|        - |  6957 | `				 }` |
|        - |  6958 | `/* OP_BAND_STORE * * *` |
|        - |  6959 | ` *` |
|        - |  6960 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6961 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6962 | ` * two elements.` |
|        - |  6963 | `*/` |
|        - |  6964 | `/* OP_BOR_STORE * * *` |
|        - |  6965 | ` *` |
|        - |  6966 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6967 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6968 | ` * two elements.` |
|        - |  6969 | ` */` |
|        - |  6970 | `/* OP_BXOR_STORE * * *` |
|        - |  6971 | ` *` |
|        - |  6972 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6973 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6974 | ` * two elements.` |
|        - |  6975 | ` */` |
|       10 |  6976 | `case PH7_OP_BAND_STORE:` |
|        - |  6977 | `case PH7_OP_BOR_STORE:` |
|        - |  6978 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6979 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6980 | `	ph7_value *pObj;` |
|        - |  6981 | `	sxi64 a,b,r;` |
|        - |  6982 | `#ifdef UNTRUST` |
|        - |  6983 | `	if( pNos < pStack ){` |
|        - |  6984 | `		goto Abort;` |
|        - |  6985 | `	}` |
|        - |  6986 | `#endif` |
|        - |  6987 | `	/* Force the operands to be integer */` |
|       21 |  6988 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6989 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6990 | `	}` |
|       21 |  6991 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6992 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6993 | `	}` |
|        - |  6994 | `	/* Perform the requested operation */` |
|       21 |  6995 | `	a = pTos->x.iVal;` |
|       21 |  6996 | `	b = pNos->x.iVal;` |
|       21 |  6997 | `	switch(pInstr->iOp){` |
|        3 |  6998 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6999 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  7000 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  7001 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  7002 | `	case PH7_OP_BAND_STORE:` |
|        3 |  7003 | `	case PH7_OP_BAND:` |
|        7 |  7004 | `	default:          r = a&b; break;` |
|        - |  7005 | `	}` |
|        - |  7006 | `	/* Push the result */` |
|       21 |  7007 | `	pNos->x.iVal = r;` |
|       21 |  7008 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  7009 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7010 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  7011 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  7012 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  7013 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  7014 | `	}` |
|       21 |  7015 | `	VmPopOperand(&pTos,1);` |
|       21 |  7016 | `	break;` |
|        - |  7017 | `				 }` |
|        - |  7018 | `/* OP_SHL * * *` |
|        - |  7019 | ` *` |
|        - |  7020 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7021 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7022 | ` * left by N bits where N is the top element on the stack.` |
|        - |  7023 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7024 | ` */` |
|        - |  7025 | `/* OP_SHR * * *` |
|        - |  7026 | ` *` |
|        - |  7027 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7028 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7029 | ` * right by N bits where N is the top element on the stack.` |
|        - |  7030 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7031 | ` */` |
|       12 |  7032 | `case PH7_OP_SHL:` |
|        - |  7033 | `case PH7_OP_SHR: {` |
|       25 |  7034 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7035 | `	sxi64 a,r;` |
|        - |  7036 | `	sxi32 b;` |
|        - |  7037 | `#ifdef UNTRUST` |
|        - |  7038 | `	if( pNos < pStack ){` |
|        - |  7039 | `		goto Abort;` |
|        - |  7040 | `	}` |
|        - |  7041 | `#endif` |
|        - |  7042 | `	/* Force the operands to be integer */` |
|       25 |  7043 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7044 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  7045 | `	}` |
|       25 |  7046 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7047 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  7048 | `	}` |
|        - |  7049 | `	/* Perform the requested operation */` |
|       25 |  7050 | `	a = pNos->x.iVal;` |
|       25 |  7051 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  7052 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  7053 | `		r = a << b;` |
|        8 |  7054 | `	}else{` |
|       11 |  7055 | `		r = a >> b;` |
|        - |  7056 | `	}` |
|        - |  7057 | `	/* Push the result */` |
|       25 |  7058 | `	pNos->x.iVal = r;` |
|       25 |  7059 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  7060 | `	VmPopOperand(&pTos,1);` |
|       25 |  7061 | `	break;` |
|        - |  7062 | `				 }` |
|        - |  7063 | `/*  OP_SHL_STORE * * *` |
|        - |  7064 | ` *` |
|        - |  7065 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7066 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7067 | ` * left by N bits where N is the top element on the stack.` |
|        - |  7068 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7069 | ` */` |
|        - |  7070 | `/* OP_SHR_STORE * * *` |
|        - |  7071 | ` *` |
|        - |  7072 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  7073 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  7074 | ` * right by N bits where N is the top element on the stack.` |
|        - |  7075 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  7076 | ` */` |
|        9 |  7077 | `case PH7_OP_SHL_STORE:` |
|        - |  7078 | `case PH7_OP_SHR_STORE: {` |
|       19 |  7079 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7080 | `	ph7_value *pObj;` |
|        - |  7081 | `	sxi64 a,r;` |
|        - |  7082 | `	sxi32 b;` |
|        - |  7083 | `#ifdef UNTRUST` |
|        - |  7084 | `	if( pNos < pStack ){` |
|        - |  7085 | `		goto Abort;` |
|        - |  7086 | `	}` |
|        - |  7087 | `#endif` |
|        - |  7088 | `	/* Force the operands to be integer */` |
|       19 |  7089 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7090 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  7091 | `	}` |
|       19 |  7092 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  7093 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  7094 | `	}` |
|        - |  7095 | `	/* Perform the requested operation */` |
|       19 |  7096 | `	a = pTos->x.iVal;` |
|       19 |  7097 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  7098 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  7099 | `		r = a << b;` |
|        5 |  7100 | `	}else{` |
|       11 |  7101 | `		r = a >> b;` |
|        - |  7102 | `	}` |
|        - |  7103 | `	/* Push the result */` |
|       19 |  7104 | `	pNos->x.iVal = r;` |
|       19 |  7105 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  7106 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7107 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  7108 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  7109 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  7110 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  7111 | `	}` |
|       19 |  7112 | `	VmPopOperand(&pTos,1);` |
|       19 |  7113 | `	break;` |
|        - |  7114 | `				 }` |
|        - |  7115 | `/* CAT:  P1 * *` |
|        - |  7116 | ` *` |
|        - |  7117 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  7118 | ` * back.` |
|        - |  7119 | ` */` |
|    72530 |  7120 | `case PH7_OP_CAT:{` |
|        - |  7121 | `	ph7_value *pNos,*pCur;` |
|   145065 |  7122 | `	if( pInstr->iP1 < 1 ){` |
|   117577 |  7123 | `		pNos = &pTos[-1];` |
|    58791 |  7124 | `	}else{` |
|    27493 |  7125 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  7126 | `	}` |
|        - |  7127 | `#ifdef UNTRUST` |
|        - |  7128 | `	if( pNos < pStack ){` |
|        - |  7129 | `		goto Abort;` |
|        - |  7130 | `	}` |
|        - |  7131 | `#endif` |
|        - |  7132 | `	/* Force a string cast */` |
|   145065 |  7133 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1685 |  7134 | `		PH7_MemObjToString(pNos);` |
|      840 |  7135 | `	}` |
|   145065 |  7136 | `	pCur = &pNos[1];` |
|   292857 |  7137 | `	while( pCur <= pTos ){` |
|   147797 |  7138 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50969 |  7139 | `			PH7_MemObjToString(pCur);` |
|    25482 |  7140 | `		}` |
|        - |  7141 | `		/* Perform the concatenation */` |
|   147797 |  7142 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   147753 |  7143 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  7144 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  7145 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7146 | `				goto Abort;` |
|        - |  7147 | `			}` |
|    73874 |  7148 | `		}` |
|   147797 |  7149 | `		SyBlobRelease(&pCur->sBlob);` |
|   147797 |  7150 | `		pCur++;` |
|        5 |  7151 | `	}` |
|   145065 |  7152 | `	pTos = pNos;` |
|   145065 |  7153 | `	break;` |
|        - |  7154 | `				}` |
|        - |  7155 | `/*  CAT_STORE: * * *` |
|        - |  7156 | ` *` |
|        - |  7157 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  7158 | ` * back.` |
|        - |  7159 | ` */` |
|     4164 |  7160 | `case PH7_OP_CAT_STORE:{` |
|     8333 |  7161 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7162 | `	ph7_value *pObj;` |
|        - |  7163 | `	sxu32 nIdx;` |
|        - |  7164 | `#ifdef UNTRUST` |
|        - |  7165 | `	if( pNos < pStack ){` |
|        - |  7166 | `		goto Abort;` |
|        - |  7167 | `	}` |
|        - |  7168 | `#endif` |
|        - |  7169 | `	/* The right operand must be a string to append it */` |
|     8333 |  7170 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7171 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7172 | `	}` |
|     8333 |  7173 | `	nIdx = pTos->nIdx;` |
|        - |  7174 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  7175 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  7176 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  7177 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  7178 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  7179 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  7180 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  7181 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  7182 | `	 * must run before any mutation (left to the slow path).` |
|        - |  7183 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  7184 | `	 * and remains O(n^2) by design. */` |
|     8331 |  7185 | `	if( nIdx != SXU32_HIGH` |
|     8328 |  7186 | `	 && nIdx != pNos->nIdx` |
|     8324 |  7187 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8325 |  7188 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4163 |  7189 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8319 |  7190 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7191 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  7192 | `			PH7_MemObjToString(pObj);` |
|        1 |  7193 | `		}` |
|     8319 |  7194 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8317 |  7195 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7196 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  7197 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  7198 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7199 | `				goto Abort;` |
|        - |  7200 | `			}` |
|     4156 |  7201 | `		}` |
|        - |  7202 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  7203 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  7204 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  7205 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  7206 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  7207 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  7208 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  7209 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  7210 | `		 * the same slot is appended to again later in the statement` |
|        - |  7211 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  7212 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  7213 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8319 |  7214 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  7215 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  7216 | `		}` |
|     8319 |  7217 | `		pNos->nIdx = SXU32_HIGH;` |
|     8319 |  7218 | `		VmPopOperand(&pTos,1);` |
|     8326 |  7219 | `		break;` |
|        - |  7220 | `	}` |
|        - |  7221 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  7222 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7223 | `		/* Force a string cast */` |
|        6 |  7224 | `		PH7_MemObjToString(pTos);` |
|        2 |  7225 | `	}` |
|        - |  7226 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  7227 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  7228 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7229 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  7230 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  7231 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7232 | `			goto Abort;` |
|        - |  7233 | `		}` |
|        7 |  7234 | `	}` |
|        - |  7235 | `	/* Perform the store operation */` |
|       16 |  7236 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7237 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  7238 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  7239 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  7240 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  7241 | `	}` |
|       11 |  7242 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  7243 | `	VmPopOperand(&pTos,1);` |
|       11 |  7244 | `	break;` |
|        - |  7245 | `				}` |
|        - |  7246 | `/* OP_AND: * * *` |
|        - |  7247 | ` *` |
|        - |  7248 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  7249 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7250 | ` * stack.` |
|        - |  7251 | ` */` |
|        - |  7252 | `/* OP_OR: * * *` |
|        - |  7253 | ` *` |
|        - |  7254 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  7255 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7256 | ` * stack.` |
|        - |  7257 | ` */` |
|   109256 |  7258 | `case PH7_OP_LAND:` |
|        - |  7259 | `case PH7_OP_LOR: {` |
|   218561 |  7260 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7261 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  7262 | `#ifdef UNTRUST` |
|        - |  7263 | `	if( pNos < pStack ){` |
|        - |  7264 | `		goto Abort;` |
|        - |  7265 | `	}` |
|        - |  7266 | `#endif` |
|        - |  7267 | `	/* Force a boolean cast */` |
|   218561 |  7268 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  7269 | `		PH7_MemObjToBool(pTos);` |
|        1 |  7270 | `	}` |
|   218561 |  7271 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7272 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7273 | `	}` |
|   218561 |  7274 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   218561 |  7275 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   218561 |  7276 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  7277 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   100395 |  7278 | `		v1 = and_logic[v1*3+v2];` |
|    50222 |  7279 | `	}else{` |
|        - |  7280 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   118171 |  7281 | `		v1 = or_logic[v1*3+v2];` |
|        - |  7282 | `	}` |
|   218561 |  7283 | `	if( v1 == 2 ){` |
|      ! 0 |  7284 | `		v1 = 1;` |
|      ! 0 |  7285 | `	}` |
|   218561 |  7286 | `	VmPopOperand(&pTos,1);` |
|   218561 |  7287 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   218561 |  7288 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   218561 |  7289 | `	break;` |
|        - |  7290 | `				 }` |
|        - |  7291 | `/*` |
|        - |  7292 | ` * OP_NULLC: * * *` |
|        - |  7293 | ` * Null coalescing operator '??'.` |
|        - |  7294 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  7295 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  7296 | ` */` |
|        - |  7297 | `/*` |
|        - |  7298 | ` * OP_NULLC: * P2 *` |
|        - |  7299 | ` * Short-circuit null coalescing '??'.` |
|        - |  7300 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  7301 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  7302 | ` */` |
|       99 |  7303 | `case PH7_OP_NULLC: {` |
|        - |  7304 | `#ifdef UNTRUST` |
|        - |  7305 | `	if( pTos < pStack ){` |
|        - |  7306 | `		goto Abort;` |
|        - |  7307 | `	}` |
|        - |  7308 | `#endif` |
|      203 |  7309 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7310 | `		/* Left is not null — keep it and skip the RHS */` |
|      123 |  7311 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       64 |  7312 | `	}else{` |
|        - |  7313 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       85 |  7314 | `		VmPopOperand(&pTos, 1);` |
|        - |  7315 | `	}` |
|      203 |  7316 | `	break;` |
|        - |  7317 |  |
|        - |  7318 | `/*` |
|        - |  7319 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  7320 | ` * Null coalescing assignment short-circuit.` |
|        - |  7321 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  7322 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  7323 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  7324 | ` */` |
|       28 |  7325 | `case PH7_OP_NULLC_JMP: {` |
|        - |  7326 | `#ifdef UNTRUST` |
|        - |  7327 | `	if( pTos < pStack ){` |
|        - |  7328 | `		goto Abort;` |
|        - |  7329 | `	}` |
|        - |  7330 | `#endif` |
|       59 |  7331 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  7332 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  7333 | `	}` |
|       59 |  7334 | `	break;` |
|        - |  7335 |  |
|        - |  7336 | `/*` |
|        - |  7337 | ` * OP_NULLC_STORE: * * *` |
|        - |  7338 | ` * Null coalescing assignment store.` |
|        - |  7339 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  7340 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  7341 | ` * expression result.` |
|        - |  7342 | ` */` |
|        - |  7343 | `/*` |
|        - |  7344 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  7345 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  7346 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  7347 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  7348 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  7349 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  7350 | ` */` |
|       51 |  7351 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  7352 | `#ifdef UNTRUST` |
|        - |  7353 | `	if( pTos < pStack ){` |
|        - |  7354 | `		goto Abort;` |
|        - |  7355 | `	}` |
|        - |  7356 | `#endif` |
|      105 |  7357 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  7358 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  7359 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  7360 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  7361 | `	}` |
|      105 |  7362 | `	break;` |
|        - |  7363 |  |
|       17 |  7364 | `case PH7_OP_NULLC_STORE: {` |
|       37 |  7365 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7366 | `	ph7_value *pObj;` |
|        - |  7367 | `	sxu32 nIdx;` |
|        - |  7368 | `#ifdef UNTRUST` |
|        - |  7369 | `	if( pNos < pStack ){` |
|        - |  7370 | `		goto Abort;` |
|        - |  7371 | `	}` |
|        - |  7372 | `#endif` |
|        - |  7373 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  7374 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  7375 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       37 |  7376 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  7377 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  7378 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  7379 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  7380 | `		ph7_value *apArg[2];` |
|        5 |  7381 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  7382 | `		apArg[1] = pTos;` |
|        5 |  7383 | `		if( pSet ){` |
|        5 |  7384 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  7385 | `		}` |
|        - |  7386 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  7387 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  7388 | `		VmPopOperand(&pTos,1);` |
|        - |  7389 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  7390 | `		VmCoalesceDisarm(pVm);` |
|        5 |  7391 | `		break;` |
|        - |  7392 | `	}` |
|       32 |  7393 | `	nIdx = pNos->nIdx;` |
|       32 |  7394 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7395 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7396 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7397 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7398 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7399 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7400 | `	}` |
|       32 |  7401 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7402 | `	VmPopOperand(&pTos,1);` |
|       32 |  7403 | `	break;` |
|        - |  7404 |  |
|        - |  7405 | `/*` |
|        - |  7406 | ` * OP_SPREAD: * * *` |
|        - |  7407 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7408 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7409 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7410 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7411 | ` */` |
|       10 |  7412 | `case PH7_OP_SPREAD: {` |
|        - |  7413 | `#ifdef UNTRUST` |
|        - |  7414 | `	if( pTos < pStack ){` |
|        - |  7415 | `		goto Abort;` |
|        - |  7416 | `	}` |
|        - |  7417 | `#endif` |
|        - |  7418 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - |  7419 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - |  7420 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - |  7421 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - |  7422 | `	 * be freed immediately. */` |
|       23 |  7423 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 |  7424 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - |  7425 | `		sxi32 rcW;` |
|        - |  7426 | `		sxu32 nEnt;` |
|        3 |  7427 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 |  7428 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 |  7429 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  7430 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 |  7431 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 |  7432 | `			goto Exception;` |
|        - |  7433 | `		}` |
|        3 |  7434 | `		nEnt = pTmpMap->nEntry;` |
|        3 |  7435 | `		if( nEnt == 0 ){` |
|      ! 0 |  7436 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7437 | `			pVm->iSpreadExtra--;` |
|        3 |  7438 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEnt - 1) >= VM_STACK_GUARD ){` |
|      ! 0 |  7439 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7440 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)", VM_STACK_GUARD);` |
|      ! 0 |  7441 | `		}else{` |
|        3 |  7442 | `			ph7_hashmap_node *pNodeT = pTmpMap->pFirst;` |
|        - |  7443 | `			ph7_value *pElemT;` |
|        - |  7444 | `			sxu32 iT;` |
|        3 |  7445 | `			pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        3 |  7446 | `			if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }else{ PH7_MemObjRelease(pTos); }` |
|        3 |  7447 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  7448 | `			pNodeT = pNodeT->pPrev;` |
|        7 |  7449 | `			for( iT = 1; iT < nEnt; iT++ ){` |
|        5 |  7450 | `				pTos++;` |
|        5 |  7451 | `				PH7_MemObjInit(pVm, pTos);` |
|        5 |  7452 | `				pTos->nIdx = SXU32_HIGH;` |
|        5 |  7453 | `				pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        5 |  7454 | `				if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }` |
|        5 |  7455 | `				pNodeT = pNodeT->pPrev;` |
|        3 |  7456 | `			}` |
|        3 |  7457 | `			pVm->iSpreadExtra += (sxi32)(nEnt - 1);` |
|        - |  7458 | `		}` |
|        3 |  7459 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 |  7460 | `		break;` |
|        - |  7461 | `	}` |
|       21 |  7462 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       21 |  7463 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       21 |  7464 | `		sxu32 nEntry = pMap->nEntry;` |
|       21 |  7465 | `		if( nEntry == 0 ){` |
|        - |  7466 | `			/* Empty array — remove from stack */` |
|        3 |  7467 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7468 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       20 |  7469 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7470 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7471 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7472 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7473 | `				VM_STACK_GUARD);` |
|      ! 0 |  7474 | `		}else{` |
|        - |  7475 | `			ph7_hashmap_node *pNode2;` |
|        - |  7476 | `			ph7_value *pElem;` |
|        - |  7477 | `			sxu32 i;` |
|        - |  7478 | `			/* Overwrite TOS with first element */` |
|       19 |  7479 | `			pNode2 = pMap->pFirst;` |
|       19 |  7480 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       19 |  7481 | `			PH7_MemObjRelease(pTos);` |
|       19 |  7482 | `			if( pElem ){` |
|       19 |  7483 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7484 | `			}` |
|       19 |  7485 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7486 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7487 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       19 |  7488 | `			pNode2 = pNode2->pPrev;` |
|        - |  7489 | `			/* Push remaining elements */` |
|       45 |  7490 | `			for( i = 1; i < nEntry; i++ ){` |
|       29 |  7491 | `				pTos++;` |
|       29 |  7492 | `				PH7_MemObjInit(pVm, pTos);` |
|       29 |  7493 | `				pTos->nIdx = SXU32_HIGH;` |
|       29 |  7494 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       29 |  7495 | `				if( pElem ){` |
|       29 |  7496 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7497 | `				}` |
|       29 |  7498 | `				pNode2 = pNode2->pPrev;` |
|       16 |  7499 | `			}` |
|       19 |  7500 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7501 | `		}` |
|        9 |  7502 | `	}` |
|        - |  7503 | `	/* else: not an array — leave as-is (single arg) */` |
|       21 |  7504 | `	break;` |
|        - |  7505 |  |
|        - |  7506 | `/*` |
|        - |  7507 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7508 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7509 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7510 | ` */` |
|       37 |  7511 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7512 | `#ifdef UNTRUST` |
|        - |  7513 | `	if( pTos < pStack ){` |
|        - |  7514 | `		goto Abort;` |
|        - |  7515 | `	}` |
|        - |  7516 | `#endif` |
|       77 |  7517 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       77 |  7518 | `	break;` |
|        - |  7519 |  |
|        - |  7520 | `/* OP_LXOR: * * *` |
|        - |  7521 | ` *` |
|        - |  7522 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7523 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7524 | ` * stack.` |
|        - |  7525 | ` * According to the PHP language reference manual:` |
|        - |  7526 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7527 | ` *  TRUE,but not both.` |
|        - |  7528 | ` */` |
|        5 |  7529 | `case PH7_OP_LXOR:{` |
|       11 |  7530 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7531 | `	sxi32 v = 0;` |
|        - |  7532 | `#ifdef UNTRUST` |
|        - |  7533 | `	if( pNos < pStack ){` |
|        - |  7534 | `		goto Abort;` |
|        - |  7535 | `	}` |
|        - |  7536 | `#endif` |
|        - |  7537 | `	/* Force a boolean cast */` |
|       11 |  7538 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7539 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7540 | `	}` |
|       11 |  7541 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7542 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7543 | `	}` |
|       11 |  7544 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7545 | `		v = 1;` |
|        3 |  7546 | `	}` |
|       11 |  7547 | `	VmPopOperand(&pTos,1);` |
|       11 |  7548 | `	pTos->x.iVal = v;` |
|       11 |  7549 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7550 | `	break;` |
|        - |  7551 | `				 }` |
|        - |  7552 | `/* OP_EQ P1 P2 P3` |
|        - |  7553 | ` *` |
|        - |  7554 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7555 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7556 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7557 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7558 | ` */` |
|        - |  7559 | `/* OP_NEQ P1 P2 P3` |
|        - |  7560 | ` *` |
|        - |  7561 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7562 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7563 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7564 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7565 | ` */` |
|     4639 |  7566 | `case PH7_OP_EQ:` |
|        - |  7567 | `case PH7_OP_NEQ: {` |
|     9283 |  7568 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7569 | `	/* Perform the comparison and act accordingly */` |
|        - |  7570 | `#ifdef UNTRUST` |
|        - |  7571 | `	if( pNos < pStack ){` |
|        - |  7572 | `		goto Abort;` |
|        - |  7573 | `	}` |
|        - |  7574 | `#endif` |
|     9283 |  7575 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9283 |  7576 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7577 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9274 |  7578 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9239 |  7579 | `		rc = rc == 0;` |
|     4622 |  7580 | `	}else{` |
|       31 |  7581 | `		rc = rc != 0;` |
|        - |  7582 | `	}` |
|     9283 |  7583 | `	VmPopOperand(&pTos,1);` |
|     9283 |  7584 | `	if( !pInstr->iP2 ){` |
|        - |  7585 | `		/* Push comparison result without taking the jump */` |
|     9283 |  7586 | `		PH7_MemObjRelease(pTos);` |
|     9283 |  7587 | `		pTos->x.iVal = rc;` |
|        - |  7588 | `		/* Invalidate any prior representation */` |
|     9283 |  7589 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4644 |  7590 | `	}else{` |
|      ! 0 |  7591 | `		if( rc ){` |
|        - |  7592 | `			/* Jump to the desired location */` |
|      ! 0 |  7593 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7594 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7595 | `		}` |
|        - |  7596 | `	}` |
|     9283 |  7597 | `	break;` |
|        - |  7598 | `				 }` |
|        - |  7599 | `/* OP_TEQ P1 P2 *` |
|        - |  7600 | ` *` |
|        - |  7601 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7602 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7603 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7604 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7605 | ` */` |
|   163839 |  7606 | `case PH7_OP_TEQ: {` |
|   327683 |  7607 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7608 | `	/* Perform the comparison and act accordingly */` |
|        - |  7609 | `#ifdef UNTRUST` |
|        - |  7610 | `	if( pNos < pStack ){` |
|        - |  7611 | `		goto Abort;` |
|        - |  7612 | `	}` |
|        - |  7613 | `#endif` |
|   327683 |  7614 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   327683 |  7615 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7616 | `		rc = 0;` |
|        2 |  7617 | `	}else{` |
|   327681 |  7618 | `		rc = rc == 0;` |
|        - |  7619 | `	}` |
|   327683 |  7620 | `	VmPopOperand(&pTos,1);` |
|   327683 |  7621 | `	if( !pInstr->iP2 ){` |
|        - |  7622 | `		/* Push comparison result without taking the jump */` |
|   327683 |  7623 | `		PH7_MemObjRelease(pTos);` |
|   327683 |  7624 | `		pTos->x.iVal = rc;` |
|        - |  7625 | `		/* Invalidate any prior representation */` |
|   327683 |  7626 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   163844 |  7627 | `	}else{` |
|      ! 0 |  7628 | `		if( rc ){` |
|        - |  7629 | `			/* Jump to the desired location */` |
|      ! 0 |  7630 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7631 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7632 | `		}` |
|        - |  7633 | `	}` |
|   327683 |  7634 | `	break;` |
|        - |  7635 | `				 }` |
|        - |  7636 | `/* OP_TNE P1 P2 *` |
|        - |  7637 | ` *` |
|        - |  7638 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7639 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7640 | ` * instruction.` |
|        - |  7641 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7642 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7643 | ` *` |
|        - |  7644 | ` */` |
|   126036 |  7645 | `case PH7_OP_TNE: {` |
|   252077 |  7646 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7647 | `	/* Perform the comparison and act accordingly */` |
|        - |  7648 | `#ifdef UNTRUST` |
|        - |  7649 | `	if( pNos < pStack ){` |
|        - |  7650 | `		goto Abort;` |
|        - |  7651 | `	}` |
|        - |  7652 | `#endif` |
|   252077 |  7653 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   252077 |  7654 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7655 | `		rc = 1;` |
|        2 |  7656 | `	}else{` |
|   252075 |  7657 | `		rc = rc != 0;` |
|        - |  7658 | `	}` |
|   252077 |  7659 | `	VmPopOperand(&pTos,1);` |
|   252077 |  7660 | `	if( !pInstr->iP2 ){` |
|        - |  7661 | `		/* Push comparison result without taking the jump */` |
|   252077 |  7662 | `		PH7_MemObjRelease(pTos);` |
|   252077 |  7663 | `		pTos->x.iVal = rc;` |
|        - |  7664 | `		/* Invalidate any prior representation */` |
|   252077 |  7665 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   126041 |  7666 | `	}else{` |
|      ! 0 |  7667 | `		if( rc ){` |
|        - |  7668 | `			/* Jump to the desired location */` |
|      ! 0 |  7669 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7670 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7671 | `		}` |
|        - |  7672 | `	}` |
|   252077 |  7673 | `	break;` |
|        - |  7674 | `				 }` |
|        - |  7675 | `/* OP_LT P1 P2 P3` |
|        - |  7676 | ` *` |
|        - |  7677 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7678 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7679 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7680 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7681 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7682 | ` *` |
|        - |  7683 | ` */` |
|        - |  7684 | `/* OP_LE P1 P2 P3` |
|        - |  7685 | ` *` |
|        - |  7686 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7687 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7688 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7689 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7690 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7691 | ` *` |
|        - |  7692 | ` */` |
|   113156 |  7693 | `case PH7_OP_LT:` |
|        - |  7694 | `case PH7_OP_LE: {` |
|   226361 |  7695 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7696 | `	/* Perform the comparison and act accordingly */` |
|        - |  7697 | `#ifdef UNTRUST` |
|        - |  7698 | `	if( pNos < pStack ){` |
|        - |  7699 | `		goto Abort;` |
|        - |  7700 | `	}` |
|        - |  7701 | `#endif` |
|   226361 |  7702 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   226361 |  7703 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7704 | `		rc = 0;` |
|   226357 |  7705 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1773 |  7706 | `		rc = rc < 1;` |
|      889 |  7707 | `	}else{` |
|   224585 |  7708 | `		rc = rc < 0;` |
|        - |  7709 | `	}` |
|   226361 |  7710 | `	VmPopOperand(&pTos,1);` |
|   226361 |  7711 | `	if( !pInstr->iP2 ){` |
|        - |  7712 | `		/* Push comparison result without taking the jump */` |
|   226361 |  7713 | `		PH7_MemObjRelease(pTos);` |
|   226361 |  7714 | `		pTos->x.iVal = rc;` |
|        - |  7715 | `		/* Invalidate any prior representation */` |
|   226361 |  7716 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   113205 |  7717 | `	}else{` |
|      ! 0 |  7718 | `		if( rc ){` |
|        - |  7719 | `			/* Jump to the desired location */` |
|      ! 0 |  7720 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7721 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7722 | `		}` |
|        - |  7723 | `	}` |
|   226361 |  7724 | `	break;` |
|        - |  7725 | `				}` |
|        - |  7726 | `/* OP_GT P1 P2 P3` |
|        - |  7727 | ` *` |
|        - |  7728 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7729 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7730 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7731 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7732 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7733 | ` *` |
|        - |  7734 | ` */` |
|        - |  7735 | `/* OP_GE P1 P2 P3` |
|        - |  7736 | ` *` |
|        - |  7737 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7738 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7739 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7740 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7741 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7742 | ` *` |
|        - |  7743 | ` */` |
|    56137 |  7744 | `case PH7_OP_GT:` |
|        - |  7745 | `case PH7_OP_GE: {` |
|   112279 |  7746 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7747 | `	/* Perform the comparison and act accordingly */` |
|        - |  7748 | `#ifdef UNTRUST` |
|        - |  7749 | `	if( pNos < pStack ){` |
|        - |  7750 | `		goto Abort;` |
|        - |  7751 | `	}` |
|        - |  7752 | `#endif` |
|   112279 |  7753 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   112279 |  7754 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7755 | `		rc = 0;` |
|   112275 |  7756 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   111839 |  7757 | `		rc = rc >= 0;` |
|    55922 |  7758 | `	}else{` |
|      437 |  7759 | `		rc = rc > 0;` |
|        - |  7760 | `	}` |
|   112279 |  7761 | `	VmPopOperand(&pTos,1);` |
|   112279 |  7762 | `	if( !pInstr->iP2 ){` |
|        - |  7763 | `		/* Push comparison result without taking the jump */` |
|   112279 |  7764 | `		PH7_MemObjRelease(pTos);` |
|   112279 |  7765 | `		pTos->x.iVal = rc;` |
|        - |  7766 | `		/* Invalidate any prior representation */` |
|   112279 |  7767 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    56142 |  7768 | `	}else{` |
|      ! 0 |  7769 | `		if( rc ){` |
|        - |  7770 | `			/* Jump to the desired location */` |
|      ! 0 |  7771 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7772 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7773 | `		}` |
|        - |  7774 | `	}` |
|   112279 |  7775 | `	break;` |
|        - |  7776 | `				}` |
|        - |  7777 | `/* OP_SPACESHIP * * *` |
|        - |  7778 | ` *` |
|        - |  7779 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7780 | ` *   -1 if left < right` |
|        - |  7781 | ` *    0 if left == right` |
|        - |  7782 | ` *    1 if left > right` |
|        - |  7783 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7784 | ` */` |
|       25 |  7785 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7786 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7787 | `#ifdef UNTRUST` |
|        - |  7788 | `	if( pNos < pStack ){` |
|        - |  7789 | `		goto Abort;` |
|        - |  7790 | `	}` |
|        - |  7791 | `#endif` |
|       51 |  7792 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7793 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7794 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7795 | `		rc = 1;` |
|        4 |  7796 | `	}else{` |
|        - |  7797 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7798 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7799 | `	}` |
|       51 |  7800 | `	VmPopOperand(&pTos,1);` |
|       51 |  7801 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7802 | `	pTos->x.iVal = rc;` |
|       51 |  7803 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7804 | `	break;` |
|        - |  7805 | `				}` |
|        - |  7806 | `/* OP_SEQ P1 P2 *` |
|        - |  7807 | ` * Strict string comparison.` |
|        - |  7808 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7809 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7810 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7811 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7812 | ` * use PH7_OP_EQ.` |
|        - |  7813 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7814 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7815 | ` */` |
|        - |  7816 | `/* OP_SNE P1 P2 *` |
|        - |  7817 | ` * Strict string comparison.` |
|        - |  7818 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7819 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7820 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7821 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7822 | ` * use PH7_OP_EQ.` |
|        - |  7823 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7824 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7825 | ` */` |
|       18 |  7826 | `case PH7_OP_SEQ:` |
|        - |  7827 | `case PH7_OP_SNE: {` |
|       38 |  7828 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7829 | `	SyString s1,s2;` |
|        - |  7830 | `	/* Perform the comparison and act accordingly */` |
|        - |  7831 | `#ifdef UNTRUST` |
|        - |  7832 | `	if( pNos < pStack ){` |
|        - |  7833 | `		goto Abort;` |
|        - |  7834 | `	}` |
|        - |  7835 | `#endif` |
|        - |  7836 | `	/* Force a string cast */` |
|       38 |  7837 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7838 | `		PH7_MemObjToString(pTos);` |
|        2 |  7839 | `	}` |
|       38 |  7840 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7841 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7842 | `	}` |
|       38 |  7843 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7844 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7845 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7846 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7847 | `		rc = rc != 0;` |
|      ! 0 |  7848 | `	}else{` |
|       38 |  7849 | `		rc = rc == 0;` |
|        - |  7850 | `	}` |
|       38 |  7851 | `	VmPopOperand(&pTos,1);` |
|       38 |  7852 | `	if( !pInstr->iP2 ){` |
|        - |  7853 | `		/* Push comparison result without taking the jump */` |
|       38 |  7854 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7855 | `		pTos->x.iVal = rc;` |
|        - |  7856 | `		/* Invalidate any prior representation */` |
|       38 |  7857 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7858 | `	}else{` |
|      ! 0 |  7859 | `		if( rc ){` |
|        - |  7860 | `			/* Jump to the desired location */` |
|      ! 0 |  7861 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7862 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7863 | `		}` |
|        - |  7864 | `	}` |
|       38 |  7865 | `	break;` |
|        - |  7866 | `				 }` |
|        - |  7867 | `/*` |
|        - |  7868 | ` * OP_LOAD_REF * * *` |
|        - |  7869 | ` * Push the index of a referenced object on the stack.` |
|        - |  7870 | ` */` |
|       60 |  7871 | `case PH7_OP_LOAD_REF: {` |
|        - |  7872 | `	sxu32 nIdx;` |
|        - |  7873 | `#ifdef UNTRUST` |
|        - |  7874 | `	if( pTos < pStack ){` |
|        - |  7875 | `		goto Abort;` |
|        - |  7876 | `	}` |
|        - |  7877 | `#endif` |
|        - |  7878 | `	/* Extract memory object index */` |
|      121 |  7879 | `	nIdx = pTos->nIdx;` |
|      121 |  7880 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7881 | `		/* Nullify the object */` |
|      101 |  7882 | `		PH7_MemObjRelease(pTos);` |
|        - |  7883 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7884 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7885 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7886 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7887 | `	}` |
|      121 |  7888 | `	break;` |
|        - |  7889 | `					  }` |
|        - |  7890 | `/*` |
|        - |  7891 | ` * OP_STORE_REF * * P3` |
|        - |  7892 | ` * Perform an assignment operation by reference.` |
|        - |  7893 | ` */` |
|       18 |  7894 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7895 | `	 SyString sName = { 0 , 0 };` |
|        - |  7896 | `	 VmFrame *pFrameLocal;` |
|        - |  7897 | `	SyHashEntry *pEntry;` |
|        - |  7898 | `	sxu32 nIdx;` |
|        - |  7899 | `#ifdef UNTRUST` |
|        - |  7900 | `	if( pTos < pStack ){` |
|        - |  7901 | `		goto Abort;` |
|        - |  7902 | `	}` |
|        - |  7903 | `#endif` |
|       38 |  7904 | `	if( pInstr->p3 == 0 ){` |
|        - |  7905 | `		char *zName;` |
|        - |  7906 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7907 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7908 | `			/* Force a string cast */` |
|      ! 0 |  7909 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7910 | `		}` |
|      ! 0 |  7911 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7912 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7913 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7914 | `			if( zName ){` |
|      ! 0 |  7915 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7916 | `			}` |
|      ! 0 |  7917 | `		}` |
|      ! 0 |  7918 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7919 | `		pTos--;` |
|      ! 0 |  7920 | `	}else{` |
|       38 |  7921 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7922 | `	}` |
|       38 |  7923 | `	nIdx = pTos->nIdx;` |
|       38 |  7924 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7925 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7926 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7927 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7928 | `		}else{` |
|        - |  7929 | `			ph7_value *pObj;` |
|        - |  7930 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7931 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7932 | `			if( pObj == 0 ){` |
|      ! 0 |  7933 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7934 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7935 | `				goto Abort;` |
|        - |  7936 | `			}` |
|        - |  7937 | `			/* Perform the store operation */` |
|      ! 0 |  7938 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7939 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7940 | `		}` |
|       38 |  7941 | `	}else if( sName.nByte > 0){` |
|       38 |  7942 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7943 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7944 | `		}else{` |
|       38 |  7945 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  7946 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7947 | `			/* Query the local frame */` |
|       38 |  7948 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  7949 | `			if( pEntry ){` |
|      ! 0 |  7950 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7951 | `			}else{` |
|       38 |  7952 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  7953 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7954 | `					/* Insert in the $GLOBALS array */` |
|       34 |  7955 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  7956 | `				}` |
|       38 |  7957 | `				if( rc == SXRET_OK ){` |
|       38 |  7958 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  7959 | `				}` |
|        - |  7960 | `			}` |
|        - |  7961 | `		}` |
|       18 |  7962 | `	}` |
|       38 |  7963 | `	break;` |
|        - |  7964 | `				 }` |
|        - |  7965 | `/*` |
|        - |  7966 | ` * OP_UPLINK P1 * *` |
|        - |  7967 | ` * Link a variable to the top active VM frame.` |
|        - |  7968 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7969 | ` */` |
|       30 |  7970 | `case PH7_OP_UPLINK: {` |
|       65 |  7971 | `	if( pVm->pFrame->pParent ){` |
|       65 |  7972 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7973 | `		SyString sName;` |
|        - |  7974 | `		/* Perform the link */` |
|      135 |  7975 | `		while( pLink <= pTos ){` |
|       75 |  7976 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7977 | `				/* Force a string cast */` |
|      ! 0 |  7978 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7979 | `			}` |
|       75 |  7980 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       75 |  7981 | `			if( sName.nByte > 0 ){` |
|       75 |  7982 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7983 | `			}` |
|       75 |  7984 | `			pLink++;` |
|        5 |  7985 | `		}` |
|       30 |  7986 | `	}` |
|       65 |  7987 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       65 |  7988 | `	break;` |
|        - |  7989 | `					}` |
|        - |  7990 | `/*` |
|        - |  7991 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7992 | ` * Push an exception in the corresponding container so that` |
|        - |  7993 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7994 | ` */` |
|      222 |  7995 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      449 |  7996 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7997 | `	VmFrame *pFrameLocal;` |
|        - |  7998 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      449 |  7999 | `	pException->iFinallyDone = 0;` |
|      449 |  8000 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  8001 | `	/* Create the exception frame */` |
|      449 |  8002 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      449 |  8003 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8004 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  8005 | `		goto Abort;` |
|        - |  8006 | `	}` |
|        - |  8007 | `	/* Mark the special frame */` |
|      449 |  8008 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      449 |  8009 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  8010 | `	/* Point to the frame that trigger the exception */` |
|      449 |  8011 | `	pFrameLocal = pFrameLocal->pParent;` |
|      449 |  8012 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      449 |  8013 | `	pException->pFrame = pFrameLocal;` |
|      449 |  8014 | `	break;` |
|        - |  8015 | `							}` |
|        - |  8016 | `/*` |
|        - |  8017 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  8018 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  8019 | ` */` |
|      216 |  8020 | `case PH7_OP_POP_EXCEPTION: {` |
|      437 |  8021 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      437 |  8022 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8023 | `		ph7_exception **apException;` |
|        - |  8024 | `		/* Pop the loaded exception */` |
|       36 |  8025 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       36 |  8026 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       32 |  8027 | `			(void)SySetPop(&pVm->aException);` |
|       15 |  8028 | `		}` |
|       17 |  8029 | `	}` |
|      437 |  8030 | `	pException->pFrame = 0;` |
|        - |  8031 | `	/* Leave the exception frame */` |
|      437 |  8032 | `	VmLeaveFrame(&(*pVm));` |
|        - |  8033 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      437 |  8034 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  8035 | `		sxi32 rcFinally;` |
|       22 |  8036 | `		pException->iFinallyDone = 1;` |
|       22 |  8037 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|       22 |  8038 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  8039 | `			goto Abort;` |
|        - |  8040 | `		}` |
|       10 |  8041 | `	}` |
|      437 |  8042 | `	if( pVm->bReturnRequested ){` |
|        - |  8043 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - |  8044 | `		 * function. Drain outer finally blocks first, then — only in the real` |
|        - |  8045 | `		 * function body — materialize; inside a mini-program propagate outward. */` |
|       29 |  8046 | `		rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|       29 |  8047 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8048 | `			goto Abort;` |
|        - |  8049 | `		}` |
|       29 |  8050 | `		if( !bReturnPropagates ){` |
|       27 |  8051 | `			VmMaterializeCatchReturn(&(*pVm),pResult,pEntryFrame);` |
|       13 |  8052 | `		}` |
|       29 |  8053 | `		goto Done;` |
|        - |  8054 | `	}` |
|      409 |  8055 | `	break;` |
|        - |  8056 | `							}` |
|        - |  8057 |  |
|        - |  8058 | `/*` |
|        - |  8059 | ` * OP_THROW * P2 *` |
|        - |  8060 | ` * Throw an user exception.` |
|        - |  8061 | ` */` |
|      104 |  8062 | `case PH7_OP_THROW: {` |
|      213 |  8063 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      213 |  8064 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  8065 | `#ifdef UNTRUST` |
|        - |  8066 | `	if( pTos < pStack ){` |
|        - |  8067 | `		goto Abort;` |
|        - |  8068 | `	}` |
|        - |  8069 | `#endif` |
|      213 |  8070 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  8071 | `	/* Tell the upper layer that an exception was thrown */` |
|      213 |  8072 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      213 |  8073 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      213 |  8074 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8075 | `		ph7_class *pThrowable;` |
|        - |  8076 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      213 |  8077 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      214 |  8078 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  8079 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  8080 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  8081 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  8082 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  8083 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  8084 | `			if( pErrorClass ){` |
|        3 |  8085 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  8086 | `			}` |
|        3 |  8087 | `			if( pErrInst ){` |
|        - |  8088 | `				ph7_class_method *pCons;` |
|        3 |  8089 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  8090 | `				if( pCons ){` |
|        - |  8091 | `					ph7_value sArg;` |
|        - |  8092 | `					ph7_value *apArg[1];` |
|        - |  8093 | `					SyString sMsgStr;` |
|        - |  8094 | `					static const char zErrMsg[] =` |
|        - |  8095 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  8096 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  8097 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  8098 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  8099 | `					apArg[0] = &sArg;` |
|        3 |  8100 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  8101 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  8102 | `				}` |
|        3 |  8103 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  8104 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  8105 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8106 | `					goto Abort;` |
|        - |  8107 | `				}` |
|        2 |  8108 | `			}else{` |
|        - |  8109 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  8110 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  8111 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8112 | `					goto Abort;` |
|        - |  8113 | `				}` |
|        - |  8114 | `			}` |
|        2 |  8115 | `		}else{` |
|        - |  8116 | `			/* Throw the exception */` |
|      211 |  8117 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      211 |  8118 | `			if( rc == SXERR_ABORT ){` |
|        - |  8119 | `				/* Abort processing immediately */` |
|       14 |  8120 | `				goto Abort;` |
|        - |  8121 | `			}` |
|        - |  8122 | `		}` |
|      104 |  8123 | `	}else{` |
|        - |  8124 | `		/* Expecting a class instance */` |
|      ! 0 |  8125 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  8126 | `		if( rc == SXERR_ABORT ){` |
|        - |  8127 | `			/* Abort processing immediately */` |
|      ! 0 |  8128 | `			goto Abort;` |
|        - |  8129 | `		}` |
|        - |  8130 | `	}` |
|        - |  8131 | `	/* Pop the top entry */` |
|      203 |  8132 | `	VmPopOperand(&pTos,1);` |
|        - |  8133 | `	/* Perform an unconditional jump to the try's OP_POP_EXCEPTION landing pad,` |
|        - |  8134 | `	 * which tears down the try frame, runs finally, and (when a catch/finally` |
|        - |  8135 | ``	 * issued a `return`) consumes pVm->bReturnRequested. Routing the return`` |
|        - |  8136 | `	 * through OP_POP_EXCEPTION keeps the frame stack balanced. */` |
|      203 |  8137 | `	pc = nJump - 1;` |
|      203 |  8138 | `	break;` |
|        - |  8139 | `				   }` |
|        - |  8140 | `/*` |
|        - |  8141 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  8142 | ` * Prepare a foreach step.` |
|        - |  8143 | ` */` |
|     6276 |  8144 | `case PH7_OP_FOREACH_INIT: {` |
|    12557 |  8145 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  8146 | `	void *pName;` |
|        - |  8147 | `#ifdef UNTRUST` |
|        - |  8148 | `	if( pTos < pStack ){` |
|        - |  8149 | `		goto Abort;` |
|        - |  8150 | `	}` |
|        - |  8151 | `#endif` |
|    12557 |  8152 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  8153 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  8154 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8155 | `			/* Force a string cast */` |
|      ! 0 |  8156 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  8157 | `		}` |
|        - |  8158 | `		/* Duplicate name */` |
|      ! 0 |  8159 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  8160 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8161 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8162 | `		}` |
|      ! 0 |  8163 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  8164 | `	}` |
|    12557 |  8165 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  8166 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8167 | `			/* Force a string cast */` |
|      ! 0 |  8168 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  8169 | `		}` |
|        - |  8170 | `		/* Duplicate name */` |
|      ! 0 |  8171 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  8172 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8173 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8174 | `		}` |
|      ! 0 |  8175 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  8176 | `	}` |
|        - |  8177 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12557 |  8178 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  8179 | `		/* Jump out of the loop */` |
|      ! 0 |  8180 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8181 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  8182 | `		}` |
|      ! 0 |  8183 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  8184 | `	}else{` |
|        - |  8185 | `		ph7_foreach_step *pStep;` |
|    12557 |  8186 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12557 |  8187 | `		if( pStep == 0 ){` |
|      ! 0 |  8188 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  8189 | `			/* Jump out of the loop */` |
|      ! 0 |  8190 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  8191 | `		}else{` |
|        - |  8192 | `			/* Zero the structure */` |
|    12557 |  8193 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  8194 | `			/* Prepare the step */` |
|    12557 |  8195 | `			pStep->iFlags = pInfo->iFlags;` |
|    12557 |  8196 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8197 | `				ph7_hashmap *pMap;` |
|        - |  8198 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  8199 | `				 * source array so mutations don't affect other sharers. */` |
|    12523 |  8200 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  8201 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  8202 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  8203 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  8204 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  8205 | `						 * variable still points at the same hashmap as` |
|        - |  8206 | `						 * the stack value. */` |
|        9 |  8207 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  8208 | `							pCur->iRef--;` |
|        - |  8209 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  8210 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  8211 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  8212 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  8213 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  8214 | `						}` |
|        4 |  8215 | `					}` |
|        4 |  8216 | `				}` |
|    12523 |  8217 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  8218 | `				/* Reset the internal loop cursor */` |
|    12523 |  8219 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8220 | `				/* Mark the step */` |
|    12523 |  8221 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12523 |  8222 | `				pStep->xIter.pMap = pMap;` |
|    12523 |  8223 | `				pMap->iRef++;` |
|     6264 |  8224 | `			}else{` |
|       39 |  8225 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8226 | `				ph7_class *pIteratorClass;` |
|        - |  8227 | `				/* Check if the object implements Iterator */` |
|       39 |  8228 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       50 |  8229 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  8230 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  8231 | `					ph7_class_method *pRewind;` |
|       26 |  8232 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       26 |  8233 | `					pStep->xIter.pThis = pThis;` |
|       26 |  8234 | `					pThis->iRef++;` |
|       26 |  8235 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       26 |  8236 | `					if( pRewind ){` |
|       26 |  8237 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  8238 | `					}` |
|       15 |  8239 | `				}else{` |
|        - |  8240 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  8241 | `					ph7_class *pIterAggClass;` |
|       14 |  8242 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  8243 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  8244 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  8245 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  8246 | `						ph7_class_method *pGetIter;` |
|        3 |  8247 | `						int iterAggOk = 0;` |
|        3 |  8248 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  8249 | `						if( pGetIter ){` |
|        - |  8250 | `							ph7_value sResult;` |
|        3 |  8251 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  8252 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  8253 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  8254 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  8255 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  8256 | `									ph7_class_method *pRewind;` |
|        3 |  8257 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  8258 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  8259 | `									pIterObj->iRef++;` |
|        - |  8260 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  8261 | `									pStep->pOwner = pThis;` |
|        3 |  8262 | `									pThis->iRef++;` |
|        3 |  8263 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  8264 | `									if( pRewind ){` |
|        3 |  8265 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  8266 | `									}` |
|        3 |  8267 | `									iterAggOk = 1;` |
|        1 |  8268 | `								}` |
|        1 |  8269 | `							}` |
|        3 |  8270 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  8271 | `						}` |
|        3 |  8272 | `						if( !iterAggOk ){` |
|        - |  8273 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  8274 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8275 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  8276 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  8277 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  8278 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  8279 | `						}` |
|        2 |  8280 | `					}else{` |
|        - |  8281 | `						/* Plain object iteration via hAttr */` |
|       12 |  8282 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  8283 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  8284 | `						pStep->xIter.pThis = pThis;` |
|       12 |  8285 | `						pThis->iRef++;` |
|        - |  8286 | `					}` |
|        - |  8287 | `				}` |
|        - |  8288 | `			}` |
|        - |  8289 | `		}` |
|    12557 |  8290 | `		if( pStep ){` |
|    12557 |  8291 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  8292 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  8293 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  8294 | `				/* Jump out of the loop */` |
|      ! 0 |  8295 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  8296 | `			}` |
|     6276 |  8297 | `		}` |
|        - |  8298 | `	}` |
|    12557 |  8299 | `	VmPopOperand(&pTos,1);` |
|    12557 |  8300 | `	break;` |
|        - |  8301 | `						  }` |
|        - |  8302 | `/*` |
|        - |  8303 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  8304 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  8305 | ` */` |
|   103275 |  8306 | `case PH7_OP_FOREACH_STEP: {` |
|   206555 |  8307 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  8308 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  8309 | `	ph7_value *pValue;` |
|        - |  8310 | `	VmFrame *pFrameLocal;` |
|        - |  8311 | `	/* Peek the last step */` |
|   206555 |  8312 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   206555 |  8313 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   206555 |  8314 | `	pFrameLocal = pVm->pFrame;` |
|   206555 |  8315 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   206555 |  8316 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   206421 |  8317 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  8318 | `		ph7_hashmap_node *pNode;` |
|        - |  8319 | `		/* Extract the current node value */` |
|   206421 |  8320 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   206421 |  8321 | `		if( pNode == 0 ){` |
|        - |  8322 | `			/* No more entry to process */` |
|    12521 |  8323 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12521 |  8324 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8325 | `				/* Break the reference with the last element */` |
|        7 |  8326 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  8327 | `			}` |
|        - |  8328 | `			/* Automatically reset the loop cursor */` |
|    12521 |  8329 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8330 | `			/* Cleanup the mess left behind */` |
|    12521 |  8331 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12521 |  8332 | `			SySetPop(&pInfo->aStep);` |
|    12521 |  8333 | `			PH7_HashmapUnref(pMap);` |
|     6263 |  8334 | `		}else{` |
|   193905 |  8335 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      531 |  8336 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      531 |  8337 | `				if( pKey ){` |
|      531 |  8338 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  8339 | `				}` |
|      263 |  8340 | `			}` |
|   193905 |  8341 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8342 | `				SyHashEntry *pEntry;` |
|        - |  8343 | `				/* Pass by reference */` |
|       23 |  8344 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  8345 | `				if( pEntry ){` |
|       21 |  8346 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  8347 | `				}else{` |
|        4 |  8348 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  8349 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  8350 | `				}` |
|       12 |  8351 | `			}else{` |
|        - |  8352 | `				/* Make a copy of the entry value */` |
|   193883 |  8353 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   193883 |  8354 | `				if( pValue ){` |
|   193883 |  8355 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    96939 |  8356 | `				}` |
|        - |  8357 | `			}` |
|        5 |  8358 | `		}` |
|   103347 |  8359 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  8360 | `		/* Iterator-based iteration.` |
|        - |  8361 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  8362 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  8363 | `		 */` |
|      109 |  8364 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  8365 | `		ph7_class_method *pMethod;` |
|        - |  8366 | `		ph7_value sResult;` |
|      109 |  8367 | `		int isValid = 0;` |
|        - |  8368 | `		/* Call next() to advance — but skip on the first iteration */` |
|      109 |  8369 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       29 |  8370 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       17 |  8371 | `		}else{` |
|       85 |  8372 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       85 |  8373 | `			if( pMethod ){` |
|       85 |  8374 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  8375 | `			}` |
|        - |  8376 | `		}` |
|        - |  8377 | `		/* Call valid() */` |
|      109 |  8378 | `		PH7_MemObjInit(pVm,&sResult);` |
|      109 |  8379 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      109 |  8380 | `		if( pMethod ){` |
|      109 |  8381 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      109 |  8382 | `			PH7_MemObjToBool(&sResult);` |
|      109 |  8383 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  8384 | `		}` |
|      109 |  8385 | `		PH7_MemObjRelease(&sResult);` |
|      109 |  8386 | `		if( !isValid ){` |
|        - |  8387 | `			/* Iterator exhausted */` |
|       27 |  8388 | `			pc = pInstr->iP2 - 1;` |
|        - |  8389 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       27 |  8390 | `			if( pStep->pOwner ){` |
|        3 |  8391 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  8392 | `			}` |
|       27 |  8393 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       27 |  8394 | `			SySetPop(&pInfo->aStep);` |
|       27 |  8395 | `			PH7_ClassInstanceUnref(pThis);` |
|       16 |  8396 | `		}else{` |
|        - |  8397 | `			/* Call current() to get value */` |
|       87 |  8398 | `			PH7_MemObjInit(pVm,&sResult);` |
|       87 |  8399 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       87 |  8400 | `			if( pMethod ){` |
|       87 |  8401 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  8402 | `			}` |
|       87 |  8403 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       87 |  8404 | `			if( pValue ){` |
|       87 |  8405 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  8406 | `			}` |
|       87 |  8407 | `			PH7_MemObjRelease(&sResult);` |
|        - |  8408 | `			/* Call key() if needed */` |
|       87 |  8409 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  8410 | `				ph7_value sKey;` |
|       37 |  8411 | `				PH7_MemObjInit(pVm,&sKey);` |
|       37 |  8412 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       37 |  8413 | `				if( pMethod ){` |
|       37 |  8414 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  8415 | `				}` |
|       37 |  8416 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       37 |  8417 | `				if( pValue ){` |
|       37 |  8418 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  8419 | `				}` |
|       37 |  8420 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  8421 | `			}` |
|        - |  8422 | `		}` |
|       57 |  8423 | `	}else{` |
|       32 |  8424 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  8425 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  8426 | `		SyHashEntry *pEntry;` |
|        - |  8427 | `		/* Point to the next attribute */` |
|       36 |  8428 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  8429 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  8430 | `			/* Check access permission */` |
|       38 |  8431 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  8432 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  8433 | `					break; /* Access is granted */` |
|        - |  8434 | `			}` |
|        1 |  8435 | `		}` |
|       32 |  8436 | `		if( pEntry == 0 ){` |
|        - |  8437 | `			/* Clean up the mess left behind */` |
|       12 |  8438 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  8439 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8440 | `				/* Break the reference with the last element */` |
|        3 |  8441 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  8442 | `			}` |
|       12 |  8443 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  8444 | `			SySetPop(&pInfo->aStep);` |
|       12 |  8445 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  8446 | `		}else{` |
|       22 |  8447 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  8448 | `			ph7_value *pAttrValue;` |
|       22 |  8449 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  8450 | `				/* Fill with the current attribute name */` |
|       22 |  8451 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8452 | `				if( pKey ){` |
|       22 |  8453 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8454 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8455 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8456 | `				}` |
|       10 |  8457 | `			}` |
|        - |  8458 | `			/* Extract attribute value */` |
|       22 |  8459 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8460 | `			if( pAttrValue ){` |
|       22 |  8461 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8462 | `					/* Pass by reference */` |
|        3 |  8463 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8464 | `					if( pEntry ){` |
|        3 |  8465 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8466 | `					}else{` |
|      ! 0 |  8467 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8468 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8469 | `					}` |
|        2 |  8470 | `				}else{` |
|        - |  8471 | `					/* Make a copy of the attribute value */` |
|       20 |  8472 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8473 | `					if( pValue ){` |
|       20 |  8474 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8475 | `					}` |
|        - |  8476 | `				}` |
|       10 |  8477 | `			}` |
|        - |  8478 | `		}` |
|        - |  8479 | `	}` |
|   206555 |  8480 | `	break;` |
|        - |  8481 | `						  }` |
|        - |  8482 | `/*` |
|        - |  8483 | ` * OP_MEMBER P1 P2` |
|        - |  8484 | ` * Load class attribute/method on the stack.` |
|        - |  8485 | ` */` |
|     4302 |  8486 | `case PH7_OP_MEMBER: {` |
|        - |  8487 | `	ph7_class_instance *pThis;` |
|        - |  8488 | `	ph7_value *pNos;` |
|        - |  8489 | `	SyString sName;` |
|     8609 |  8490 | `	if( !pInstr->iP1 ){` |
|     8327 |  8491 | `		pNos = &pTos[-1];` |
|        - |  8492 | `#ifdef UNTRUST` |
|        - |  8493 | `		if( pNos < pStack ){` |
|        - |  8494 | `			goto Abort;` |
|        - |  8495 | `		}` |
|        - |  8496 | `#endif` |
|     8327 |  8497 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8498 | `			ph7_class *pClass;` |
|        - |  8499 | `			/* Class already instantiated */` |
|     8325 |  8500 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8501 | `			/* Point to the instantiated class */` |
|     8325 |  8502 | `			pClass = pThis->pClass;` |
|        - |  8503 | `			/* Extract attribute name first */` |
|     8325 |  8504 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     8325 |  8505 | `			if( pInstr->iP2 ){` |
|        - |  8506 | `				/* Method call */` |
|      805 |  8507 | `				ph7_class_method *pMeth = 0;` |
|      805 |  8508 | `				if( sName.nByte > 0 ){` |
|        - |  8509 | `					/* Extract the target method */` |
|      805 |  8510 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      400 |  8511 | `				}` |
|      805 |  8512 | `				if( pMeth == 0 ){` |
|      ! 0 |  8513 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8514 | `						&pClass->sName,&sName` |
|        - |  8515 | `						);` |
|        - |  8516 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8517 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8518 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8519 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8520 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8521 | `				}else{` |
|        - |  8522 | `					/* Push method name on the stack */` |
|      805 |  8523 | `					PH7_MemObjRelease(pTos);` |
|      805 |  8524 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      805 |  8525 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8526 | `				}` |
|      805 |  8527 | `				pTos->nIdx = SXU32_HIGH;` |
|      405 |  8528 | `			}else{` |
|        - |  8529 | `				/* Attribute access */` |
|     7525 |  8530 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8531 | `				SyHashEntry *pEntry;` |
|        - |  8532 | `				/* Extract the target attribute */` |
|     7525 |  8533 | `				if( sName.nByte > 0 ){` |
|     7525 |  8534 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7525 |  8535 | `					if( pEntry ){` |
|        - |  8536 | `						/* Point to the attribute value */` |
|     7523 |  8537 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3759 |  8538 | `					}` |
|     3760 |  8539 | `				}` |
|     7525 |  8540 | `				if( pObjAttr == 0 ){` |
|        - |  8541 | `					/* No such attribute,load null */` |
|        4 |  8542 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8543 | `						&pClass->sName,&sName);` |
|        - |  8544 | `					/* Call the __get magic method if available */` |
|        3 |  8545 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8546 | `				}` |
|     7525 |  8547 | `				VmPopOperand(&pTos,1);` |
|        - |  8548 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8549 | `				 * This is due to the following case:` |
|        - |  8550 | `				 *     (new TestClass())->foo;` |
|        - |  8551 | `				 */` |
|     7525 |  8552 | `				pThis->iRef++;` |
|     7525 |  8553 | `				PH7_MemObjRelease(pTos);` |
|     7525 |  8554 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7525 |  8555 | `				if( pObjAttr ){` |
|     7523 |  8556 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8557 | `					/* Check attribute access */` |
|     7523 |  8558 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8559 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8560 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8561 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8562 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8563 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7518 |  8564 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3804 |  8565 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       84 |  8566 | `							VmInstr *pNext = pInstr + 1;` |
|       84 |  8567 | `							int bIsLhs = 0;` |
|       84 |  8568 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       82 |  8569 | `								bIsLhs = 1;` |
|       39 |  8570 | `							}` |
|       84 |  8571 | `							if( !bIsLhs ){` |
|        3 |  8572 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8573 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8574 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8575 | `									goto Abort;` |
|        - |  8576 | `								}` |
|        - |  8577 | `								{` |
|        3 |  8578 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8579 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8580 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4302 |  8581 | `										break;` |
|        - |  8582 | `									}` |
|        - |  8583 | `								}` |
|      ! 0 |  8584 | `								goto Exception;` |
|        - |  8585 | `							}` |
|       39 |  8586 | `						}` |
|        - |  8587 | `						/* Load attribute */` |
|     7521 |  8588 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7521 |  8589 | `						if( pValue ){` |
|     7521 |  8590 | `							if( pThis->iRef < 2 ){` |
|        - |  8591 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8592 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8593 | `								 */` |
|        7 |  8594 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8595 | `							}else{` |
|        - |  8596 | `								/* Simple load */` |
|     7515 |  8597 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8598 | `							}` |
|     7521 |  8599 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7519 |  8600 | `								if( pThis->iRef > 1 ){` |
|        - |  8601 | `									/* Load attribute index */` |
|     7513 |  8602 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3754 |  8603 | `								}` |
|     3757 |  8604 | `							}` |
|     3758 |  8605 | `						}` |
|     3763 |  8606 | `					}else{` |
|        - |  8607 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8608 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8609 | `						char zMsg[256];` |
|      ! 0 |  8610 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8611 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8612 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8613 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8614 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8615 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8616 | `						goto Abort;` |
|        - |  8617 | `					}` |
|     3758 |  8618 | `				}` |
|        - |  8619 | `				/* Safely unreference the object */` |
|     7523 |  8620 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8621 | `			}` |
|     4164 |  8622 | `		}else{` |
|        3 |  8623 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8624 | `			VmPopOperand(&pTos,1);` |
|        3 |  8625 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8626 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8627 | `		}` |
|     4165 |  8628 | `	}else{` |
|        - |  8629 | `		/* Static member access using class name */` |
|      287 |  8630 | `		pNos = pTos;` |
|      287 |  8631 | `		pThis = 0;` |
|      287 |  8632 | `		if( !pInstr->p3 ){` |
|      237 |  8633 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      237 |  8634 | `			pNos--;` |
|        - |  8635 | `#ifdef UNTRUST` |
|        - |  8636 | `			if( pNos < pStack ){` |
|        - |  8637 | `				goto Abort;` |
|        - |  8638 | `			}` |
|        - |  8639 | `#endif` |
|      121 |  8640 | `		}else{` |
|        - |  8641 | `			/* Attribute name already computed */` |
|       54 |  8642 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8643 | `		}` |
|      287 |  8644 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      287 |  8645 | `			ph7_class *pClass = 0;` |
|      287 |  8646 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8647 | `				/* Class already instantiated */` |
|        5 |  8648 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8649 | `				pClass = pThis->pClass;` |
|        5 |  8650 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8651 | `			}else{` |
|        - |  8652 | `				/* Try to extract the target class */` |
|      283 |  8653 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      283 |  8654 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      283 |  8655 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8656 | `					/* Handle self/static/parent keywords */` |
|      283 |  8657 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       65 |  8658 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       65 |  8659 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8660 | `							/* In a trait method, self:: resolves to the using class */` |
|       14 |  8661 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       11 |  8662 | `						}` |
|      253 |  8663 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       29 |  8664 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      223 |  8665 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       30 |  8666 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  8667 | `						if( pSelf && pSelf->pBase ){` |
|       30 |  8668 | `							pClass = pSelf->pBase;` |
|       13 |  8669 | `						}` |
|       17 |  8670 | `					}else{` |
|      171 |  8671 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8672 | `					}` |
|      139 |  8673 | `				}` |
|        - |  8674 | `			}` |
|      287 |  8675 | `			if( pClass == 0 ){` |
|        - |  8676 | `				/* Undefined class */` |
|      ! 0 |  8677 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8678 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8679 | `					);` |
|      ! 0 |  8680 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8681 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8682 | `				}` |
|      ! 0 |  8683 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8684 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8685 | `			}else{` |
|      287 |  8686 | `				if( pInstr->iP2 ){` |
|        - |  8687 | `					/* Method call */` |
|       89 |  8688 | `					ph7_class_method *pMeth = 0;` |
|       89 |  8689 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8690 | `						/* Extract the target method */` |
|       89 |  8691 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8692 | `					}` |
|       89 |  8693 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8694 | `						if( pMeth ){` |
|      ! 0 |  8695 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8696 | `								&pClass->sName,&sName` |
|        - |  8697 | `								);` |
|      ! 0 |  8698 | `						}else{` |
|      ! 0 |  8699 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8700 | `								&pClass->sName,&sName` |
|        - |  8701 | `								);` |
|        - |  8702 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8703 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8704 | `						}` |
|        - |  8705 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8706 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8707 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8708 | `						}` |
|      ! 0 |  8709 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8710 | `					}else{` |
|        - |  8711 | `						/* Push method name on the stack */` |
|       89 |  8712 | `						PH7_MemObjRelease(pTos);` |
|       89 |  8713 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       89 |  8714 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8715 | `					}` |
|       89 |  8716 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 |  8717 | `				}else{` |
|        - |  8718 | `					/* Attribute access */` |
|      203 |  8719 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8720 | `					/* Check for special ::class pseudo-constant */` |
|      249 |  8721 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8722 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8723 | `						/* ::class returns the fully qualified class name */` |
|        - |  8724 | `						/* Pop the attribute name from the stack */` |
|       62 |  8725 | `						if( !pInstr->p3 ){` |
|       62 |  8726 | `							VmPopOperand(&pTos,1);` |
|       29 |  8727 | `						}` |
|       62 |  8728 | `						PH7_MemObjRelease(pTos);` |
|        - |  8729 | `						/* Load the class name */` |
|       62 |  8730 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       62 |  8731 | `						pTos->nIdx = SXU32_HIGH;` |
|       33 |  8732 | `					}else{` |
|        - |  8733 | `						/* Extract the target attribute */` |
|      144 |  8734 | `						if( sName.nByte > 0 ){` |
|      144 |  8735 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       70 |  8736 | `						}` |
|      144 |  8737 | `						if( pAttr == 0 ){` |
|        - |  8738 | `							/* No such attribute,load null */` |
|      ! 0 |  8739 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8740 | `								&pClass->sName,&sName);` |
|        - |  8741 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8742 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8743 | `						}` |
|        - |  8744 | `						/* Pop the attribute name from the stack */` |
|      144 |  8745 | `						if( !pInstr->p3 ){` |
|       92 |  8746 | `							VmPopOperand(&pTos,1);` |
|       45 |  8747 | `						}` |
|      144 |  8748 | `						PH7_MemObjRelease(pTos);` |
|      144 |  8749 | `						pTos->nIdx = SXU32_HIGH;` |
|      144 |  8750 | `						if( pAttr ){` |
|      144 |  8751 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8752 | `								/* Access to a non static attribute */` |
|      ! 0 |  8753 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8754 | `									&pClass->sName,&pAttr->sName` |
|        - |  8755 | `									);` |
|      ! 0 |  8756 | `							}else{` |
|        - |  8757 | `								ph7_value *pValue;` |
|        - |  8758 | `								/* Check if the access to the attribute is allowed */` |
|      144 |  8759 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8760 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8761 | `									 * Same LHS-of-store peek as the instance path. */` |
|      136 |  8762 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|      105 |  8763 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       60 |  8764 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8765 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       41 |  8766 | `										if( pS ){` |
|       41 |  8767 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       41 |  8768 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8769 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8770 | `												int bIsLhs = 0;` |
|        8 |  8771 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8772 | `													bIsLhs = 1;` |
|        2 |  8773 | `												}` |
|        8 |  8774 | `												if( !bIsLhs ){` |
|        3 |  8775 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8776 | `													if( pThis ){` |
|      ! 0 |  8777 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8778 | `													}` |
|        3 |  8779 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8780 | `														goto Abort;` |
|        - |  8781 | `													}` |
|        - |  8782 | `													{` |
|        3 |  8783 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8784 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8785 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8786 | `															break;` |
|        - |  8787 | `														}` |
|        - |  8788 | `													}` |
|      ! 0 |  8789 | `													goto Exception;` |
|        - |  8790 | `												}` |
|        2 |  8791 | `											}` |
|       18 |  8792 | `										}` |
|       18 |  8793 | `									}` |
|        - |  8794 | `									/* Load the desired attribute */` |
|      138 |  8795 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|      138 |  8796 | `									if( pValue ){` |
|      138 |  8797 | `										PH7_MemObjLoad(pValue,pTos);` |
|      138 |  8798 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8799 | `											/* Load index number */` |
|       52 |  8800 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8801 | `										}` |
|       67 |  8802 | `									}` |
|       71 |  8803 | `								}else{` |
|        - |  8804 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8805 | `									char zMsg[256];` |
|        5 |  8806 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8807 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8808 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8809 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8810 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8811 | `									}else{` |
|      ! 0 |  8812 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8813 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8814 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8815 | `									}` |
|        5 |  8816 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8817 | `									goto Abort;` |
|        - |  8818 | `								}` |
|        - |  8819 | `							}` |
|       67 |  8820 | `						}` |
|        - |  8821 | `					}` |
|        - |  8822 | `				}` |
|      281 |  8823 | `				if( pThis ){` |
|        - |  8824 | `					/* Safely unreference the object */` |
|        5 |  8825 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8826 | `				}` |
|        - |  8827 | `			}` |
|      143 |  8828 | `		}else{` |
|        - |  8829 | `			/* Pop operands */` |
|      ! 0 |  8830 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8831 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8832 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8833 | `			}` |
|      ! 0 |  8834 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8835 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8836 | `		}` |
|        - |  8837 | `	}` |
|     8601 |  8838 | `	break;` |
|        - |  8839 | `					}` |
|        - |  8840 | `/*` |
|        - |  8841 | ` * OP_NEW P1 * * *` |
|        - |  8842 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8843 | ` */` |
|      700 |  8844 | `case PH7_OP_NEW: {` |
|     1405 |  8845 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1405 |  8846 | `	ph7_class *pClass = 0;` |
|        - |  8847 | `	ph7_class_instance *pNew;` |
|     1405 |  8848 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8849 | `		/* Try to extract the desired class */` |
|     2105 |  8850 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1400 |  8851 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      700 |  8852 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8853 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8854 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8855 | `	}` |
|     1405 |  8856 | `	if( pClass == 0 ){` |
|        - |  8857 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8858 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8859 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8860 | `			);` |
|        - |  8861 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8862 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8863 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8864 | `			/* Pop given arguments */` |
|      ! 0 |  8865 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8866 | `		}` |
|      ! 0 |  8867 | `		goto Abort;` |
|      ! 0 |  8868 | `	}else{` |
|        - |  8869 | `		ph7_class_method *pCons;` |
|        - |  8870 | `		/* Create a new class instance */` |
|     1405 |  8871 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1405 |  8872 | `		if( pNew == 0 ){` |
|      ! 0 |  8873 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8874 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8875 | `				&pClass->sName` |
|        - |  8876 | `			);` |
|      ! 0 |  8877 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8878 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8879 | `				/* Pop given arguments */` |
|      ! 0 |  8880 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8881 | `			}` |
|      ! 0 |  8882 | `			break;` |
|        - |  8883 | `		}` |
|        - |  8884 | `		/* Check if a constructor is available */` |
|     1405 |  8885 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1405 |  8886 | `		if( pCons == 0 ){` |
|      955 |  8887 | `			SyString *pName = &pClass->sName;` |
|        - |  8888 | `			/* Check for a constructor with the same base class name */` |
|      955 |  8889 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      475 |  8890 | `		}` |
|     1405 |  8891 | `		if( pCons ){` |
|        - |  8892 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8893 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8894 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8895 | `			 * (including variadic string-key packing). */` |
|      455 |  8896 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8897 | `			sxi32 rcCons;` |
|      455 |  8898 | `			SySetReset(&aArg);` |
|      883 |  8899 | `			while( pArg < pTos ){` |
|      433 |  8900 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      433 |  8901 | `				pArg++;` |
|        5 |  8902 | `			}` |
|      455 |  8903 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8904 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8905 | `				sxu32 n;` |
|      121 |  8906 | `				n = SySetUsed(&aArg);` |
|        - |  8907 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8908 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8909 | `				 * after resolution). */` |
|      237 |  8910 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      121 |  8911 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      121 |  8912 | `					if( pFuncArg ){` |
|      121 |  8913 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        8 |  8914 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8915 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8916 | `						}` |
|       58 |  8917 | `					}` |
|      121 |  8918 | `					n++;` |
|        5 |  8919 | `				}` |
|       58 |  8920 | `			}` |
|      455 |  8921 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8922 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      455 |  8923 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8924 | `				pNew->iRef = 1;` |
|      ! 0 |  8925 | `			}` |
|      455 |  8926 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8927 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8928 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8929 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8930 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8931 | `				sxi32 iResumePc;` |
|        5 |  8932 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8933 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8934 | `					goto Abort;` |
|        - |  8935 | `				}` |
|        5 |  8936 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8937 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8938 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8939 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8940 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8941 | `					}` |
|        5 |  8942 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8943 | `					pc = iResumePc;` |
|        5 |  8944 | `					break;` |
|        - |  8945 | `				}` |
|      ! 0 |  8946 | `				goto Exception;` |
|        - |  8947 | `			}` |
|      223 |  8948 | `		}` |
|     1401 |  8949 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8950 | `			/* Pop given arguments */` |
|      363 |  8951 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      179 |  8952 | `		}` |
|     1401 |  8953 | `		PH7_MemObjRelease(pTos);` |
|     1401 |  8954 | `		pTos->x.pOther = pNew;` |
|     1401 |  8955 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8956 | `	}` |
|     1401 |  8957 | `	break;` |
|        - |  8958 | `				 }` |
|        - |  8959 | `/*` |
|        - |  8960 | ` * OP_CLONE * * *` |
|        - |  8961 | ` * Perfome a clone operation.` |
|        - |  8962 | ` */` |
|       24 |  8963 | `case PH7_OP_CLONE: {` |
|        - |  8964 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8965 | `#ifdef UNTRUST` |
|        - |  8966 | `	if( pTos < pStack ){` |
|        - |  8967 | `		goto Abort;` |
|        - |  8968 | `	}` |
|        - |  8969 | `#endif` |
|        - |  8970 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8971 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8972 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8973 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8974 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8975 | `		break;` |
|        - |  8976 | `	}` |
|        - |  8977 | `	/* Point to the source */` |
|       46 |  8978 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8979 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8980 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8981 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8982 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8983 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8984 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8985 | `		break;` |
|        - |  8986 | `	}` |
|        - |  8987 | `	/* Perform the clone operation */` |
|       46 |  8988 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8989 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8990 | `	if( pClone == 0 ){` |
|      ! 0 |  8991 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8992 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8993 | `	}else{` |
|        - |  8994 | `		/* Load the cloned object */` |
|       46 |  8995 | `		pTos->x.pOther = pClone;` |
|       46 |  8996 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8997 | `	}` |
|       46 |  8998 | `	break;` |
|        - |  8999 | `				   }` |
|        - |  9000 | `/*` |
|        - |  9001 | ` * OP_SWITCH * * P3` |
|        - |  9002 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  9003 | ` */` |
|       26 |  9004 | `case PH7_OP_SWITCH: {` |
|       57 |  9005 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  9006 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  9007 | `	ph7_value sValue,sCaseValue;` |
|        - |  9008 | `	sxu32 n,nEntry;` |
|        - |  9009 | `#ifdef UNTRUST` |
|        - |  9010 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  9011 | `		goto Abort;` |
|        - |  9012 | `	}` |
|        - |  9013 | `#endif` |
|        - |  9014 | `	/* Point to the case table  */` |
|       57 |  9015 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       57 |  9016 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  9017 | `	/* Select the appropriate case block to execute */` |
|       57 |  9018 | `	PH7_MemObjInit(pVm,&sValue);` |
|       57 |  9019 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      135 |  9020 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      133 |  9021 | `		pCase = &aCase[n];` |
|      133 |  9022 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  9023 | `		/* Execute the case expression first */` |
|      133 |  9024 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|        - |  9025 | `		/* Compare the two expression */` |
|      133 |  9026 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      133 |  9027 | `		PH7_MemObjRelease(&sValue);` |
|      133 |  9028 | `		PH7_MemObjRelease(&sCaseValue);` |
|      133 |  9029 | `		if( rc == 0 ){` |
|        - |  9030 | `			/* Value match,jump to this block */` |
|       55 |  9031 | `			pc = pCase->nStart - 1;` |
|       55 |  9032 | `			break;` |
|        - |  9033 | `		}` |
|       44 |  9034 | `	}` |
|       57 |  9035 | `	VmPopOperand(&pTos,1);` |
|       57 |  9036 | `	if( n >= nEntry ){` |
|        - |  9037 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  9038 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  9039 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  9040 | `		}else{` |
|        - |  9041 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  9042 | `			pc = pSwitch->nOut - 1;` |
|        - |  9043 | `		}` |
|        1 |  9044 | `	}` |
|       57 |  9045 | `	break;` |
|        - |  9046 | `					}` |
|        - |  9047 | `/*` |
|        - |  9048 | ` * OP_MATCH * * P3` |
|        - |  9049 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  9050 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  9051 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  9052 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  9053 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  9054 | ` */` |
|       54 |  9055 | `case PH7_OP_MATCH: {` |
|      111 |  9056 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      111 |  9057 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  9058 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  9059 | `	sxu32 i,j,nArm,nCond;` |
|      111 |  9060 | `	int matched = 0;` |
|        - |  9061 | `#ifdef UNTRUST` |
|        - |  9062 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  9063 | `		goto Abort;` |
|        - |  9064 | `	}` |
|        - |  9065 | `#endif` |
|      111 |  9066 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      111 |  9067 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      111 |  9068 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      111 |  9069 | `	PH7_MemObjInit(pVm,&sCond);` |
|      111 |  9070 | `	PH7_MemObjInit(pVm,&sResult);` |
|      111 |  9071 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      349 |  9072 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  9073 | `		pArm = &aArm[i];` |
|      240 |  9074 | `		if( pArm->bDefault ){` |
|       13 |  9075 | `			pDefault = pArm;` |
|       13 |  9076 | `			continue;` |
|        - |  9077 | `		}` |
|      228 |  9078 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  9079 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  9080 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  9081 | `			if( pCondBc == 0 ){` |
|      ! 0 |  9082 | `				continue;` |
|        - |  9083 | `			}` |
|      260 |  9084 | `			VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|      260 |  9085 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  9086 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  9087 | `			if( rc == 0 ){` |
|       93 |  9088 | `				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|       93 |  9089 | `				matched = 1;` |
|       93 |  9090 | `				break;` |
|        - |  9091 | `			}` |
|       85 |  9092 | `		}` |
|      115 |  9093 | `	}` |
|      111 |  9094 | `	if( !matched && pDefault ){` |
|       13 |  9095 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|       13 |  9096 | `		matched = 1;` |
|        6 |  9097 | `	}` |
|      111 |  9098 | `	if( !matched ){` |
|        6 |  9099 | `		const char *zType = "unknown";` |
|        - |  9100 | `		char zMsg[128];` |
|        - |  9101 | `		sxu32 nMsg;` |
|        6 |  9102 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  9103 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  9104 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        6 |  9105 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  9106 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  9107 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  9108 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  9109 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  9110 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  9111 | `		default: break;` |
|        - |  9112 | `		}` |
|        8 |  9113 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  9114 | `			"Unhandled match case of type %s",zType);` |
|        8 |  9115 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  9116 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        6 |  9117 | `		PH7_MemObjRelease(&sSubject);` |
|        6 |  9118 | `		PH7_MemObjRelease(&sResult);` |
|        6 |  9119 | `		goto Abort;` |
|        - |  9120 | `	}` |
|      105 |  9121 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  9122 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  9123 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  9124 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  9125 | `	break;` |
|        - |  9126 | `					}` |
|        - |  9127 | `/*` |
|        - |  9128 | ` * OP_YIELD P1 P2 *` |
|        - |  9129 | ` *  Yield a value from a generator function.` |
|        - |  9130 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  9131 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  9132 | ` */` |
|       62 |  9133 | `case PH7_OP_YIELD: {` |
|        - |  9134 | `	ph7_generator *pGen;` |
|      129 |  9135 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  9136 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  9137 | `		goto Abort;` |
|        - |  9138 | `	}` |
|      129 |  9139 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|      129 |  9140 | `	if( pInstr->iP2 ){` |
|        - |  9141 | `		/* yield $key => $value: value on top, key below */` |
|        - |  9142 | `#ifdef UNTRUST` |
|        - |  9143 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  9144 | `#endif` |
|       20 |  9145 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 |  9146 | `		VmPopOperand(&pTos, 1);` |
|       20 |  9147 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 |  9148 | `		VmPopOperand(&pTos, 1);` |
|        - |  9149 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 |  9150 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  9151 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  9152 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  9153 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  9154 | `			}` |
|        2 |  9155 | `		}` |
|      120 |  9156 | `	}else if( pInstr->iP1 ){` |
|        - |  9157 | `		/* yield $value */` |
|        - |  9158 | `#ifdef UNTRUST` |
|        - |  9159 | `		if( pTos < pStack ) goto Abort;` |
|        - |  9160 | `#endif` |
|      111 |  9161 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      111 |  9162 | `		VmPopOperand(&pTos, 1);` |
|        - |  9163 | `		/* Auto-increment key */` |
|      111 |  9164 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      111 |  9165 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      111 |  9166 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       58 |  9167 | `	}else{` |
|        - |  9168 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  9169 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9170 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9171 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  9172 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  9173 | `	}` |
|        - |  9174 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|      129 |  9175 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|      129 |  9176 | `	goto Suspend;` |
|        - |  9177 |  |
|        - |  9178 | `/*` |
|        - |  9179 | ` * OP_CALL P1 * *` |
|        - |  9180 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  9181 | ` *  function on the stack.` |
|        - |  9182 | ` */` |
|   363620 |  9183 | `case PH7_OP_CALL: {` |
|   727289 |  9184 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  9185 | `	ph7_value *pArg;` |
|   727289 |  9186 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   727289 |  9187 | `	pArg = &pTos[-nCallArgs];` |
|        - |  9188 | `	SyHashEntry *pEntry;` |
|        - |  9189 | `	SyString sName;` |
|        - |  9190 | `	/* Extract function name */` |
|   727289 |  9191 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  9192 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  9193 | `			ph7_value sResult;` |
|        - |  9194 | `			sxi32 rcArr;` |
|        3 |  9195 | `			SySetReset(&aArg);` |
|        3 |  9196 | `			while( pArg < pTos ){` |
|      ! 0 |  9197 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  9198 | `				pArg++;` |
|      ! 0 |  9199 | `			}` |
|        3 |  9200 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9201 | `			/* May be a class instance and it's static method */` |
|        3 |  9202 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  9203 | `			SySetReset(&aArg);` |
|        - |  9204 | `			/* Pop given arguments */` |
|        3 |  9205 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9206 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9207 | `			}` |
|        3 |  9208 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  9209 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9210 | `				goto Abort;` |
|        - |  9211 | `			}` |
|        3 |  9212 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  9213 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  9214 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  9215 | `				sxi32 iResumePc;` |
|        3 |  9216 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  9217 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  9218 | `					PH7_MemObjRelease(pTos);` |
|        3 |  9219 | `					pc = iResumePc;` |
|        3 |  9220 | `					break;` |
|        - |  9221 | `				}` |
|      ! 0 |  9222 | `				goto Exception;` |
|        - |  9223 | `			}` |
|        - |  9224 | `			/* Copy result */` |
|      ! 0 |  9225 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  9226 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  9227 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  9228 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  9229 | `			ph7_value sResult;` |
|        - |  9230 | `			sxi32 rcInv;` |
|       84 |  9231 | `			SySetReset(&aArg);` |
|      200 |  9232 | `			while( pArg < pTos ){` |
|      118 |  9233 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  9234 | `				pArg++;` |
|        2 |  9235 | `			}` |
|       84 |  9236 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  9237 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  9238 | `				(int)SySetUsed(&aArg),` |
|       82 |  9239 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  9240 | `				&sResult,` |
|       82 |  9241 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  9242 | `			SySetReset(&aArg);` |
|       84 |  9243 | `			if( nCallArgs > 0 ){` |
|       76 |  9244 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  9245 | `			}` |
|       84 |  9246 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  9247 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  9248 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  9249 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  9250 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  9251 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  9252 | `				pThis->iRef++;` |
|       13 |  9253 | `				PH7_MemObjRelease(pTos);` |
|       13 |  9254 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  9255 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  9256 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9257 | `					goto Abort;` |
|        - |  9258 | `				}` |
|        - |  9259 | `				{` |
|       13 |  9260 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  9261 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  9262 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  9263 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  9264 | `						break;` |
|        - |  9265 | `					}` |
|        - |  9266 | `				}` |
|      ! 0 |  9267 | `				goto Exception;` |
|        - |  9268 | `			}` |
|       72 |  9269 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  9270 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9271 | `				goto Abort;` |
|        - |  9272 | `			}` |
|       72 |  9273 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  9274 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  9275 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  9276 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  9277 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  9278 | `				sxi32 iResumePc;` |
|        7 |  9279 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  9280 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  9281 | `					PH7_MemObjRelease(pTos);` |
|        5 |  9282 | `					pc = iResumePc;` |
|        5 |  9283 | `					break;` |
|        - |  9284 | `				}` |
|        3 |  9285 | `				goto Exception;` |
|        - |  9286 | `			}` |
|       66 |  9287 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  9288 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  9289 | `		}else{` |
|        - |  9290 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  9291 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  9292 | `			/* Pop given arguments */` |
|      ! 0 |  9293 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9294 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9295 | `			}` |
|        - |  9296 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9297 | `			PH7_MemObjRelease(pTos);` |
|        - |  9298 | `		}` |
|       66 |  9299 | `		break;` |
|        - |  9300 | `	}` |
|   727205 |  9301 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  9302 | `	/* Check for a compiled function first.` |
|        - |  9303 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  9304 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   727205 |  9305 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9306 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  9307 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  9308 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  9309 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  9310 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  9311 | `	{` |
|   727205 |  9312 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   727205 |  9313 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  9314 | `		const char *zFunc;` |
|        - |  9315 | `		const char *zEnd;` |
|        - |  9316 | `		const char *z;` |
|        - |  9317 | `		SyString sGlobal;` |
|       24 |  9318 | `		zFunc = sName.zString;` |
|       24 |  9319 | `		zEnd  = zFunc + sName.nByte;` |
|       24 |  9320 | `		z = zEnd;` |
|        - |  9321 | `		/* Find last namespace separator */` |
|      196 |  9322 | `		while( z > zFunc ){` |
|      196 |  9323 | `			if( z[-1] == '\\' ){` |
|       24 |  9324 | `				break;` |
|        - |  9325 | `			}` |
|      176 |  9326 | `			z--;` |
|        4 |  9327 | `		}` |
|       24 |  9328 | `		if( z > zFunc && z < zEnd ){` |
|        - |  9329 | `			/* Retry lookup using the unqualified/global function name */` |
|       24 |  9330 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       24 |  9331 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  9332 | `		}` |
|       10 |  9333 | `	}` |
|        - |  9334 | `	} /* end VmCallArgMap namespace scope */` |
|   727205 |  9335 | `	if( pEntry ){` |
|        - |  9336 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  9337 | `		ph7_class_instance *pThis;` |
|        - |  9338 | `		ph7_value *pFrameStack;` |
|        - |  9339 | `		ph7_vm_func *pVmFunc;` |
|        - |  9340 | `		ph7_class *pSelf;` |
|        - |  9341 | `		VmFrame *pFrame;` |
|        - |  9342 | `		ph7_value *pObj;` |
|        - |  9343 | `		VmSlot sArg;` |
|        - |  9344 | `		sxu32 n;` |
|        - |  9345 | `		/* initialize fields */` |
|    19497 |  9346 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    19497 |  9347 | `		pThis = 0;` |
|    19497 |  9348 | `		pSelf = 0;` |
|    19497 |  9349 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  9350 | `			ph7_class_method *pMeth;` |
|        - |  9351 | `			/* Class method call */` |
|     3769 |  9352 | `			ph7_value *pTarget = &pTos[-1];` |
|     3769 |  9353 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  9354 | `				/* Extract the 'this' pointer */` |
|     3769 |  9355 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  9356 | `					/* Instance already loaded */` |
|     3679 |  9357 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3679 |  9358 | `					pThis->iRef++;` |
|     3679 |  9359 | `					pSelf = pThis->pClass;` |
|     1837 |  9360 | `				}` |
|     3769 |  9361 | `				if( pSelf == 0 ){` |
|       95 |  9362 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  9363 | `						/* "Late Static Binding" class name */` |
|      131 |  9364 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  9365 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  9366 | `					}` |
|       95 |  9367 | `					if( pSelf == 0 ){` |
|       21 |  9368 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  9369 | `					}` |
|       45 |  9370 | `				}` |
|     3769 |  9371 | `				if( pThis == 0  ){` |
|       95 |  9372 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       95 |  9373 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       95 |  9374 | `					if( pFrameLocal->pParent ){` |
|        - |  9375 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       69 |  9376 | `						pThis = pFrameLocal->pThis;` |
|       69 |  9377 | `						if( pThis ){` |
|       21 |  9378 | `							pThis->iRef++;` |
|       10 |  9379 | `						}` |
|       32 |  9380 | `					}` |
|       45 |  9381 | `				}` |
|     3769 |  9382 | `				VmPopOperand(&pTos,1);` |
|     3769 |  9383 | `				PH7_MemObjRelease(pTos);` |
|        - |  9384 | `				/* Synchronize pointers */` |
|     3769 |  9385 | `				pArg = &pTos[-nCallArgs];` |
|        - |  9386 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  9387 | `				 * user have already computed the random generated unique class method name` |
|        - |  9388 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  9389 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  9390 | `				 */` |
|     3769 |  9391 | `				while( pArg < pStack ){` |
|      ! 0 |  9392 | `					pArg++;` |
|      ! 0 |  9393 | `				}` |
|     3769 |  9394 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  9395 | `					/* Check if the call is allowed */` |
|     3769 |  9396 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3769 |  9397 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  9398 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  9399 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  9400 | `							char zMsg[256];` |
|      ! 0 |  9401 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  9402 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  9403 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  9404 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  9405 | `							/* Pop given arguments */` |
|      ! 0 |  9406 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  9407 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9408 | `							}` |
|      ! 0 |  9409 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  9410 | `							goto Abort;` |
|        - |  9411 | `						}` |
|        6 |  9412 | `					}` |
|     1882 |  9413 | `				}` |
|     1882 |  9414 | `			}` |
|     1882 |  9415 | `		}` |
|        - |  9416 | `		/* Check The recursion limit. Hitting it raises a clean, non-catchable` |
|        - |  9417 | `		 * fatal (was: silently set NULL and continue) and halts. The check is` |
|        - |  9418 | `		 * before VmEnterFrame/the recursive VmByteCodeExec below, so a` |
|        - |  9419 | `		 * correctly-set cap also keeps deep recursion off the native stack. */` |
|    19497 |  9420 | `		if( VmRecursionExceeded(pVm) ){` |
|        - |  9421 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - |  9422 | `			 * which walks the whole operand stack — don't release them here. */` |
|        6 |  9423 | `			VmRecursionFatal(&(*pVm));` |
|        6 |  9424 | `			goto Abort;` |
|        - |  9425 | `		}` |
|    19493 |  9426 | `		if( pVmFunc->pNextName ){` |
|        - |  9427 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  9428 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  9429 | `		}` |
|    19493 |  9430 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  9431 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  9432 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  9433 | `			ph7_generator *pGenerator;` |
|        - |  9434 | `			ph7_class_instance *pGenObj;` |
|        - |  9435 | `			ph7_value *pCtxAttr;` |
|        - |  9436 | `			SyString sAttrName;` |
|        - |  9437 | `			ph7_value **apCallArgs;` |
|        - |  9438 | `			int nGenArgs, iArg;` |
|        - |  9439 | `			/* Collect arguments from the operand stack */` |
|       53 |  9440 | `			nGenArgs = (int)(pTos - pArg);` |
|       53 |  9441 | `			apCallArgs = 0;` |
|       53 |  9442 | `			if( nGenArgs > 0 ){` |
|       14 |  9443 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9444 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  9445 | `				if( apCallArgs == 0 ){` |
|        - |  9446 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  9447 | `					nGenArgs = 0;` |
|      ! 0 |  9448 | `				}else{` |
|       10 |  9449 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9450 | `					int didReorder = 0;` |
|       10 |  9451 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9452 | `						/* Named-argument reordering for generator */` |
|        5 |  9453 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9454 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9455 | `						sxu32 nNV = nF;` |
|        5 |  9456 | `						sxi32 iVIdx = -1;` |
|        - |  9457 | `						sxi32 *aGSlot;` |
|        - |  9458 | `						sxu8 *aGUsed;` |
|        - |  9459 | `						sxu32 gi;` |
|       13 |  9460 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9461 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9462 | `						}` |
|        7 |  9463 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9464 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9465 | `						if( aGSlot ){` |
|        5 |  9466 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9467 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9468 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9469 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9470 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9471 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9472 | `								goto Abort;` |
|        - |  9473 | `							}` |
|        - |  9474 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9475 | `							 * append overflow (variadic / positional beyond` |
|        - |  9476 | `							 * formals) so downstream sees every argument. */` |
|        - |  9477 | `							{` |
|        5 |  9478 | `								int nOut = 0;` |
|       13 |  9479 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9480 | `									sxu32 gj;` |
|       13 |  9481 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9482 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9483 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9484 | `											break;` |
|        - |  9485 | `										}` |
|        3 |  9486 | `									}` |
|        5 |  9487 | `								}` |
|       13 |  9488 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9489 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9490 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9491 | `									}` |
|        5 |  9492 | `								}` |
|        5 |  9493 | `								nGenArgs = nOut;` |
|        - |  9494 | `							}` |
|        5 |  9495 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9496 | `							didReorder = 1;` |
|        2 |  9497 | `						}` |
|        - |  9498 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9499 | `						 * positional fill below — preserves arg order rather` |
|        - |  9500 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9501 | `					}` |
|       10 |  9502 | `					if( !didReorder ){` |
|       12 |  9503 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9504 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9505 | `						}` |
|        2 |  9506 | `					}` |
|        - |  9507 | `				}` |
|        4 |  9508 | `			}` |
|        - |  9509 | `			/* Create execution context and generator wrapper */` |
|       53 |  9510 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       53 |  9511 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9512 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9513 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9514 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9515 | `				break;` |
|        - |  9516 | `			}` |
|       53 |  9517 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       53 |  9518 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9519 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9520 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9521 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9522 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9523 | `				break;` |
|        - |  9524 | `			}` |
|        - |  9525 | `			/* Set up the frame with arguments, closure env, $this */` |
|       53 |  9526 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       53 |  9527 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       53 |  9528 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       53 |  9529 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       53 |  9530 | `			pExecCtx->pFrame->pParent = 0;` |
|       53 |  9531 | `			if( apCallArgs ){` |
|       10 |  9532 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9533 | `			}` |
|       53 |  9534 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9535 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9536 | `				if( pThis ){` |
|      ! 0 |  9537 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9538 | `				}` |
|      ! 0 |  9539 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9540 | `					goto Abort;` |
|        - |  9541 | `				}` |
|      ! 0 |  9542 | `				break;` |
|        - |  9543 | `			}` |
|        - |  9544 | `			/* Create Generator class instance */` |
|       53 |  9545 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       53 |  9546 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9547 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9548 | `				break;` |
|        - |  9549 | `			}` |
|        - |  9550 | `			/* Store generator in __ctx attribute */` |
|       53 |  9551 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       53 |  9552 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       53 |  9553 | `			if( pCtxAttr ){` |
|       53 |  9554 | `				pCtxAttr->x.pOther = pGenerator;` |
|       53 |  9555 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       24 |  9556 | `			}` |
|        - |  9557 | `			/* Pop args and function name, push Generator object */` |
|       53 |  9558 | `			PH7_MemObjRelease(pTos);` |
|       53 |  9559 | `			pTos = &pTos[-nCallArgs];` |
|       53 |  9560 | `			pTos->x.pOther = pGenObj;` |
|       53 |  9561 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       53 |  9562 | `			pGenObj->iRef++;` |
|       53 |  9563 | `			if( pThis ){` |
|      ! 0 |  9564 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9565 | `			}` |
|       53 |  9566 | `			break;` |
|        - |  9567 | `		}` |
|        - |  9568 | `		/* Extract the formal argument set */` |
|    19445 |  9569 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9570 | `		/* Create a new VM frame  */` |
|    19445 |  9571 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    19445 |  9572 | `		if( rc != SXRET_OK ){` |
|        - |  9573 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9574 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9575 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9576 | `				&pVmFunc->sName);` |
|        - |  9577 | `			/* Pop given arguments */` |
|      ! 0 |  9578 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9579 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9580 | `			}` |
|        - |  9581 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9582 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9583 | `			break;` |
|        - |  9584 | `		}` |
|    19445 |  9585 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9586 | `			/* Install the '$this' variable */` |
|        - |  9587 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3697 |  9588 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3697 |  9589 | `			if( pObj ){` |
|        - |  9590 | `				/* Reflect the change */` |
|     3697 |  9591 | `				pObj->x.pOther = pThis;` |
|     3697 |  9592 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1846 |  9593 | `			}` |
|     1846 |  9594 | `		}` |
|    19445 |  9595 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9596 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9597 | `			/* Install static variables */` |
|       13 |  9598 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       25 |  9599 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       13 |  9600 | `				pStatic = &aStatic[n];` |
|       13 |  9601 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9602 | `					/* Initialize the static variables */` |
|        9 |  9603 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        9 |  9604 | `					if( pObj ){` |
|        - |  9605 | `						/* Assume a NULL initialization value */` |
|        9 |  9606 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        9 |  9607 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9608 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        9 |  9609 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|        4 |  9610 | `						}` |
|        9 |  9611 | `						pObj->nIdx = pStatic->nIdx;` |
|        5 |  9612 | `					}else{` |
|      ! 0 |  9613 | `						continue;` |
|        - |  9614 | `					}` |
|        4 |  9615 | `				}` |
|        - |  9616 | `				/* Install in the current frame */` |
|       19 |  9617 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|       12 |  9618 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        7 |  9619 | `			}` |
|        6 |  9620 | `		}` |
|        - |  9621 | `		/* Push arguments in the local frame */` |
|        - |  9622 | `		{` |
|    19445 |  9623 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9624 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9625 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    19445 |  9626 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    19445 |  9627 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9628 | `			/* ============================================================` |
|        - |  9629 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9630 | `			 *` |
|        - |  9631 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9632 | `			 * or position, then install them in the frame.` |
|        - |  9633 | `			 * ============================================================ */` |
|       97 |  9634 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       97 |  9635 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       97 |  9636 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9637 | `			sxu32 nNonVariadic;` |
|        - |  9638 | `			sxi32 *aSlot;` |
|        - |  9639 | `			sxu8  *aUsed;` |
|        - |  9640 | `			sxu32 i;` |
|        - |  9641 | `			/* Find variadic parameter index */` |
|      293 |  9642 | `			for( i = 0; i < nFormal; i++ ){` |
|      207 |  9643 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9644 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9645 | `					break;` |
|        - |  9646 | `				}` |
|      101 |  9647 | `			}` |
|       97 |  9648 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9649 | `			/* Allocate mapping arrays */` |
|      144 |  9650 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9651 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       97 |  9652 | `			if( aSlot == 0 ){` |
|      ! 0 |  9653 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9654 | `				goto Abort;` |
|        - |  9655 | `			}` |
|       97 |  9656 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9657 | `			/* Resolve named arguments to formal parameters */` |
|      144 |  9658 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9659 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       97 |  9660 | `			if( rc == PH7_ABORT ){` |
|        8 |  9661 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 |  9662 | `				goto Abort;` |
|        - |  9663 | `			}` |
|        - |  9664 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9665 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9666 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9667 | `				sxi32 iSrc = -1;` |
|      309 |  9668 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9669 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9670 | `						iSrc = (sxi32)i;` |
|      169 |  9671 | `						break;` |
|        - |  9672 | `					}` |
|       62 |  9673 | `				}` |
|      187 |  9674 | `				if( iSrc >= 0 ){` |
|        - |  9675 | `					/* Argument was provided — install with type checking */` |
|      169 |  9676 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9677 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9678 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9679 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9680 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal,FALSE);` |
|      ! 0 |  9681 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9682 | `					}` |
|        - |  9683 | `					/* Type checking: union types */` |
|      169 |  9684 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9685 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9686 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9687 | `							bCallIsStrict);` |
|       13 |  9688 | `						if( rcU != SXRET_OK ){` |
|        - |  9689 | `							const char *zGiven;` |
|      ! 0 |  9690 | `							const char *zExpected = "union";` |
|        - |  9691 | `							char zBuf[128];` |
|        - |  9692 | `							char zTypeBuf[128];` |
|      ! 0 |  9693 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9694 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9695 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9696 | `								zGiven = "null";` |
|      ! 0 |  9697 | `							}else{` |
|      ! 0 |  9698 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9699 | `							}` |
|      ! 0 |  9700 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9701 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9702 | `							}` |
|      ! 0 |  9703 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9704 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9705 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9706 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9707 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9708 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9709 | `							pFrameStack = 0;` |
|      ! 0 |  9710 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9711 | `							goto SkipFuncBody;` |
|        - |  9712 | `						}` |
|      171 |  9713 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9714 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9715 | `						/* Scalar/class type checking */` |
|       17 |  9716 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9717 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9718 | `							ph7_class *pClass;` |
|      ! 0 |  9719 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      ! 0 |  9720 | `							if( rcPseudo == 0 ){` |
|        - |  9721 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9722 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 |  9723 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9724 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9725 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  9726 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 |  9727 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9728 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9729 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9730 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9731 | `								pFrameStack = 0;` |
|      ! 0 |  9732 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9733 | `								goto SkipFuncBody;` |
|        - |  9734 | `							}` |
|        - |  9735 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class */` |
|      ! 0 |  9736 | `							pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9737 | `							if( pClass ){` |
|      ! 0 |  9738 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9739 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9740 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9741 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9742 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9743 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9744 | `									}` |
|      ! 0 |  9745 | `								}else{` |
|      ! 0 |  9746 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9747 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9748 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9749 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9750 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9751 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9752 | `									}` |
|        - |  9753 | `								}` |
|      ! 0 |  9754 | `							}` |
|       17 |  9755 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9756 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9757 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9758 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9759 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9760 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9761 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9762 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9763 | `								pFrameStack = 0;` |
|      ! 0 |  9764 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9765 | `								goto SkipFuncBody;` |
|        7 |  9766 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9767 | `								char zTypeBuf[128];` |
|      ! 0 |  9768 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9769 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9770 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9771 | `									ph7_type_name(pVal));` |
|      ! 0 |  9772 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9773 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9774 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9775 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9776 | `								pFrameStack = 0;` |
|      ! 0 |  9777 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9778 | `								goto SkipFuncBody;` |
|        - |  9779 | `							}` |
|        3 |  9780 | `						}` |
|        8 |  9781 | `					}` |
|        - |  9782 | `					/* Install: by reference or by value */` |
|      169 |  9783 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9784 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9785 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9786 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9787 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9788 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9789 | `							}` |
|      ! 0 |  9790 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9791 | `						}else{` |
|        7 |  9792 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9793 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9794 | `							if( pRefEntry == 0 ){` |
|        7 |  9795 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9796 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9797 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9798 | `								sArg.pUserData = 0;` |
|        5 |  9799 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9800 | `							}` |
|        5 |  9801 | `							pObj = 0;` |
|        - |  9802 | `						}` |
|        3 |  9803 | `					}else{` |
|      165 |  9804 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9805 | `					}` |
|      169 |  9806 | `					if( pObj ){` |
|      165 |  9807 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9808 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9809 | `						sArg.pUserData = 0;` |
|      165 |  9810 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9811 | `					}` |
|       85 |  9812 | `				}else{` |
|        - |  9813 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9814 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9815 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9816 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9817 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9818 | `						if( pObj ){` |
|       19 |  9819 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|       19 |  9820 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9821 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9822 | `							sArg.pUserData = 0;` |
|       19 |  9823 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9824 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9825 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9826 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9827 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9828 | `							}` |
|        9 |  9829 | `						}` |
|        9 |  9830 | `					}` |
|        - |  9831 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9832 | `				}` |
|       94 |  9833 | `			}` |
|        - |  9834 | `			/* Handle variadic parameter */` |
|       89 |  9835 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9836 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9837 | `				if( pObj ){` |
|        9 |  9838 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9839 | `					{` |
|        9 |  9840 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9841 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9842 | `							if( aSlot[i] == -1 ){` |
|       16 |  9843 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9844 | `									/* Named variadic entry: insert with string key */` |
|        - |  9845 | `									ph7_value sKey;` |
|       11 |  9846 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9847 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9848 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9849 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9850 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9851 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9852 | `								}else{` |
|        - |  9853 | `									/* Positional variadic entry */` |
|      ! 0 |  9854 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9855 | `								}` |
|        5 |  9856 | `							}` |
|       12 |  9857 | `						}` |
|        - |  9858 | `					}` |
|        9 |  9859 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9860 | `					sArg.pUserData = 0;` |
|        9 |  9861 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9862 | `				}` |
|        5 |  9863 | `			}else{` |
|        - |  9864 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9865 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9866 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9867 | `				 * the positional-only path's behavior. */` |
|       81 |  9868 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9869 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9870 | `					if( aSlot[i] == -2 ){` |
|        - |  9871 | `						char zAnonBuf[32];` |
|        - |  9872 | `						SyString sAnonName;` |
|      ! 0 |  9873 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9874 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9875 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9876 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9877 | `						if( pObj ){` |
|      ! 0 |  9878 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9879 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9880 | `							sArg.pUserData = 0;` |
|      ! 0 |  9881 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9882 | `						}` |
|      ! 0 |  9883 | `						nAnon++;` |
|      ! 0 |  9884 | `					}` |
|       79 |  9885 | `				}` |
|        - |  9886 | `			}` |
|        - |  9887 | `			/* Release all stack arguments */` |
|      267 |  9888 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9889 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9890 | `			}` |
|       89 |  9891 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9892 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9893 | `			n = nFormal;` |
|       45 |  9894 | `		}else{` |
|        - |  9895 | `		/* ============================================================` |
|        - |  9896 | `		 * Positional-only matching path (original)` |
|        - |  9897 | `		 * ============================================================ */` |
|    19351 |  9898 | `		n = 0;` |
|    50675 |  9899 | `		while( pArg < pTos ){` |
|    31409 |  9900 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9901 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       45 |  9902 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       45 |  9903 | `				if( pObj ){` |
|        - |  9904 | `					/* Initialize as empty array */` |
|       45 |  9905 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9906 | `					{` |
|       45 |  9907 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      161 |  9908 | `						while( pArg < pTos ){` |
|        - |  9909 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9910 | `							 *` |
|        - |  9911 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9912 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9913 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9914 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9915 | `							 * fixing both wants a separate counter for elements` |
|        - |  9916 | `							 * already packed into the variadic array. */` |
|      123 |  9917 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9918 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9919 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9920 | `									bCallIsStrict);` |
|       16 |  9921 | `								if( rcU != SXRET_OK ){` |
|        - |  9922 | `									const char *zGiven;` |
|        3 |  9923 | `									const char *zExpected = "union";` |
|        - |  9924 | `									char zBuf[128];` |
|        - |  9925 | `									char zTypeBuf[128];` |
|        3 |  9926 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9927 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9928 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9929 | `										zGiven = "null";` |
|      ! 0 |  9930 | `									}else{` |
|        3 |  9931 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9932 | `									}` |
|        3 |  9933 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9934 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9935 | `									}` |
|        4 |  9936 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9937 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9938 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9939 | `										goto Abort;` |
|        - |  9940 | `									}` |
|        3 |  9941 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9942 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9943 | `									pFrameStack = 0;` |
|        3 |  9944 | `									rc = PH7_EXCEPTION;` |
|        3 |  9945 | `									goto SkipFuncBody;` |
|        - |  9946 | `								}` |
|       14 |  9947 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9948 | `								pArg++;` |
|       14 |  9949 | `								continue;` |
|        - |  9950 | `							}` |
|        - |  9951 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9952 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      120 |  9953 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9954 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       44 |  9955 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9956 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9957 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9958 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9959 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9960 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9961 | `										goto Abort;` |
|        - |  9962 | `									}` |
|        - |  9963 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9964 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9965 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9966 | `									pFrameStack = 0;` |
|      ! 0 |  9967 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9968 | `									goto SkipFuncBody;` |
|       13 |  9969 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9970 | `									char zTypeBuf[128];` |
|      ! 0 |  9971 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9972 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9973 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9974 | `										ph7_type_name(pArg));` |
|      ! 0 |  9975 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9976 | `										goto Abort;` |
|        - |  9977 | `									}` |
|      ! 0 |  9978 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9979 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9980 | `									pFrameStack = 0;` |
|      ! 0 |  9981 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9982 | `									goto SkipFuncBody;` |
|        - |  9983 | `								}` |
|        6 |  9984 | `							}` |
|      109 |  9985 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      109 |  9986 | `							pArg++;` |
|        5 |  9987 | `						}` |
|        - |  9988 | `					}` |
|       43 |  9989 | `					sArg.nIdx = pObj->nIdx;` |
|       43 |  9990 | `					sArg.pUserData = 0;` |
|       43 |  9991 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       19 |  9992 | `				}` |
|       43 |  9993 | `				break; /* All remaining args consumed */` |
|        - |  9994 | `			}` |
|    31369 |  9995 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    31148 |  9996 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       44 |  9997 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9998 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9999 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg,FALSE);` |
|      ! 0 | 10000 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 10001 | `						goto Abort;` |
|        - | 10002 | `					}` |
|      ! 0 | 10003 | `				}` |
|        - | 10004 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    31153 | 10005 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       98 | 10006 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       62 | 10007 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       31 | 10008 | `						bCallIsStrict);` |
|       67 | 10009 | `					if( rcU != SXRET_OK ){` |
|        - | 10010 | `						const char *zGiven;` |
|       22 | 10011 | `						const char *zExpected = "union";` |
|        - | 10012 | `						char zBuf[128];` |
|        - | 10013 | `						char zTypeBuf[128];` |
|       22 | 10014 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        8 | 10015 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       18 | 10016 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|       10 | 10017 | `							zGiven = "null";` |
|        6 | 10018 | `						}else{` |
|        6 | 10019 | `							zGiven = ph7_type_name(pArg);` |
|        - | 10020 | `						}` |
|       22 | 10021 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       22 | 10022 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 | 10023 | `						}` |
|       31 | 10024 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 | 10025 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       22 | 10026 | `						if( rc == PH7_ABORT ){` |
|      ! 0 | 10027 | `							goto Abort;` |
|        - | 10028 | `						}` |
|       22 | 10029 | `						PH7_MemObjRelease(pTos);` |
|       22 | 10030 | `						pTos = &pTos[-nCallArgs];` |
|       22 | 10031 | `						pFrameStack = 0;` |
|       22 | 10032 | `						rc = PH7_EXCEPTION;` |
|       22 | 10033 | `						goto SkipFuncBody;` |
|        - | 10034 | `					}` |
|       23 | 10035 | `				}else` |
|        - | 10036 | `				/* Make sure the given arguments are of the correct type.` |
|        - | 10037 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    31116 | 10038 | `				if( aFormalArg[n].nType > 0` |
|    16274 | 10039 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1427 | 10040 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - | 10041 | `						/* Argument must be a class instance [i.e: object] */` |
|       37 | 10042 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - | 10043 | `						ph7_class *pClass;` |
|       37 | 10044 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|       37 | 10045 | `						if( rcPseudo == 0 ){` |
|        - | 10046 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - | 10047 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 | 10048 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 | 10049 | `								&aFormalArg[n].sName,` |
|        2 | 10050 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 | 10051 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 | 10052 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 | 10053 | `							PH7_MemObjRelease(pTos);` |
|        5 | 10054 | `							pTos = &pTos[-nCallArgs];` |
|        5 | 10055 | `							pFrameStack = 0;` |
|        5 | 10056 | `							rc = PH7_EXCEPTION;` |
|        5 | 10057 | `							goto SkipFuncBody;` |
|        - | 10058 | `						}` |
|        - | 10059 | `						/* Try to extract the desired class (rcPseudo==1 accepts; -1 real class) */` |
|       33 | 10060 | `						pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       33 | 10061 | `						if( pClass ){` |
|       23 | 10062 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10063 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 | 10064 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 10065 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 | 10066 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 | 10067 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 | 10068 | `								}` |
|      ! 0 | 10069 | `							}else{` |
|        - | 10070 | `								/* reuse pThis declared in outer scope */` |
|       23 | 10071 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - | 10072 | `								/* Make sure the object is an instance of the given class */` |
|       23 | 10073 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 | 10074 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 10075 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 | 10076 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 | 10077 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 | 10078 | `								}` |
|        - | 10079 | `							}` |
|       13 | 10080 | `						}` |
|     1408 | 10081 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       30 | 10082 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - | 10083 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 | 10084 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 | 10085 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 | 10086 | `							if( rc == PH7_ABORT ){` |
|      ! 0 | 10087 | `								goto Abort;` |
|        - | 10088 | `							}` |
|        - | 10089 | `							/* Skip function body, route through normal cleanup */` |
|       11 | 10090 | `							PH7_MemObjRelease(pTos);` |
|       11 | 10091 | `							pTos = &pTos[-nCallArgs];` |
|       11 | 10092 | `							pFrameStack = 0;` |
|       11 | 10093 | `							rc = PH7_EXCEPTION;` |
|       11 | 10094 | `							goto SkipFuncBody;` |
|       19 | 10095 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 10096 | `							char zTypeBuf[128];` |
|       15 | 10097 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        8 | 10098 | `								&aFormalArg[n].sName,` |
|        8 | 10099 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        4 | 10100 | `								ph7_type_name(pArg));` |
|       11 | 10101 | `							if( rc == PH7_ABORT ){` |
|        6 | 10102 | `								goto Abort;` |
|        - | 10103 | `							}` |
|        5 | 10104 | `							PH7_MemObjRelease(pTos);` |
|        5 | 10105 | `							pTos = &pTos[-nCallArgs];` |
|        5 | 10106 | `							pFrameStack = 0;` |
|        5 | 10107 | `							rc = PH7_EXCEPTION;` |
|        5 | 10108 | `							goto SkipFuncBody;` |
|        - | 10109 | `						}` |
|        4 | 10110 | `					}` |
|      700 | 10111 | `				}` |
|    31113 | 10112 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - | 10113 | `					/* Pass by reference */` |
|       58 | 10114 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - | 10115 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 | 10116 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 | 10117 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 10118 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 | 10119 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 | 10120 | `						}` |
|        - | 10121 | `						/* Switch to pass by value */` |
|      ! 0 | 10122 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 | 10123 | `					}else{` |
|        - | 10124 | `						SyHashEntry *pRefEntry;` |
|        - | 10125 | `						/* Install the referenced variable in the private function frame */` |
|       58 | 10126 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 | 10127 | `						if( pRefEntry == 0 ){` |
|       86 | 10128 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 | 10129 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 | 10130 | `							sArg.nIdx = pArg->nIdx;` |
|       58 | 10131 | `							sArg.pUserData = 0;` |
|       58 | 10132 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 | 10133 | `						}` |
|       58 | 10134 | `						pObj = 0;` |
|        - | 10135 | `					}` |
|       30 | 10136 | `				}else{` |
|        - | 10137 | `					/* Pass by value,make a copy of the given argument */` |
|    31057 | 10138 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - | 10139 | `				}` |
|    15559 | 10140 | `			}else{` |
|        - | 10141 | `				char zName[32];` |
|        - | 10142 | `				SyString sArgName;` |
|        - | 10143 | `				/* Set a dummy name */` |
|      220 | 10144 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      220 | 10145 | `				sArgName.zString = zName;` |
|        - | 10146 | `				/* Annonymous argument */` |
|      220 | 10147 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 10148 | `			}` |
|    31329 | 10149 | `			if( pObj ){` |
|    31273 | 10150 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 10151 | `				/* Insert argument index  */` |
|    31273 | 10152 | `				sArg.nIdx = pObj->nIdx;` |
|    31273 | 10153 | `				sArg.pUserData = 0;` |
|    31273 | 10154 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15634 | 10155 | `			}` |
|    31329 | 10156 | `			PH7_MemObjRelease(pArg);` |
|    31329 | 10157 | `			pArg++;` |
|    31329 | 10158 | `			++n;` |
|        5 | 10159 | `		}` |
|        - | 10160 | `		} /* end named vs positional branch */` |
|        - | 10161 | `		/* Set up closure environment */` |
|    19397 | 10162 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10163 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 10164 | `			ph7_value *pValue;` |
|        - | 10165 | `			sxu32 iEnv;` |
|      184 | 10166 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 | 10167 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 | 10168 | `				pEnv = &aEnv[iEnv];` |
|      252 | 10169 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 10170 | `					/* Do not install null value */` |
|      178 | 10171 | `					continue;` |
|        - | 10172 | `				}` |
|       76 | 10173 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 | 10174 | `				if( pValue == 0 ){` |
|      ! 0 | 10175 | `					continue;` |
|        - | 10176 | `				}` |
|        - | 10177 | `				/* Invalidate any prior representation */` |
|       76 | 10178 | `				PH7_MemObjRelease(pValue);` |
|        - | 10179 | `				/* Duplicate bound variable value */` |
|       76 | 10180 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 | 10181 | `			}` |
|       91 | 10182 | `		}` |
|        - | 10183 | `		/* Process default values for remaining formal parameters */` |
|    22437 | 10184 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     3093 | 10185 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 10186 | `				/* Variadic parameter with no extra args — create empty array */` |
|       53 | 10187 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       53 | 10188 | `				if( pObj ){` |
|       53 | 10189 | `					PH7_MemObjToHashmap(pObj);` |
|       53 | 10190 | `					sArg.nIdx = pObj->nIdx;` |
|       53 | 10191 | `					sArg.pUserData = 0;` |
|       53 | 10192 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 | 10193 | `				}` |
|       53 | 10194 | `				n++;` |
|       53 | 10195 | `				break; /* Variadic is always last */` |
|        - | 10196 | `			}` |
|     3045 | 10197 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     3039 | 10198 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     3039 | 10199 | `				if( pObj ){` |
|        - | 10200 | `					/* Evaluate the default value and extract it's result */` |
|     3039 | 10201 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|     3039 | 10202 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 10203 | `						goto Abort;` |
|        - | 10204 | `					}` |
|        - | 10205 | `					/* Insert argument index */` |
|     3039 | 10206 | `					sArg.nIdx = pObj->nIdx;` |
|     3039 | 10207 | `					sArg.pUserData = 0;` |
|     3039 | 10208 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 10209 | `					/* Make sure the default argument is of the correct type */` |
|     3034 | 10210 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1943 | 10211 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 | 10212 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 10213 | `						/* Cast to the desired type */` |
|        3 | 10214 | `						xCast(pObj);` |
|        1 | 10215 | `					}` |
|     1517 | 10216 | `				}` |
|     1517 | 10217 | `			}` |
|     3045 | 10218 | `			++n;` |
|        5 | 10219 | `		}` |
|        - | 10220 | `		} /* end VmCallArgMap scope */` |
|        - | 10221 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 10222 | `		 * does not return anything.` |
|        - | 10223 | `		 */` |
|    19397 | 10224 | `		PH7_MemObjRelease(pTos);` |
|    19397 | 10225 | `		pTos = &pTos[-nCallArgs];` |
|        - | 10226 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    19397 | 10227 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    19397 | 10228 | `		if( pFrameStack == 0 ){` |
|        - | 10229 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 10230 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 10231 | `				&pVmFunc->sName);` |
|      ! 0 | 10232 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 10233 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 10234 | `			}` |
|      ! 0 | 10235 | `			break;` |
|        - | 10236 | `		}` |
|     9696 | 10237 | `SkipFuncBody:` |
|    19435 | 10238 | `		if( pSelf ){` |
|        - | 10239 | `			/* Push class name */` |
|     3767 | 10240 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1881 | 10241 | `		}` |
|        - | 10242 | `		/* Increment nesting level */` |
|    19435 | 10243 | `		pVm->nRecursionDepth++;` |
|    19435 | 10244 | `		if( rc != PH7_EXCEPTION ){` |
|        - | 10245 | `			/* Execute function body */` |
|    29093 | 10246 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    19392 | 10247 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0, FALSE);` |
|     9696 | 10248 | `		}` |
|        - | 10249 | `		/* Decrement nesting level */` |
|    19435 | 10250 | `		pVm->nRecursionDepth--;` |
|    19435 | 10251 | `		if( pSelf ){` |
|        - | 10252 | `			/* Pop class name */` |
|     3767 | 10253 | `			(void)SySetPop(&pVm->aSelf);` |
|     1881 | 10254 | `		}` |
|        - | 10255 | `		/* Cleanup the mess left behind */` |
|    19435 | 10256 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - | 10257 | `			/* Return by reference,reflect that */` |
|        9 | 10258 | `			if( n != SXU32_HIGH ){` |
|        9 | 10259 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - | 10260 | `				sxu32 i;` |
|        - | 10261 | `				/* Make sure the referenced object is not a local variable */` |
|       13 | 10262 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 | 10263 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 | 10264 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 | 10265 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10266 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10267 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 | 10268 | `								&pVmFunc->sName);` |
|      ! 0 | 10269 | `						}` |
|      ! 0 | 10270 | `						n = SXU32_HIGH;` |
|      ! 0 | 10271 | `						break;` |
|        - | 10272 | `					}` |
|        3 | 10273 | `				}` |
|        5 | 10274 | `			}else{` |
|      ! 0 | 10275 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10276 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10277 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 | 10278 | `						&pVmFunc->sName);` |
|      ! 0 | 10279 | `				}` |
|        - | 10280 | `			}` |
|        9 | 10281 | `			pTos->nIdx = n;` |
|        4 | 10282 | `		}` |
|        - | 10283 | `		/* Cleanup the mess left behind */` |
|    19435 | 10284 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - | 10285 | `			/* An exception was throw in this frame */` |
|      121 | 10286 | `			pFrame = pFrame->pParent;` |
|      121 | 10287 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - | 10288 | `				/* Pop the resutlt */` |
|       77 | 10289 | `				VmPopOperand(&pTos,1);` |
|        - | 10290 | `				/* Jump to this destination */` |
|       77 | 10291 | `				pc = pFrame->iExceptionJump - 1;` |
|       77 | 10292 | `				rc = PH7_OK;` |
|       41 | 10293 | `			}else{` |
|       45 | 10294 | `				if( pFrame->pParent ){` |
|       43 | 10295 | `					rc = PH7_EXCEPTION;` |
|       22 | 10296 | `				}else{` |
|        - | 10297 | `					/* Continue normal execution */` |
|        3 | 10298 | `					rc = PH7_OK;` |
|        - | 10299 | `				}` |
|        - | 10300 | `			}` |
|       58 | 10301 | `		}` |
|        - | 10302 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    19435 | 10303 | `		if( pFrameStack ){` |
|    19397 | 10304 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9696 | 10305 | `		}` |
|        - | 10306 | `		/* Leave the frame */` |
|    19435 | 10307 | `		VmLeaveFrame(&(*pVm));` |
|    19435 | 10308 | `		if( rc == PH7_ABORT ){` |
|        - | 10309 | `			/* Abort processing immeditaley */` |
|      120 | 10310 | `			goto Abort;` |
|    19319 | 10311 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10312 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - | 10313 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - | 10314 | `			 * overwriting the state saved by the inner level.` |
|        - | 10315 | `			 * pTos points to the result slot (not yet written).` |
|        - | 10316 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       43 | 10317 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       43 | 10318 | `			goto Suspend;` |
|    19281 | 10319 | `		}else if( rc == PH7_EXCEPTION ){` |
|       43 | 10320 | `			goto Exception;` |
|        - | 10321 | `		}` |
|     9622 | 10322 | `	}else{` |
|        - | 10323 | `		ph7_user_func *pFunc;` |
|        - | 10324 | `		ph7_context sCtx;` |
|        - | 10325 | `		ph7_value sRet;` |
|        - | 10326 | `		/* Look for an installed foreign function.` |
|        - | 10327 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 10328 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 10329 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 10330 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   707713 | 10331 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 10332 | `		{` |
|   707713 | 10333 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   707713 | 10334 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 10335 | `			/* Compiler-qualified: try short name as global fallback */` |
|       24 | 10336 | `			const char *zShort = sName.zString;` |
|        - | 10337 | `			sxu32 i;` |
|      336 | 10338 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      316 | 10339 | `				if( sName.zString[i] == '\\' ){` |
|       30 | 10340 | `					zShort = &sName.zString[i + 1];` |
|       13 | 10341 | `				}` |
|      160 | 10342 | `			}` |
|       24 | 10343 | `			if( zShort != sName.zString ){` |
|       24 | 10344 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       24 | 10345 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 | 10346 | `			}` |
|       10 | 10347 | `		}` |
|        - | 10348 | `		} /* end VmCallArgMap namespace scope */` |
|   707713 | 10349 | `		if( pEntry == 0 ){` |
|        - | 10350 | `			/* Call to undefined function */` |
|        6 | 10351 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - | 10352 | `			/* Pop given arguments */` |
|        6 | 10353 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 | 10354 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 | 10355 | `			}` |
|        - | 10356 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        6 | 10357 | `			PH7_MemObjRelease(pTos);` |
|       61 | 10358 | `			break;` |
|        - | 10359 | `		}` |
|   707709 | 10360 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 10361 | `		/* Start collecting function arguments */` |
|   707709 | 10362 | `		SySetReset(&aArg);` |
|  1908525 | 10363 | `		while( pArg < pTos ){` |
|  1200821 | 10364 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1200821 | 10365 | `			pArg++;` |
|        5 | 10366 | `		}` |
|        - | 10367 | `		/* Assume a null return value */` |
|   707709 | 10368 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 10369 | `		/* Init the call context */` |
|   707709 | 10370 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 10371 | `		/* Call the foreign function */` |
|   707709 | 10372 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 10373 | `		/* Release the call context */` |
|   707709 | 10374 | `		VmReleaseCallContext(&sCtx);` |
|   707709 | 10375 | `		if( rc == PH7_ABORT ){` |
|        - | 10376 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 10377 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 10378 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      536 | 10379 | `			PH7_MemObjRelease(&sRet);` |
|      536 | 10380 | `			goto Abort;` |
|   707177 | 10381 | `		}else if( rc == PH7_EXCEPTION ){` |
|      118 | 10382 | `			VmFrame *pFrm = pVm->pFrame;` |
|      118 | 10383 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      118 | 10384 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - | 10385 | `				/* Exception was NOT caught, propagate */` |
|        6 | 10386 | `				goto Exception;` |
|        - | 10387 | `			}` |
|        - | 10388 | `			/* Exception was caught: pop args and the result slot */` |
|      113 | 10389 | `			PH7_MemObjRelease(&sRet);` |
|      113 | 10390 | `			if( pInstr->iP1 > 0 ){` |
|       97 | 10391 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       47 | 10392 | `			}` |
|        - | 10393 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      113 | 10394 | `			VmPopOperand(&pTos,1);` |
|        - | 10395 | `			/* Jump past the try/catch block via the exception frame */` |
|      113 | 10396 | `			pFrm = pVm->pFrame;` |
|      113 | 10397 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      113 | 10398 | `				pc = pFrm->iExceptionJump - 1;` |
|       55 | 10399 | `			}` |
|      113 | 10400 | `			break;` |
|        - | 10401 | `		}` |
|   707063 | 10402 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10403 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 10404 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 10405 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 10406 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 10407 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 10408 | `			 * body), the user-function path above will handle re-saving. */` |
|       43 | 10409 | `			PH7_MemObjRelease(&sRet);` |
|       43 | 10410 | `			if( pInstr->iP1 > 0 ){` |
|       43 | 10411 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 | 10412 | `			}` |
|        - | 10413 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 10414 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       43 | 10415 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       43 | 10416 | `			goto Suspend;` |
|        - | 10417 | `		}` |
|   707025 | 10418 | `		if( pInstr->iP1 > 0 ){` |
|        - | 10419 | `			/* Pop function name and arguments */` |
|   684627 | 10420 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   342333 | 10421 | `		}` |
|        - | 10422 | `		/* Save foreign function return value */` |
|   707025 | 10423 | `		PH7_MemObjStore(&sRet,pTos);` |
|   707025 | 10424 | `		PH7_MemObjRelease(&sRet);` |
|        - | 10425 | `	}` |
|   726259 | 10426 | `	break;` |
|        - | 10427 | `				  }` |
|        - | 10428 | `/*` |
|        - | 10429 | ` * OP_CONSUME: P1 * *` |
|        - | 10430 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 10431 | ` */` |
|    16286 | 10432 | `case PH7_OP_CONSUME: {` |
|    32577 | 10433 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32577 | 10434 | `	ph7_value *pCur,*pOut = pTos;` |
|        - | 10435 |  |
|    32577 | 10436 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32577 | 10437 | `	pCur = pOut;` |
|        - | 10438 | `	/* Start the consume process  */` |
|    65191 | 10439 | `	while( pOut <= pTos ){` |
|        - | 10440 | `		/* Force a string cast */` |
|    32619 | 10441 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1107 | 10442 | `			PH7_MemObjToString(pOut);` |
|      551 | 10443 | `		}` |
|    32619 | 10444 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - | 10445 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - | 10446 | `			/* Invoke the output consumer callback */` |
|    20021 | 10447 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    20021 | 10448 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    20021 | 10449 | `			SyBlobRelease(&pOut->sBlob);` |
|    20021 | 10450 | `			if( rc == SXERR_ABORT ){` |
|        - | 10451 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 | 10452 | `				goto Abort;` |
|        - | 10453 | `			}` |
|    10008 | 10454 | `		}` |
|    32619 | 10455 | `		pOut++;` |
|        5 | 10456 | `	}` |
|    32577 | 10457 | `	pTos = &pCur[-1];` |
|    32572 | 10458 | `	break;` |
|        - | 10459 | `					 }` |
|        - | 10460 |  |
|        - | 10461 | `		} /* Switch() */` |
| 11917995 | 10462 | `		pc++; /* Next instruction in the stream */` |
|        5 | 10463 | `	} /* For(;;) */` |
|    24166 | 10464 | `Done:` |
|    48337 | 10465 | `	SySetRelease(&aArg);` |
|    48337 | 10466 | `	return SXRET_OK;` |
|      100 | 10467 | `Suspend:` |
|      205 | 10468 | `	SySetRelease(&aArg);` |
|      205 | 10469 | `	return PH7_SUSPEND;` |
|      350 | 10470 | `Abort:` |
|      704 | 10471 | `	SySetRelease(&aArg);` |
|     2194 | 10472 | `	while( pTos >= pStack ){` |
|     1494 | 10473 | `		PH7_MemObjRelease(pTos);` |
|     1494 | 10474 | `		pTos--;` |
|        4 | 10475 | `	}` |
|      704 | 10476 | `	return PH7_ABORT;` |
|       34 | 10477 | `Exception:` |
|       72 | 10478 | `	SySetRelease(&aArg);` |
|      128 | 10479 | `	while( pTos >= pStack ){` |
|       59 | 10480 | `		PH7_MemObjRelease(pTos);` |
|       59 | 10481 | `		pTos--;` |
|        3 | 10482 | `	}` |
|       72 | 10483 | `	return PH7_EXCEPTION;` |
|    24655 | 10484 |  |
|        - | 10485 | `/*` |
|        - | 10486 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10487 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10488 | ` * See block-comment on that function for additional information.` |
|        - | 10489 | ` */` |
|    23338 | 10490 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 10491 |  |
|        - | 10492 | `	ph7_value *pStack;` |
|        - | 10493 | `	sxi32 rc;` |
|        - | 10494 | `	/* Allocate a new operand stack */` |
|    23343 | 10495 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    23343 | 10496 | `	if( pStack == 0 ){` |
|      ! 0 | 10497 | `		return SXERR_MEM;` |
|        - | 10498 | `	}` |
|        - | 10499 | `	/* Execute the program */` |
|    23343 | 10500 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates);` |
|        - | 10501 | `	/* Free the operand stack */` |
|    23343 | 10502 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10503 | `	/* Execution result */` |
|    23343 | 10504 | `	return rc;` |
|    11674 | 10505 |  |
|        - | 10506 | `/*` |
|        - | 10507 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10508 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10509 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10510 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10511 | ` * execution ends.` |
|        - | 10512 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10513 | ` * additional information.` |
|        - | 10514 | ` */` |
|     2850 | 10515 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 10516 |  |
|        - | 10517 | `	VmShutdownCB *pEntry;` |
|        - | 10518 | `	ph7_value *apArg[10];` |
|        - | 10519 | `	sxu32 n,nEntry;` |
|        - | 10520 | `	int i;` |
|        - | 10521 | `	/* Point to the stack of registered callbacks */` |
|     2855 | 10522 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31355 | 10523 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28505 | 10524 | `		apArg[i] = 0;` |
|    14255 | 10525 | `	}` |
|        - | 10526 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10527 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10528 | `	 * callbacks, mirroring PHP.` |
|        - | 10529 | `	 */` |
|     2855 | 10530 | `	pVm->bHaltRequested = 0;` |
|     2867 | 10531 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       17 | 10532 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       17 | 10533 | `		if( pEntry ){` |
|        - | 10534 | `			/* Prepare callback arguments if any */` |
|       17 | 10535 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10536 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10537 | `					break;` |
|        - | 10538 | `				}` |
|      ! 0 | 10539 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10540 | `			}` |
|        - | 10541 | `			/* Invoke the callback */` |
|       17 | 10542 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10543 | `			/*` |
|        - | 10544 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10545 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10546 | `			 */` |
|       17 | 10547 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       17 | 10548 | `			if( pEntry ){` |
|       17 | 10549 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       17 | 10550 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10551 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10552 | `				}` |
|        6 | 10553 | `			}` |
|       17 | 10554 | `			if( pVm->bHaltRequested ){` |
|        - | 10555 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10556 | `				break;` |
|        - | 10557 | `			}` |
|        6 | 10558 | `		}` |
|       11 | 10559 | `	}` |
|     2855 | 10560 | `	SySetReset(&pVm->aShutdown);` |
|     2855 | 10561 |  |
|        - | 10562 | `/*` |
|        - | 10563 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10564 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10565 | ` * See block-comment on that function for additional information.` |
|        - | 10566 | ` */` |
|     2850 | 10567 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 10568 |  |
|        - | 10569 | `	/* Make sure we are ready to execute this program */` |
|     2855 | 10570 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10571 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10572 | `	}` |
|        - | 10573 | `	/* Set the execution magic number  */` |
|     2855 | 10574 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10575 | `	/* Execute the program */` |
|     2855 | 10576 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE);` |
|        - | 10577 | `	/* Invoke any shutdown callbacks */` |
|     2855 | 10578 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10579 | `	/*` |
|        - | 10580 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10581 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10582 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10583 | `	 */` |
|     2855 | 10584 | `	return SXRET_OK;` |
|     1430 | 10585 |  |
|        - | 10586 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10587 | `/*` |
|        - | 10588 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10589 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10590 | ` */` |
|       72 | 10591 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        5 | 10592 |  |
|        - | 10593 | `	ph7_exec_ctx *pCtx;` |
|        - | 10594 | `	ph7_value *pStack;` |
|        - | 10595 | `	VmFrame *pFrame;` |
|       77 | 10596 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       77 | 10597 | `	if( pCtx == 0 ){` |
|      ! 0 | 10598 | `		return 0;` |
|        - | 10599 | `	}` |
|       77 | 10600 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       77 | 10601 | `	pCtx->pVm = pVm;` |
|       77 | 10602 | `	pCtx->pFunc = pFunc;` |
|       77 | 10603 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       77 | 10604 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       77 | 10605 | `	pCtx->pc = 0;` |
|       77 | 10606 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       77 | 10607 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10608 | `	/* Allocate a private operand stack */` |
|       77 | 10609 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       77 | 10610 | `	if( pStack == 0 ){` |
|      ! 0 | 10611 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10612 | `		return 0;` |
|        - | 10613 | `	}` |
|       77 | 10614 | `	pCtx->pStack = pStack;` |
|        - | 10615 | `	/* Create a detached frame for the fiber */` |
|       77 | 10616 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       77 | 10617 | `	if( pFrame == 0 ){` |
|      ! 0 | 10618 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10619 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10620 | `		return 0;` |
|        - | 10621 | `	}` |
|       77 | 10622 | `	pCtx->pFrame = pFrame;` |
|       77 | 10623 | `	return pCtx;` |
|       41 | 10624 |  |
|        - | 10625 | `/*` |
|        - | 10626 | ` * Start executing a fiber context for the first time.` |
|        - | 10627 | ` */` |
|       68 | 10628 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        5 | 10629 |  |
|        - | 10630 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10631 | `	sxi32 rc;` |
|       73 | 10632 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10633 | `		return SXERR_INVALID;` |
|        - | 10634 | `	}` |
|        - | 10635 | `	/* Bound fiber/generator nesting under the same cap (each start adds a C` |
|        - | 10636 | `	 * frame); reject before mutating VM state so the abort is clean. */` |
|       73 | 10637 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10638 | `		return VmRecursionFatal(pVm);` |
|        - | 10639 | `	}` |
|        - | 10640 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       73 | 10641 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       73 | 10642 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10643 | `	/* Save and set the active context */` |
|       73 | 10644 | `	pOldCtx = pVm->pActiveCtx;` |
|       73 | 10645 | `	pVm->pActiveCtx = pCtx;` |
|       73 | 10646 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       73 | 10647 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       73 | 10648 | `	pVm->nRecursionDepth++;` |
|        - | 10649 | `	/* Execute from the beginning */` |
|       73 | 10650 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       34 | 10651 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       68 | 10652 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0, FALSE);` |
|       73 | 10653 | `	pVm->nRecursionDepth--;` |
|        - | 10654 | `	/* Restore the previous context */` |
|       73 | 10655 | `	pVm->pActiveCtx = pOldCtx;` |
|       73 | 10656 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10657 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       69 | 10658 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       69 | 10659 | `		pCtx->pFrame->pParent = 0;` |
|       69 | 10660 | `		if( pResult ){` |
|       27 | 10661 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10662 | `		}` |
|       69 | 10663 | `		return SXRET_OK;` |
|        - | 10664 | `	}` |
|        - | 10665 | `	/* Detach frame */` |
|        6 | 10666 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        6 | 10667 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        6 | 10668 | `		pCtx->pFrame->pParent = 0;` |
|        2 | 10669 | `	}` |
|        6 | 10670 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10671 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10672 | `		return PH7_ABORT;` |
|        - | 10673 | `	}` |
|        6 | 10674 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10675 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10676 | `		return PH7_EXCEPTION;` |
|        - | 10677 | `	}` |
|        - | 10678 | `	/* Normal completion */` |
|        6 | 10679 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        6 | 10680 | `	if( pResult ){` |
|        3 | 10681 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10682 | `	}` |
|        6 | 10683 | `	return SXRET_OK;` |
|       39 | 10684 |  |
|        - | 10685 | `/*` |
|        - | 10686 | ` * Resume a suspended fiber context.` |
|        - | 10687 | ` */` |
|      150 | 10688 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        5 | 10689 |  |
|        - | 10690 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10691 | `	sxi32 rc;` |
|      155 | 10692 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10693 | `		return SXERR_INVALID;` |
|        - | 10694 | `	}` |
|        - | 10695 | `	/* Bound fiber/generator nesting under the same cap; reject before mutating` |
|        - | 10696 | `	 * VM state so the abort is clean. */` |
|      155 | 10697 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10698 | `		return VmRecursionFatal(pVm);` |
|        - | 10699 | `	}` |
|        - | 10700 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10701 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10702 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      155 | 10703 | `	if( pResumeValue ){` |
|       43 | 10704 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       24 | 10705 | `	}else{` |
|      117 | 10706 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10707 | `	}` |
|      155 | 10708 | `	pCtx->nTos++;` |
|        - | 10709 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      155 | 10710 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      155 | 10711 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10712 | `	/* Save and set the active context */` |
|      155 | 10713 | `	pOldCtx = pVm->pActiveCtx;` |
|      155 | 10714 | `	pVm->pActiveCtx = pCtx;` |
|      155 | 10715 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      155 | 10716 | `	pVm->nRecursionDepth++;` |
|        - | 10717 | `	/* Resume execution from saved PC */` |
|      155 | 10718 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       75 | 10719 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|      150 | 10720 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0, FALSE);` |
|      155 | 10721 | `	pVm->nRecursionDepth--;` |
|        - | 10722 | `	/* Restore the previous context */` |
|      155 | 10723 | `	pVm->pActiveCtx = pOldCtx;` |
|      155 | 10724 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10725 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|      103 | 10726 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|      103 | 10727 | `		pCtx->pFrame->pParent = 0;` |
|      103 | 10728 | `		if( pResult ){` |
|       20 | 10729 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10730 | `		}` |
|      103 | 10731 | `		return SXRET_OK;` |
|        - | 10732 | `	}` |
|        - | 10733 | `	/* Detach frame */` |
|       57 | 10734 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       57 | 10735 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       57 | 10736 | `		pCtx->pFrame->pParent = 0;` |
|       26 | 10737 | `	}` |
|       57 | 10738 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10739 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10740 | `		return PH7_ABORT;` |
|        - | 10741 | `	}` |
|       57 | 10742 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10743 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10744 | `		return PH7_EXCEPTION;` |
|        - | 10745 | `	}` |
|        - | 10746 | `	/* Normal completion */` |
|       57 | 10747 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       57 | 10748 | `	if( pResult ){` |
|       23 | 10749 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10750 | `	}` |
|       57 | 10751 | `	return SXRET_OK;` |
|       80 | 10752 |  |
|        - | 10753 | `/*` |
|        - | 10754 | ` * Release an execution context and all its resources.` |
|        - | 10755 | ` */` |
|        4 | 10756 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10757 |  |
|        5 | 10758 | `	if( pCtx == 0 ){` |
|      ! 0 | 10759 | `		return;` |
|        - | 10760 | `	}` |
|        5 | 10761 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10762 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10763 | `		return;` |
|        - | 10764 | `	}` |
|        5 | 10765 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10766 | `	/* Release values */` |
|        5 | 10767 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10768 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10769 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10770 | `	if( pCtx->pFrame ){` |
|        - | 10771 | `		VmSlot *aSlot;` |
|        - | 10772 | `		sxu32 n;` |
|        - | 10773 | `		/* Free local variables */` |
|        5 | 10774 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10775 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10776 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10777 | `		}` |
|        - | 10778 | `		/* Remove local references */` |
|        5 | 10779 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10780 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10781 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10782 | `		}` |
|        5 | 10783 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10784 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10785 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10786 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10787 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10788 | `		pCtx->pFrame = 0;` |
|        2 | 10789 | `	}` |
|        - | 10790 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10791 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10792 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10793 | `	if( pCtx->pStack ){` |
|        5 | 10794 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10795 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10796 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10797 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10798 | `				pTos--;` |
|        1 | 10799 | `			}` |
|        2 | 10800 | `		}` |
|        5 | 10801 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10802 | `		pCtx->pStack = 0;` |
|        2 | 10803 | `	}` |
|        - | 10804 | `	/* Free the context itself */` |
|        5 | 10805 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10806 |  |
|        - | 10807 | `/*` |
|        - | 10808 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10809 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10810 | ` */` |
|       90 | 10811 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        5 | 10812 |  |
|        - | 10813 | `	ph7_class_instance *pThis;` |
|        - | 10814 | `	SyString sAttr;` |
|        - | 10815 | `	ph7_value *pAttr;` |
|       95 | 10816 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10817 | `		return 0;` |
|        - | 10818 | `	}` |
|       95 | 10819 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       95 | 10820 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10821 | `		return 0;` |
|        - | 10822 | `	}` |
|       95 | 10823 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       95 | 10824 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       95 | 10825 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       35 | 10826 | `		return 0;` |
|        - | 10827 | `	}` |
|       65 | 10828 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       50 | 10829 |  |
|        - | 10830 | `/*` |
|        - | 10831 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10832 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10833 | ` */` |
|       38 | 10834 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10835 |  |
|       43 | 10836 | `	ph7_vm *pVm = pCtx->pVm;` |
|       43 | 10837 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10838 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10839 | `			"Cannot suspend outside of a fiber");` |
|        - | 10840 | `	}` |
|       43 | 10841 | `	if( nArg > 0 ){` |
|       43 | 10842 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       24 | 10843 | `	}else{` |
|      ! 0 | 10844 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10845 | `	}` |
|       43 | 10846 | `	return PH7_SUSPEND;` |
|       24 | 10847 |  |
|        - | 10848 | `/*` |
|        - | 10849 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10850 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10851 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10852 | ` */` |
|       24 | 10853 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10854 |  |
|        - | 10855 | `	ph7_class_instance *pThis;` |
|        - | 10856 | `	ph7_value *pAttr;` |
|        - | 10857 | `	SyString sAttrName;` |
|       29 | 10858 | `	if( nArg < 2 ){` |
|      ! 0 | 10859 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10860 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10861 | `	}` |
|       29 | 10862 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10863 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10864 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10865 | `	}` |
|       29 | 10866 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       29 | 10867 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10868 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10869 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10870 | `	}` |
|        - | 10871 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       29 | 10872 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10873 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10874 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10875 | `	}` |
|        - | 10876 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       29 | 10877 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       29 | 10878 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       29 | 10879 | `	if( pAttr ){` |
|       29 | 10880 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10881 | `	}` |
|       29 | 10882 | `	return PH7_OK;` |
|       17 | 10883 |  |
|        - | 10884 | `/*` |
|        - | 10885 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10886 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10887 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10888 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10889 | ` */` |
|       24 | 10890 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10891 | `	ph7_class_instance **ppThis)` |
|        5 | 10892 |  |
|       29 | 10893 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10894 | `	ph7_value *pCallable;` |
|        - | 10895 | `	SyString sAttrName;` |
|       29 | 10896 | `	*ppThis = 0;` |
|       29 | 10897 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       29 | 10898 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       29 | 10899 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10900 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10901 | `		return 0;` |
|        - | 10902 | `	}` |
|       29 | 10903 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10904 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10905 | `		SyString sName;` |
|        - | 10906 | `		SyHashEntry *pEntry;` |
|        - | 10907 | `		ph7_vm_func *pFunc;` |
|       29 | 10908 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       29 | 10909 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       29 | 10910 | `		if( pEntry == 0 ){` |
|      ! 0 | 10911 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10912 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10913 | `			return 0;` |
|        - | 10914 | `		}` |
|       29 | 10915 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       29 | 10916 | `		return pFunc;` |
|      ! 0 | 10917 | `	}else{` |
|        - | 10918 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10919 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10920 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10921 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10922 | `		if( pMethod == 0 ){` |
|      ! 0 | 10923 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10924 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10925 | `			return 0;` |
|        - | 10926 | `		}` |
|      ! 0 | 10927 | `		*ppThis = pClosure;` |
|      ! 0 | 10928 | `		return &pMethod->sFunc;` |
|        - | 10929 | `	}` |
|       17 | 10930 |  |
|        - | 10931 | `/*` |
|        - | 10932 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10933 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10934 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10935 | ` */` |
|       72 | 10936 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10937 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        5 | 10938 |  |
|       77 | 10939 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10940 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10941 | `	sxu32 nFormal, n;` |
|        - | 10942 | `	VmSlot sSlot;` |
|        - | 10943 | `	sxi32 rc;` |
|        - | 10944 | `	/* Install $this for closure/method callables */` |
|       77 | 10945 | `	if( pClosureThis ){` |
|        - | 10946 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10947 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10948 | `		if( pObj ){` |
|      ! 0 | 10949 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10950 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10951 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10952 | `		}` |
|      ! 0 | 10953 | `	}` |
|        - | 10954 | `	/* Install static variables */` |
|       77 | 10955 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10956 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10957 | `		ph7_value *pVal;` |
|      ! 0 | 10958 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10959 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10960 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10961 | `			if( pVal ){` |
|      ! 0 | 10962 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10963 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10964 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10965 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10966 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10967 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|      ! 0 | 10968 | `				}` |
|      ! 0 | 10969 | `			}` |
|      ! 0 | 10970 | `		}` |
|      ! 0 | 10971 | `	}` |
|        - | 10972 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       77 | 10973 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       77 | 10974 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       95 | 10975 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10976 | `		ph7_value *pObj;` |
|       20 | 10977 | `		if( n < (sxu32)nArg ){` |
|        - | 10978 | `			/* Argument provided — install with type casting */` |
|       20 | 10979 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10980 | `			if( pObj ){` |
|       20 | 10981 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10982 | `				/* Type casting */` |
|       20 | 10983 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10984 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10985 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10986 | `						if( xCast ){` |
|      ! 0 | 10987 | `							xCast(pObj);` |
|      ! 0 | 10988 | `						}` |
|      ! 0 | 10989 | `					}` |
|      ! 0 | 10990 | `				}` |
|       20 | 10991 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10992 | `				sSlot.pUserData = 0;` |
|       20 | 10993 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10994 | `			}` |
|        9 | 10995 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10996 | `			/* Default value */` |
|      ! 0 | 10997 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10998 | `			if( pObj ){` |
|      ! 0 | 10999 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      ! 0 | 11000 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11001 | `					return rc;` |
|        - | 11002 | `				}` |
|      ! 0 | 11003 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 11004 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 11005 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 11006 | `						if( xCast ){` |
|      ! 0 | 11007 | `							xCast(pObj);` |
|      ! 0 | 11008 | `						}` |
|      ! 0 | 11009 | `					}` |
|      ! 0 | 11010 | `				}` |
|      ! 0 | 11011 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 11012 | `				sSlot.pUserData = 0;` |
|      ! 0 | 11013 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 11014 | `			}` |
|      ! 0 | 11015 | `		}` |
|       11 | 11016 | `	}` |
|        - | 11017 | `	/* Install closure environment (captured variables) */` |
|       77 | 11018 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 11019 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 11020 | `		ph7_value *pValue;` |
|        - | 11021 | `		sxu32 iEnv;` |
|        3 | 11022 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 11023 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 11024 | `			pEnv = &aEnv[iEnv];` |
|        7 | 11025 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 11026 | `				continue;` |
|        - | 11027 | `			}` |
|        5 | 11028 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 11029 | `			if( pValue == 0 ){` |
|      ! 0 | 11030 | `				continue;` |
|        - | 11031 | `			}` |
|        5 | 11032 | `			PH7_MemObjRelease(pValue);` |
|        5 | 11033 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 11034 | `		}` |
|        1 | 11035 | `	}` |
|       77 | 11036 | `	return SXRET_OK;` |
|       41 | 11037 |  |
|        - | 11038 | `/*` |
|        - | 11039 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 11040 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 11041 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 11042 | ` */` |
|       26 | 11043 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11044 |  |
|       31 | 11045 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11046 | `	ph7_class_instance *pThis;` |
|        - | 11047 | `	ph7_class_instance *pClosureThis;` |
|        - | 11048 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 11049 | `	ph7_vm_func *pFunc;` |
|        - | 11050 | `	ph7_value sResult;` |
|        - | 11051 | `	ph7_value *pCtxAttr;` |
|        - | 11052 | `	SyString sAttrName;` |
|        - | 11053 | `	sxi32 rc;` |
|       31 | 11054 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11055 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 11056 | `	}` |
|       31 | 11057 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11058 | `	/* Check if already started (has a __ctx) */` |
|       31 | 11059 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       31 | 11060 | `	if( pExecCtx != 0 ){` |
|        3 | 11061 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11062 | `			"Cannot start a fiber that has already been started");` |
|        - | 11063 | `	}` |
|        - | 11064 | `	/* Resolve callable */` |
|       29 | 11065 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       29 | 11066 | `	if( pFunc == 0 ){` |
|      ! 0 | 11067 | `		return PH7_EXCEPTION;` |
|        - | 11068 | `	}` |
|        - | 11069 | `	/* Create execution context now that we know the function */` |
|       29 | 11070 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       29 | 11071 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11072 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11073 | `			"Fiber::start(): out of memory");` |
|        - | 11074 | `	}` |
|        - | 11075 | `	/* Store context in $this->__ctx */` |
|       29 | 11076 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       29 | 11077 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       29 | 11078 | `	if( pCtxAttr ){` |
|       29 | 11079 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       29 | 11080 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 11081 | `	}` |
|        - | 11082 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 11083 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 11084 | `	 * into the fiber's frame, not the caller's. */` |
|       29 | 11085 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       29 | 11086 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 11087 | `	/* Unpack the args array and install into the frame */` |
|        - | 11088 | `	{` |
|       29 | 11089 | `		ph7_value **apValues = 0;` |
|       29 | 11090 | `		ph7_value *aStore = 0;` |
|       29 | 11091 | `		int nActual = 0;` |
|       29 | 11092 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       29 | 11093 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 11094 | `			ph7_hashmap_node *pNode;` |
|       29 | 11095 | `			sxu32 nCount = pMap->nEntry;` |
|       29 | 11096 | `			if( nCount > 0 ){` |
|        3 | 11097 | `				sxu32 idx = 0;` |
|        4 | 11098 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 11099 | `					nCount * sizeof(ph7_value *));` |
|        4 | 11100 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 11101 | `					nCount * sizeof(ph7_value));` |
|        3 | 11102 | `				if( apValues && aStore ){` |
|        3 | 11103 | `					pNode = pMap->pFirst;` |
|        7 | 11104 | `					while( pNode && idx < nCount ){` |
|        - | 11105 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 11106 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 11107 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 11108 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 11109 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 11110 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 11111 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 11112 | `						if( pSrc ){` |
|        5 | 11113 | `							aStore[idx] = *pSrc;` |
|        3 | 11114 | `						}else{` |
|      ! 0 | 11115 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 11116 | `						}` |
|        5 | 11117 | `						apValues[idx] = &aStore[idx];` |
|        5 | 11118 | `						idx++;` |
|        5 | 11119 | `						pNode = pNode->pPrev;` |
|        1 | 11120 | `					}` |
|        3 | 11121 | `					nActual = (int)idx;` |
|        1 | 11122 | `				}` |
|        1 | 11123 | `			}` |
|       12 | 11124 | `		}` |
|       29 | 11125 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       29 | 11126 | `		if( aStore ){` |
|        3 | 11127 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 11128 | `		}` |
|       29 | 11129 | `		if( apValues ){` |
|        3 | 11130 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 11131 | `		}` |
|        - | 11132 | `	}` |
|        - | 11133 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       29 | 11134 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       29 | 11135 | `	pExecCtx->pFrame->pParent = 0;` |
|       29 | 11136 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11137 | `		return PH7_ABORT;` |
|        - | 11138 | `	}` |
|       29 | 11139 | `	PH7_MemObjInit(pVm, &sResult);` |
|       29 | 11140 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       29 | 11141 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 11142 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11143 | `		return PH7_ABORT;` |
|        - | 11144 | `	}` |
|       29 | 11145 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 11146 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11147 | `		return PH7_EXCEPTION;` |
|        - | 11148 | `	}` |
|       29 | 11149 | `	ph7_result_value(pCtx, &sResult);` |
|       29 | 11150 | `	PH7_MemObjRelease(&sResult);` |
|       29 | 11151 | `	return PH7_OK;` |
|       18 | 11152 |  |
|        - | 11153 | `/*` |
|        - | 11154 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 11155 | ` */` |
|       36 | 11156 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11157 |  |
|       41 | 11158 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11159 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 11160 | `	ph7_value sResult;` |
|        - | 11161 | `	ph7_value *pResumeVal;` |
|        - | 11162 | `	sxi32 rc;` |
|       41 | 11163 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11164 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 11165 | `		return PH7_OK;` |
|        - | 11166 | `	}` |
|       41 | 11167 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       41 | 11168 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11169 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 11170 | `		return PH7_OK;` |
|        - | 11171 | `	}` |
|       41 | 11172 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11173 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11174 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 11175 | `	}` |
|       39 | 11176 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       39 | 11177 | `	PH7_MemObjInit(pVm, &sResult);` |
|       39 | 11178 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       39 | 11179 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 11180 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11181 | `		return PH7_ABORT;` |
|        - | 11182 | `	}` |
|       39 | 11183 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 11184 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11185 | `		return PH7_EXCEPTION;` |
|        - | 11186 | `	}` |
|       39 | 11187 | `	ph7_result_value(pCtx, &sResult);` |
|       39 | 11188 | `	PH7_MemObjRelease(&sResult);` |
|       39 | 11189 | `	return PH7_OK;` |
|       23 | 11190 |  |
|        - | 11191 | `/*` |
|        - | 11192 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 11193 | ` */` |
|        6 | 11194 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 11195 |  |
|        9 | 11196 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11197 | `	ph7_exec_ctx *pExecCtx;` |
|        9 | 11198 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11199 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11200 | `		return PH7_OK;` |
|        - | 11201 | `	}` |
|        9 | 11202 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        9 | 11203 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11204 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11205 | `		return PH7_OK;` |
|        - | 11206 | `	}` |
|        9 | 11207 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11208 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11209 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11210 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 11211 | `		}` |
|      ! 0 | 11212 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11213 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 11214 | `	}` |
|        9 | 11215 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        9 | 11216 | `	return PH7_OK;` |
|        6 | 11217 |  |
|        - | 11218 | `/*` |
|        - | 11219 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 11220 | ` */` |
|        6 | 11221 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11222 |  |
|        - | 11223 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11224 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11225 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11226 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 11227 | `	return PH7_OK;` |
|        4 | 11228 |  |
|      ! 0 | 11229 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11230 |  |
|        - | 11231 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 11232 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 11233 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11234 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 11235 | `	return PH7_OK;` |
|      ! 0 | 11236 |  |
|        6 | 11237 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11238 |  |
|        - | 11239 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11240 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11241 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11242 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 11243 | `	return PH7_OK;` |
|        4 | 11244 |  |
|        6 | 11245 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11246 |  |
|        - | 11247 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11248 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11249 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11250 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 11251 | `	return PH7_OK;` |
|        4 | 11252 |  |
|        - | 11253 | `/*` |
|        - | 11254 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 11255 | ` */` |
|        4 | 11256 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11257 |  |
|        5 | 11258 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11259 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 11260 | `	if( nArg < 1 ){` |
|      ! 0 | 11261 | `		return PH7_OK;` |
|        - | 11262 | `	}` |
|        5 | 11263 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 11264 | `	if( pExecCtx ){` |
|        5 | 11265 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 11266 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 11267 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 11268 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11269 | `			SyString sAttrName;` |
|        - | 11270 | `			ph7_value *pAttr;` |
|        5 | 11271 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 11272 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 11273 | `			if( pAttr ){` |
|        5 | 11274 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 11275 | `			}` |
|        2 | 11276 | `		}` |
|        2 | 11277 | `	}` |
|        5 | 11278 | `	return PH7_OK;` |
|        3 | 11279 |  |
|        - | 11280 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 11281 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 11282 |  |
|        - | 11283 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11284 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 11285 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 11286 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 11287 |  |
|      ! 0 | 11288 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 11289 |  |
|        - | 11290 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11291 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 11292 | `	ph7_exec_ctx *pCtx;` |
|        - | 11293 | `	ph7_vm_func *pFunc;` |
|        - | 11294 | `	ph7_value *pCallable;` |
|        - | 11295 | `	ph7_value *pCtxAttr;` |
|        - | 11296 | `	SyString sAttrName;` |
|        - | 11297 | `	/* Must not already be started */` |
|      ! 0 | 11298 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11299 | `	if( pCtx != 0 ){` |
|      ! 0 | 11300 | `		return SXERR_INVALID;` |
|        - | 11301 | `	}` |
|      ! 0 | 11302 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11303 | `		return SXERR_INVALID;` |
|        - | 11304 | `	}` |
|      ! 0 | 11305 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 11306 | `	/* Get the callable */` |
|      ! 0 | 11307 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 11308 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11309 | `	if( pCallable == 0 ){` |
|      ! 0 | 11310 | `		return SXERR_INVALID;` |
|        - | 11311 | `	}` |
|        - | 11312 | `	/* Resolve callable */` |
|      ! 0 | 11313 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 11314 | `		SyString sName;` |
|        - | 11315 | `		SyHashEntry *pEntry;` |
|      ! 0 | 11316 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 11317 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 11318 | `		if( pEntry == 0 ){` |
|      ! 0 | 11319 | `			return SXERR_NOTFOUND;` |
|        - | 11320 | `		}` |
|      ! 0 | 11321 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 11322 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11323 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 11324 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 11325 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 11326 | `		if( pMethod == 0 ){` |
|      ! 0 | 11327 | `			return SXERR_INVALID;` |
|        - | 11328 | `		}` |
|      ! 0 | 11329 | `		pClosureThis = pClosure;` |
|      ! 0 | 11330 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 11331 | `	}else{` |
|      ! 0 | 11332 | `		return SXERR_INVALID;` |
|        - | 11333 | `	}` |
|        - | 11334 | `	/* Create context */` |
|      ! 0 | 11335 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 11336 | `	if( pCtx == 0 ){` |
|      ! 0 | 11337 | `		return SXERR_MEM;` |
|        - | 11338 | `	}` |
|        - | 11339 | `	/* Store in __ctx */` |
|      ! 0 | 11340 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11341 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11342 | `	if( pCtxAttr ){` |
|      ! 0 | 11343 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 11344 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 11345 | `	}` |
|        - | 11346 | `	/* Set up frame with args */` |
|      ! 0 | 11347 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 11348 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 11349 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 11350 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 11351 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 11352 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 11353 |  |
|      ! 0 | 11354 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 11355 |  |
|      ! 0 | 11356 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11357 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 11358 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 11359 |  |
|      ! 0 | 11360 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11361 |  |
|      ! 0 | 11362 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11363 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 11364 |  |
|      ! 0 | 11365 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11366 |  |
|      ! 0 | 11367 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11368 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 11369 |  |
|      ! 0 | 11370 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11371 |  |
|      ! 0 | 11372 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11373 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 11374 | `	return &pCtx->sRetValue;` |
|      ! 0 | 11375 |  |
|        - | 11376 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 11377 | `/*` |
|        - | 11378 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 11379 | ` */` |
|       48 | 11380 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 | 11381 |  |
|        - | 11382 | `	ph7_generator *pGen;` |
|       53 | 11383 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       53 | 11384 | `	if( pGen == 0 ){` |
|      ! 0 | 11385 | `		return 0;` |
|        - | 11386 | `	}` |
|       53 | 11387 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       53 | 11388 | `	pGen->pCtx = pCtx;` |
|       53 | 11389 | `	pGen->iImplicitKey = 0;` |
|       53 | 11390 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       53 | 11391 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 11392 | `	/* Link the generator back to the exec context */` |
|       53 | 11393 | `	pCtx->pPrivate = pGen;` |
|       53 | 11394 | `	return pGen;` |
|       29 | 11395 |  |
|        - | 11396 | `/*` |
|        - | 11397 | ` * Release a generator and its execution context.` |
|        - | 11398 | ` */` |
|      ! 0 | 11399 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 11400 |  |
|      ! 0 | 11401 | `	if( pGen == 0 ){` |
|      ! 0 | 11402 | `		return;` |
|        - | 11403 | `	}` |
|      ! 0 | 11404 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 11405 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 11406 | `	if( pGen->pCtx ){` |
|      ! 0 | 11407 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 11408 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 11409 | `		pGen->pCtx = 0;` |
|      ! 0 | 11410 | `	}` |
|      ! 0 | 11411 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 11412 |  |
|        - | 11413 | `/*` |
|        - | 11414 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 11415 | ` */` |
|      496 | 11416 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        5 | 11417 |  |
|        - | 11418 | `	ph7_class_instance *pThis;` |
|        - | 11419 | `	SyString sAttr;` |
|        - | 11420 | `	ph7_value *pAttr;` |
|      501 | 11421 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11422 | `		return 0;` |
|        - | 11423 | `	}` |
|      501 | 11424 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      501 | 11425 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 11426 | `		return 0;` |
|        - | 11427 | `	}` |
|      501 | 11428 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      501 | 11429 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      501 | 11430 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 11431 | `		return 0;` |
|        - | 11432 | `	}` |
|      501 | 11433 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      253 | 11434 |  |
|        - | 11435 | `/*` |
|        - | 11436 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 11437 | ` */` |
|       44 | 11438 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11439 |  |
|        - | 11440 | `	ph7_generator *pGen;` |
|        - | 11441 | `	sxi32 rc;` |
|       49 | 11442 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 | 11443 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 | 11444 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 | 11445 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       49 | 11446 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 | 11447 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 | 11448 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       22 | 11449 | `	}` |
|       49 | 11450 | `	return PH7_OK;` |
|       27 | 11451 |  |
|        - | 11452 | `/*` |
|        - | 11453 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 11454 | ` */` |
|      142 | 11455 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 11456 |  |
|        - | 11457 | `	ph7_generator *pGen;` |
|      146 | 11458 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      146 | 11459 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      146 | 11460 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|      146 | 11461 | `	return PH7_OK;` |
|       75 | 11462 |  |
|        - | 11463 | `/*` |
|        - | 11464 | ` * Generator::current() — return the last yielded value.` |
|        - | 11465 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11466 | ` */` |
|      124 | 11467 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11468 |  |
|        - | 11469 | `	ph7_generator *pGen;` |
|        - | 11470 | `	sxi32 rc;` |
|      129 | 11471 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      129 | 11472 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      129 | 11473 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      129 | 11474 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11475 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11476 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11477 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11478 | `	}` |
|      129 | 11479 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      129 | 11480 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       67 | 11481 | `	}else{` |
|      ! 0 | 11482 | `		ph7_result_null(pCtx);` |
|        - | 11483 | `	}` |
|      129 | 11484 | `	return PH7_OK;` |
|       67 | 11485 |  |
|        - | 11486 | `/*` |
|        - | 11487 | ` * Generator::key() — return the last yielded key.` |
|        - | 11488 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11489 | ` */` |
|       68 | 11490 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 11491 |  |
|        - | 11492 | `	ph7_generator *pGen;` |
|        - | 11493 | `	sxi32 rc;` |
|       71 | 11494 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       71 | 11495 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       71 | 11496 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       71 | 11497 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11498 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11499 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11500 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11501 | `	}` |
|       71 | 11502 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       71 | 11503 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|       37 | 11504 | `	}else{` |
|      ! 0 | 11505 | `		ph7_result_null(pCtx);` |
|        - | 11506 | `	}` |
|       71 | 11507 | `	return PH7_OK;` |
|       37 | 11508 |  |
|        - | 11509 | `/*` |
|        - | 11510 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11511 | ` */` |
|      112 | 11512 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11513 |  |
|        - | 11514 | `	ph7_generator *pGen;` |
|        - | 11515 | `	sxi32 rc;` |
|      117 | 11516 | `	if( nArg < 1 ) return PH7_OK;` |
|      117 | 11517 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      117 | 11518 | `	if( pGen == 0 ) return PH7_OK;` |
|      117 | 11519 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11520 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      117 | 11521 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      117 | 11522 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       61 | 11523 | `	}else{` |
|      ! 0 | 11524 | `		return PH7_OK;` |
|        - | 11525 | `	}` |
|      117 | 11526 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      117 | 11527 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      117 | 11528 | `	return PH7_OK;` |
|       61 | 11529 |  |
|        - | 11530 | `/*` |
|        - | 11531 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11532 | ` */` |
|        4 | 11533 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11534 |  |
|        - | 11535 | `	ph7_generator *pGen;` |
|        - | 11536 | `	ph7_value *pSendVal;` |
|        - | 11537 | `	sxi32 rc;` |
|        5 | 11538 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11539 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11540 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11541 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11542 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11543 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11544 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11545 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11546 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11547 | `	}else{` |
|      ! 0 | 11548 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11549 | `		return PH7_OK;` |
|        - | 11550 | `	}` |
|        5 | 11551 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11552 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11553 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11554 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11555 | `	}else{` |
|        3 | 11556 | `		ph7_result_null(pCtx);` |
|        - | 11557 | `	}` |
|        5 | 11558 | `	return PH7_OK;` |
|        3 | 11559 |  |
|        - | 11560 | `/*` |
|        - | 11561 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11562 | ` *` |
|        - | 11563 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11564 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11565 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11566 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11567 | ` * the exception to the caller.` |
|        - | 11568 | ` */` |
|      ! 0 | 11569 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11570 |  |
|        - | 11571 | `	ph7_generator *pGen;` |
|        - | 11572 | `	const char *zMsg;` |
|        - | 11573 | `	int nLen;` |
|      ! 0 | 11574 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11575 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11576 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11577 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11578 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11579 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11580 | `			"Cannot throw into a closed generator");` |
|        - | 11581 | `	}` |
|        - | 11582 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11583 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11584 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11585 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11586 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11587 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11588 | `	nLen = 0;` |
|      ! 0 | 11589 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11590 | `		/* Try to get the exception's message */` |
|        - | 11591 | `		SyString sAttr;` |
|        - | 11592 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11593 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11594 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11595 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11596 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11597 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11598 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11599 | `		}` |
|      ! 0 | 11600 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11601 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11602 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11603 | `	}` |
|      ! 0 | 11604 | `	(void)nLen;` |
|      ! 0 | 11605 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11606 |  |
|        - | 11607 | `/*` |
|        - | 11608 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11609 | ` */` |
|        2 | 11610 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11611 |  |
|        - | 11612 | `	ph7_generator *pGen;` |
|        3 | 11613 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11614 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11615 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11616 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11617 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11618 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11619 | `	}` |
|        3 | 11620 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11621 | `	return PH7_OK;` |
|        2 | 11622 |  |
|        - | 11623 | `/*` |
|        - | 11624 | ` * Generator::__destruct() — clean up.` |
|        - | 11625 | ` */` |
|      ! 0 | 11626 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11627 |  |
|        - | 11628 | `	ph7_generator *pGen;` |
|      ! 0 | 11629 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11630 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11631 | `	if( pGen ){` |
|      ! 0 | 11632 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11633 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11634 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11635 | `			SyString sAttrName;` |
|        - | 11636 | `			ph7_value *pAttr;` |
|      ! 0 | 11637 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11638 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11639 | `			if( pAttr ){` |
|      ! 0 | 11640 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11641 | `			}` |
|      ! 0 | 11642 | `		}` |
|      ! 0 | 11643 | `	}` |
|      ! 0 | 11644 | `	return PH7_OK;` |
|      ! 0 | 11645 |  |
|        - | 11646 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11647 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11648 | `/*` |
|        - | 11649 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11650 | ` * the desired message.` |
|        - | 11651 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11652 | ` * in 'api.c' for additional information.` |
|        - | 11653 | ` */` |
|      370 | 11654 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11655 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11656 | `	SyString *pString /* Message to output */` |
|        - | 11657 | `	)` |
|        3 | 11658 |  |
|      373 | 11659 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      373 | 11660 | `	sxi32 rc = SXRET_OK;` |
|        - | 11661 | `	/* Call the output consumer */` |
|      373 | 11662 | `	if( pString->nByte > 0 ){` |
|      373 | 11663 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      373 | 11664 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11665 | `	}` |
|      373 | 11666 | `	return rc;` |
|        3 | 11667 |  |
|        - | 11668 | `/*` |
|        - | 11669 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11670 | ` * callback to consume the formatted message.` |
|        - | 11671 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11672 | ` * in 'api.c' for additional information.` |
|        - | 11673 | ` */` |
|        2 | 11674 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11675 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11676 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11677 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11678 | `	)` |
|        1 | 11679 |  |
|        3 | 11680 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11681 | `	sxi32 rc = SXRET_OK;` |
|        - | 11682 | `	SyBlob sWorker;` |
|        - | 11683 | `	/* Format the message and call the output consumer */` |
|        3 | 11684 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11685 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11686 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11687 | `		/* Consume the formatted message */` |
|        3 | 11688 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11689 | `	}` |
|        3 | 11690 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11691 | `	/* Release the working buffer */` |
|        3 | 11692 | `	SyBlobRelease(&sWorker);` |
|        3 | 11693 | `	return rc;` |
|        1 | 11694 |  |
|        - | 11695 | `/*` |
|        - | 11696 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11697 | ` * This function never fail and always return a pointer` |
|        - | 11698 | ` * to a null terminated string.` |
|        - | 11699 | ` */` |
|       12 | 11700 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11701 |  |
|       13 | 11702 | `	const char *zOp = "Unknown     ";` |
|       13 | 11703 | `	switch(nOp){` |
|        3 | 11704 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11705 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11706 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11707 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11708 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11709 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11710 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11711 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11712 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11713 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11714 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11715 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11716 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11717 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11718 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11719 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11720 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11721 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11722 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11723 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11724 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11725 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11726 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11727 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11728 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11729 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11730 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11731 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11732 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11733 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11734 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11735 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11736 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11737 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11738 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11739 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11740 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11741 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11742 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11743 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11744 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11745 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11746 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11747 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11748 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11749 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11750 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11751 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11752 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11753 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11754 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11755 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11756 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11757 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11758 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11759 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11760 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11761 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11762 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11763 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11764 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11765 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11766 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11767 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11768 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11769 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11770 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11771 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11772 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11773 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11774 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11775 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11776 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11777 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11778 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11779 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11780 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11781 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11782 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11783 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11784 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11785 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11786 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11787 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11788 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11789 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11790 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11791 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11792 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11793 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11794 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11795 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11796 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11797 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11798 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11799 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11800 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11801 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11802 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11803 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11804 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11805 | `	default:` |
|      ! 0 | 11806 | `		break;` |
|        - | 11807 | `	}` |
|       13 | 11808 | `	return zOp;` |
|        1 | 11809 |  |
|        - | 11810 | `/*` |
|        - | 11811 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11812 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11813 | ` * is responsible of consuming the generated dump.` |
|        - | 11814 | ` */` |
|        2 | 11815 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11816 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11817 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11818 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11819 | `	)` |
|        1 | 11820 |  |
|        - | 11821 | `	sxi32 rc;` |
|        3 | 11822 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11823 | `	return rc;` |
|        1 | 11824 |  |
|        - | 11825 | `/*` |
|        - | 11826 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11827 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11828 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11829 | ` * in 'compile.c' for additional information.` |
|        - | 11830 | ` */` |
|       14 | 11831 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11832 |  |
|       15 | 11833 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11834 | `	/* Evaluate and expand constant value */` |
|       15 | 11835 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       15 | 11836 |  |
|        - | 11837 | `/*` |
|        - | 11838 | ` * Section:` |
|        - | 11839 | ` *  Function handling functions.` |
|        - | 11840 | ` * Status:` |
|        - | 11841 | ` *    Stable.` |
|        - | 11842 | ` */` |
|        - | 11843 | `/*` |
|        - | 11844 | ` * int func_num_args(void)` |
|        - | 11845 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11846 | ` * Parameters` |
|        - | 11847 | ` *   None.` |
|        - | 11848 | ` * Return` |
|        - | 11849 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11850 | ` *  or -1 if called from the globe scope.` |
|        - | 11851 | ` */` |
|      986 | 11852 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 11853 |  |
|        - | 11854 | `	VmFrame *pFrame;` |
|        - | 11855 | `	ph7_vm *pVm;` |
|        - | 11856 | `	/* Point to the target VM */` |
|      991 | 11857 | `	pVm = pCtx->pVm;` |
|        - | 11858 | `	/* Current frame */` |
|      991 | 11859 | `	pFrame = pVm->pFrame;` |
|      991 | 11860 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      991 | 11861 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11862 | `		SXUNUSED(nArg);` |
|      ! 0 | 11863 | `		SXUNUSED(apArg);` |
|        - | 11864 | `		/* Global frame,return -1 */` |
|      ! 0 | 11865 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11866 | `		return SXRET_OK;` |
|        - | 11867 | `	}` |
|        - | 11868 | `	/* Total number of arguments passed to the enclosing function */` |
|      991 | 11869 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      991 | 11870 | `	ph7_result_int(pCtx,nArg);` |
|      991 | 11871 | `	return SXRET_OK;` |
|      498 | 11872 |  |
|        - | 11873 | `/*` |
|        - | 11874 | ` * value func_get_arg(int $arg_num)` |
|        - | 11875 | ` *   Return an item from the argument list.` |
|        - | 11876 | ` * Parameters` |
|        - | 11877 | ` *  Argument number(index start from zero).` |
|        - | 11878 | ` * Return` |
|        - | 11879 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11880 | ` */` |
|       22 | 11881 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11882 |  |
|       24 | 11883 | `	ph7_value *pObj = 0;` |
|       24 | 11884 | `	VmSlot *pSlot = 0;` |
|        - | 11885 | `	VmFrame *pFrame;` |
|        - | 11886 | `	ph7_vm *pVm;` |
|        - | 11887 | `	/* Point to the target VM */` |
|       24 | 11888 | `	pVm = pCtx->pVm;` |
|        - | 11889 | `	/* Current frame */` |
|       24 | 11890 | `	pFrame = pVm->pFrame;` |
|       24 | 11891 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11892 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11893 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11894 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11895 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11896 | `		return SXRET_OK;` |
|        - | 11897 | `	}` |
|        - | 11898 | `	/* Extract the desired index */` |
|       21 | 11899 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11900 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11901 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11902 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11903 | `		return SXRET_OK;` |
|        - | 11904 | `	}` |
|        - | 11905 | `	/* Extract the desired argument */` |
|       21 | 11906 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11907 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11908 | `			/* Return the desired argument */` |
|       21 | 11909 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11910 | `		}else{` |
|        - | 11911 | `			/* No such argument,return false */` |
|      ! 0 | 11912 | `			ph7_result_bool(pCtx,0);` |
|        - | 11913 | `		}` |
|       11 | 11914 | `	}else{` |
|        - | 11915 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11916 | `		ph7_result_bool(pCtx,0);` |
|        - | 11917 | `	}` |
|       21 | 11918 | `	return SXRET_OK;` |
|       13 | 11919 |  |
|        - | 11920 | `/*` |
|        - | 11921 | ` * array func_get_args_byref(void)` |
|        - | 11922 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11923 | ` * Parameters` |
|        - | 11924 | ` *  None.` |
|        - | 11925 | ` * Return` |
|        - | 11926 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11927 | ` *  member of the current user-defined function's argument list.` |
|        - | 11928 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11929 | ` * NOTE:` |
|        - | 11930 | ` *  Arguments are returned to the array by reference.` |
|        - | 11931 | ` */` |
|        2 | 11932 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11933 |  |
|        - | 11934 | `	ph7_value *pArray;` |
|        - | 11935 | `	VmFrame *pFrame;` |
|        - | 11936 | `	VmSlot *aSlot;` |
|        - | 11937 | `	sxu32 n;` |
|        - | 11938 | `	/* Point to the current frame */` |
|        3 | 11939 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11940 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11941 | `	if( pFrame->pParent == 0 ){` |
|        - | 11942 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11943 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11944 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11945 | `		return SXRET_OK;` |
|        - | 11946 | `	}` |
|        - | 11947 | `	/* Create a new array */` |
|        3 | 11948 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11949 | `	if( pArray == 0 ){` |
|      ! 0 | 11950 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11951 | `		SXUNUSED(apArg);` |
|      ! 0 | 11952 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11953 | `		return SXRET_OK;` |
|        - | 11954 | `	}` |
|        - | 11955 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11956 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11957 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11958 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11959 | `	}` |
|        - | 11960 | `	/* Return the freshly created array */` |
|        3 | 11961 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11962 | `	return SXRET_OK;` |
|        2 | 11963 |  |
|        - | 11964 | `/*` |
|        - | 11965 | ` * array func_get_args(void)` |
|        - | 11966 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11967 | ` * Parameters` |
|        - | 11968 | ` *  None.` |
|        - | 11969 | ` * Return` |
|        - | 11970 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11971 | ` *  member of the current user-defined function's argument list.` |
|        - | 11972 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11973 | ` */` |
|       88 | 11974 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 11975 |  |
|       93 | 11976 | `	ph7_value *pObj = 0;` |
|        - | 11977 | `	ph7_value *pArray;` |
|        - | 11978 | `	VmFrame *pFrame;` |
|        - | 11979 | `	VmSlot *aSlot;` |
|        - | 11980 | `	sxu32 n;` |
|        - | 11981 | `	/* Point to the current frame */` |
|       93 | 11982 | `	pFrame = pCtx->pVm->pFrame;` |
|       93 | 11983 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       93 | 11984 | `	if( pFrame->pParent == 0 ){` |
|        - | 11985 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11986 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11987 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11988 | `		return SXRET_OK;` |
|        - | 11989 | `	}` |
|        - | 11990 | `	/* Create a new array */` |
|       93 | 11991 | `	pArray = ph7_context_new_array(pCtx);` |
|       93 | 11992 | `	if( pArray == 0 ){` |
|      ! 0 | 11993 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11994 | `		SXUNUSED(apArg);` |
|      ! 0 | 11995 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11996 | `		return SXRET_OK;` |
|        - | 11997 | `	}` |
|        - | 11998 | `	/* Start filling the array with the given arguments */` |
|       93 | 11999 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      225 | 12000 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 12001 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 12002 | `		if( pObj ){` |
|      134 | 12003 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 12004 | `		}` |
|       68 | 12005 | `	}` |
|        - | 12006 | `	/* Return the freshly created array */` |
|       93 | 12007 | `	ph7_result_value(pCtx,pArray);` |
|       93 | 12008 | `	return SXRET_OK;` |
|       49 | 12009 |  |
|        - | 12010 | `/*` |
|        - | 12011 | ` * bool function_exists(string $name)` |
|        - | 12012 | ` *  Return TRUE if the given function has been defined.` |
|        - | 12013 | ` * Parameters` |
|        - | 12014 | ` *  The name of the desired function.` |
|        - | 12015 | ` * Return` |
|        - | 12016 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 12017 | ` */` |
|     1748 | 12018 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 12019 |  |
|        - | 12020 | `	const char *zName;` |
|        - | 12021 | `	ph7_vm *pVm;` |
|        - | 12022 | `	int nLen;` |
|        - | 12023 | `	int res;` |
|     1753 | 12024 | `	if( nArg < 1 ){` |
|        - | 12025 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 12026 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12027 | `		return SXRET_OK;` |
|        - | 12028 | `	}` |
|        - | 12029 | `	/* Point to the target VM */` |
|     1753 | 12030 | `	pVm = pCtx->pVm;` |
|        - | 12031 | `	/* Extract the function name */` |
|     1753 | 12032 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12033 | `	/* Assume the function is not defined */` |
|     1753 | 12034 | `	res = 0;` |
|        - | 12035 | `	/* Perform the lookup */` |
|     2625 | 12036 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1744 | 12037 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12038 | `			/* Function is defined */` |
|      271 | 12039 | `			res = 1;` |
|      133 | 12040 | `	}` |
|     1753 | 12041 | `	ph7_result_bool(pCtx,res);` |
|     1753 | 12042 | `	return SXRET_OK;` |
|      879 | 12043 |  |
|        - | 12044 | `/*` |
|        - | 12045 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 12046 | ` * [i.e: Whether it is callable or not].` |
|        - | 12047 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 12048 | ` */` |
|    24342 | 12049 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        5 | 12050 |  |
|    24347 | 12051 | `	int res = 0;` |
|    24347 | 12052 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12053 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 12054 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 12055 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 12056 | `		 * standard PHP behavior. */` |
|       21 | 12057 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       21 | 12058 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       19 | 12059 | `			res = 1;` |
|       11 | 12060 | `		}` |
|        9 | 12061 | `		(void)CallInvoke;` |
|    24338 | 12062 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       30 | 12063 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       30 | 12064 | `		if( pMap->nEntry == 2 ){` |
|        - | 12065 | `			ph7_class *pClass;` |
|        - | 12066 | `			ph7_value *pV;` |
|        - | 12067 | `			/* Extract the target class */` |
|       13 | 12068 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       13 | 12069 | `			if( pV ){` |
|       13 | 12070 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       13 | 12071 | `				if( pClass ){` |
|        - | 12072 | `					ph7_class_method *pMethod;` |
|        - | 12073 | `					/* Extract the target method */` |
|       10 | 12074 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 12075 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 12076 | `						/* Perform the lookup */` |
|       10 | 12077 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 12078 | `						if( pMethod ){` |
|        - | 12079 | `							/* Method is callable */` |
|        5 | 12080 | `							res = 1;` |
|        2 | 12081 | `						}` |
|        4 | 12082 | `					}` |
|        4 | 12083 | `				}` |
|        5 | 12084 | `			}` |
|        9 | 12085 | `		}` |
|    24316 | 12086 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 12087 | `		const char *zName;` |
|        - | 12088 | `		int nLen;` |
|        - | 12089 | `		/* Extract the name */` |
|     5965 | 12090 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 12091 | `		/* Perform the lookup */` |
|     5980 | 12092 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 12093 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12094 | `				/* Function is callable */` |
|     5947 | 12095 | `				res = 1;` |
|     2971 | 12096 | `		}` |
|     2980 | 12097 | `	}` |
|    24347 | 12098 | `	return res;` |
|        5 | 12099 |  |
|        - | 12100 | `/*` |
|        - | 12101 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 12102 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 12103 | ` * Parameters` |
|        - | 12104 | ` * $name` |
|        - | 12105 | ` *    The callback function to check` |
|        - | 12106 | ` * $syntax_only` |
|        - | 12107 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 12108 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 12109 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 12110 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 12111 | ` *    a string.` |
|        - | 12112 | ` * Return` |
|        - | 12113 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 12114 | ` */` |
|       20 | 12115 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12116 |  |
|        - | 12117 | `	ph7_vm *pVm;` |
|        - | 12118 | `	int res;` |
|       21 | 12119 | `	if( nArg < 1 ){` |
|        - | 12120 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 12121 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12122 | `		return SXRET_OK;` |
|        - | 12123 | `	}` |
|        - | 12124 | `	/* Point to the target VM */` |
|       21 | 12125 | `	pVm = pCtx->pVm;` |
|        - | 12126 | `	/* Perform the requested operation */` |
|       21 | 12127 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 12128 | `	ph7_result_bool(pCtx,res);` |
|       21 | 12129 | `	return SXRET_OK;` |
|       11 | 12130 |  |
|        - | 12131 | `/*` |
|        - | 12132 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 12133 | ` * defined below.` |
|        - | 12134 | ` */` |
|     1312 | 12135 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12136 |  |
|     1313 | 12137 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12138 | `	ph7_value sName;` |
|        - | 12139 | `	sxi32 rc;` |
|        - | 12140 | `	/* Prepare the function name for insertion */` |
|     1313 | 12141 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1313 | 12142 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12143 | `	/* Perform the insertion */` |
|     1313 | 12144 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1313 | 12145 | `	PH7_MemObjRelease(&sName);` |
|     1313 | 12146 | `	return rc;` |
|        1 | 12147 |  |
|        - | 12148 | `/*` |
|        - | 12149 | ` * array get_defined_functions(void)` |
|        - | 12150 | ` *  Returns an array of all defined functions.` |
|        - | 12151 | ` * Parameter` |
|        - | 12152 | ` *  None.` |
|        - | 12153 | ` * Return` |
|        - | 12154 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 12155 | ` *  both built-in (internal) and user-defined.` |
|        - | 12156 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 12157 | ` *  defined ones using $arr["user"].` |
|        - | 12158 | ` * Note:` |
|        - | 12159 | ` *  NULL is returned on failure.` |
|        - | 12160 | ` */` |
|        2 | 12161 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12162 |  |
|        - | 12163 | `	ph7_value *pArray,*pEntry;` |
|        - | 12164 | `	/* NOTE:` |
|        - | 12165 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 12166 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 12167 | `	 */` |
|        3 | 12168 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12169 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12170 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12171 | `		SXUNUSED(apArg);` |
|        - | 12172 | `		/* Return NULL */` |
|      ! 0 | 12173 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12174 | `		return SXRET_OK;` |
|        - | 12175 | `	}` |
|        3 | 12176 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 12177 | `	if( pEntry == 0 ){` |
|        - | 12178 | `		/* Return NULL */` |
|      ! 0 | 12179 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12180 | `		return SXRET_OK;` |
|        - | 12181 | `	}` |
|        - | 12182 | `	/* Fill with the appropriate information */` |
|        3 | 12183 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 12184 | `	/* Create the 'internal' index */` |
|        3 | 12185 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 12186 | `	/* Create the user-func array */` |
|        3 | 12187 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 12188 | `	if( pEntry == 0 ){` |
|        - | 12189 | `		/* Return NULL */` |
|      ! 0 | 12190 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12191 | `		return SXRET_OK;` |
|        - | 12192 | `	}` |
|        - | 12193 | `	/* Fill with the appropriate information */` |
|        3 | 12194 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 12195 | `	/* Create the 'user' index */` |
|        3 | 12196 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 12197 | `	/* Return the multi-dimensional array */` |
|        3 | 12198 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12199 | `	return SXRET_OK;` |
|        2 | 12200 |  |
|        - | 12201 | `/*` |
|        - | 12202 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 12203 | ` *  Register a function for execution on shutdown.` |
|        - | 12204 | ` * Note` |
|        - | 12205 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 12206 | ` *  be called in the same order as they were registered.` |
|        - | 12207 | ` * Parameters` |
|        - | 12208 | ` *  $callback` |
|        - | 12209 | ` *   The shutdown callback to register.` |
|        - | 12210 | ` * $param` |
|        - | 12211 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 12212 | ` * Return` |
|        - | 12213 | ` *  Nothing.` |
|        - | 12214 | ` */` |
|       12 | 12215 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 12216 |  |
|        - | 12217 | `	VmShutdownCB sEntry;` |
|        - | 12218 | `	int i,j;` |
|       17 | 12219 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12220 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 12221 | `		return PH7_OK;` |
|        - | 12222 | `	}` |
|        - | 12223 | `	/* Zero the Entry */` |
|       17 | 12224 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 12225 | `	/* Initialize fields */` |
|       17 | 12226 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 12227 | `	/* Save the callback name for later invocation name */` |
|       17 | 12228 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      137 | 12229 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      125 | 12230 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       65 | 12231 | `	}` |
|        - | 12232 | `	/* Copy arguments */` |
|       17 | 12233 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 12234 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 12235 | `			/* Limit reached */` |
|      ! 0 | 12236 | `			break;` |
|        - | 12237 | `		}` |
|      ! 0 | 12238 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 12239 | `	}` |
|       17 | 12240 | `	sEntry.nArg = j;` |
|        - | 12241 | `	/* Install the callback */` |
|       17 | 12242 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       17 | 12243 | `	return PH7_OK;` |
|       11 | 12244 |  |
|        - | 12245 | `/*` |
|        - | 12246 | ` * Section:` |
|        - | 12247 | ` *  Class handling functions.` |
|        - | 12248 | ` * Status:` |
|        - | 12249 | ` *    Stable.` |
|        - | 12250 | ` */` |
|        - | 12251 | `/*` |
|        - | 12252 | ` * Extract the top active class. NULL is returned` |
|        - | 12253 | ` * if the class stack is empty.` |
|        - | 12254 | ` */` |
|     1054 | 12255 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        5 | 12256 |  |
|     1059 | 12257 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 12258 | `	ph7_class **apClass;` |
|     1059 | 12259 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 12260 | `		/* Empty stack,return NULL */` |
|       15 | 12261 | `		return 0;` |
|        - | 12262 | `	}` |
|        - | 12263 | `	/* Peek the last entry */` |
|     1045 | 12264 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|     1045 | 12265 | `	return apClass[pSet->nUsed - 1];` |
|      532 | 12266 |  |
|        - | 12267 | `/*` |
|        - | 12268 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 12269 | ` *   Get the class that declared the currently executing method.` |
|        - | 12270 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 12271 | ` *` |
|        - | 12272 | ` * Parameters` |
|        - | 12273 | ` *   pVm: Target VM` |
|        - | 12274 | ` *` |
|        - | 12275 | ` * Return` |
|        - | 12276 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 12277 | ` *   - Not executing within a class method` |
|        - | 12278 | ` *` |
|        - | 12279 | ` * Note` |
|        - | 12280 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 12281 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 12282 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 12283 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 12284 | ` *   declaring class.` |
|        - | 12285 | ` */` |
|       98 | 12286 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        5 | 12287 |  |
|      103 | 12288 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12289 | `	ph7_vm_func *pVmFunc;` |
|        - | 12290 |  |
|        - | 12291 | `	/* Skip exception frames to find the actual method frame */` |
|      103 | 12292 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 12293 |  |
|        - | 12294 | `	/* Check if we're in a method context */` |
|      103 | 12295 | `	if( pFrame->pParent ){` |
|       99 | 12296 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       99 | 12297 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 12298 | `			/* Return the declaring class */` |
|       99 | 12299 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 12300 | `		}` |
|      ! 0 | 12301 | `	}` |
|        - | 12302 |  |
|        5 | 12303 | `	return 0;` |
|       54 | 12304 |  |
|        - | 12305 |  |
|        - | 12306 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 12307 | `/*` |
|        - | 12308 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 12309 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 12310 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 12311 | ` * return value indicates failure.` |
|        - | 12312 | ` */` |
|        - | 12313 | `/*` |
|        - | 12314 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 12315 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 12316 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 12317 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 12318 | ` */` |
|     2880 | 12319 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 12320 | `	ph7_vm *pVm,` |
|        - | 12321 | `	ph7_class_instance *pThis,` |
|        - | 12322 | `	ph7_class_method *pMethod,` |
|        - | 12323 | `	ph7_value *pResult,` |
|        - | 12324 | `	int nArg,` |
|        - | 12325 | `	ph7_value **apArg,` |
|        - | 12326 | `	VmCallArgMap *pMap` |
|        - | 12327 | `	)` |
|        5 | 12328 |  |
|        - | 12329 | `	ph7_value *aStack;` |
|        - | 12330 | `	VmInstr aInstr[2];` |
|        - | 12331 | `	int iCursor;` |
|        - | 12332 | `	int i;` |
|        - | 12333 | `	sxi32 rc;` |
|     2885 | 12334 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2885 | 12335 | `	if( aStack == 0 ){` |
|      ! 0 | 12336 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12337 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 12338 | `		return SXERR_MEM;` |
|        - | 12339 | `	}` |
|     4493 | 12340 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1613 | 12341 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1613 | 12342 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      809 | 12343 | `	}` |
|     2885 | 12344 | `	iCursor = nArg + 1;` |
|     2885 | 12345 | `	if( pThis ){` |
|     2879 | 12346 | `		pThis->iRef++;` |
|     2879 | 12347 | `		aStack[i].x.pOther = pThis;` |
|     2879 | 12348 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1437 | 12349 | `	}` |
|     2885 | 12350 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2885 | 12351 | `	i++;` |
|     2885 | 12352 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2885 | 12353 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2885 | 12354 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2885 | 12355 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2885 | 12356 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2885 | 12357 | `	aInstr[0].iP1 = nArg;` |
|     2885 | 12358 | `	aInstr[0].iP2 = 0;` |
|     2885 | 12359 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2885 | 12360 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2885 | 12361 | `	aInstr[1].iP1 = 1;` |
|     2885 | 12362 | `	aInstr[1].iP2 = 0;` |
|     2885 | 12363 | `	aInstr[1].p3  = 0;` |
|     2885 | 12364 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE);` |
|     2885 | 12365 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12366 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 12367 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2885 | 12368 | `	return rc;` |
|     1445 | 12369 |  |
|     2268 | 12370 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 12371 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 12372 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 12373 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 12374 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 12375 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 12376 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 12377 | `	)` |
|        5 | 12378 |  |
|     2273 | 12379 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        5 | 12380 |  |
|        - | 12381 | `/*` |
|        - | 12382 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|        - | 12383 | ` * returning its result. Returns the exec status so a method that throws` |
|        - | 12384 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|        - | 12385 | ` * opcode, which discards it.` |
|        - | 12386 | ` */` |
|      324 | 12387 | `static sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|        1 | 12388 |  |
|      325 | 12389 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|      325 | 12390 | `	if( pMethod == 0 ){` |
|      ! 0 | 12391 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|        - | 12392 | `	}` |
|      325 | 12393 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|      163 | 12394 |  |
|        - | 12395 | `/*` |
|        - | 12396 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|        - | 12397 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|        - | 12398 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|        - | 12399 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|        - | 12400 | ` *` |
|        - | 12401 | ` * Returns:` |
|        - | 12402 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|        - | 12403 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|        - | 12404 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|        - | 12405 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|        - | 12406 | ` *` |
|        - | 12407 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|        - | 12408 | ` * returns); xStep must copy what it needs.` |
|        - | 12409 | ` */` |
|       28 | 12410 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|        1 | 12411 |  |
|        - | 12412 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|       29 | 12413 | `	ph7_class_instance *pAggregate = 0;` |
|        - | 12414 | `	ph7_class *pIteratorClass;` |
|       29 | 12415 | `	sxi32 rc = SXRET_OK;` |
|       29 | 12416 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|      ! 0 | 12417 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12418 | `	}` |
|       29 | 12419 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       29 | 12420 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       29 | 12421 | `	if( pIteratorClass == 0 ){` |
|      ! 0 | 12422 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12423 | `	}` |
|       29 | 12424 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|       27 | 12425 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|       14 | 12426 | `	}else{` |
|        - | 12427 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|        3 | 12428 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|        - | 12429 | `		ph7_value sInner;` |
|        3 | 12430 | `		int bOk = 0;` |
|        3 | 12431 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|      ! 0 | 12432 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|        - | 12433 | `		}` |
|        3 | 12434 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|        3 | 12435 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|        3 | 12436 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      ! 0 | 12437 | `			PH7_MemObjRelease(&sInner);` |
|      ! 0 | 12438 | `			return rc;` |
|        - | 12439 | `		}` |
|        3 | 12440 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|        3 | 12441 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|        3 | 12442 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|        3 | 12443 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|        3 | 12444 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|        3 | 12445 | `				bOk = 1;` |
|        1 | 12446 | `			}` |
|        1 | 12447 | `		}` |
|        3 | 12448 | `		PH7_MemObjRelease(&sInner);` |
|        3 | 12449 | `		if( !bOk ){` |
|        - | 12450 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|      ! 0 | 12451 | `			return SXERR_NOTIMPLEMENTED;` |
|        - | 12452 | `		}` |
|        - | 12453 | `	}` |
|        - | 12454 | `	/* Drive rewind / valid / current / key / step / next */` |
|       29 | 12455 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|       29 | 12456 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       78 | 12457 | `	for(;;){` |
|        - | 12458 | `		ph7_value sValid,sValue,sKey;` |
|        - | 12459 | `		int isValid;` |
|       93 | 12460 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|       93 | 12461 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|       96 | 12462 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|       93 | 12463 | `		PH7_MemObjToBool(&sValid);` |
|       93 | 12464 | `		isValid = (sValid.x.iVal != 0);` |
|       93 | 12465 | `		PH7_MemObjRelease(&sValid);` |
|       93 | 12466 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|       71 | 12467 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|       71 | 12468 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|       71 | 12469 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|       69 | 12470 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|       69 | 12471 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       69 | 12472 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|       69 | 12473 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|       69 | 12474 | `		PH7_MemObjRelease(&sValue);` |
|       69 | 12475 | `		PH7_MemObjRelease(&sKey);` |
|       69 | 12476 | `		if( rc != SXRET_OK ){` |
|        5 | 12477 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|        5 | 12478 | `			goto done;` |
|        - | 12479 | `		}` |
|       65 | 12480 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|       65 | 12481 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       12 | 12482 | `	}` |
|       14 | 12483 | `done:` |
|       29 | 12484 | `	PH7_ClassInstanceUnref(pThis);` |
|       29 | 12485 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|       29 | 12486 | `	return rc;` |
|       15 | 12487 |  |
|        - | 12488 | `/*` |
|        - | 12489 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 12490 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 12491 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 12492 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 12493 | ` *` |
|        - | 12494 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 12495 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 12496 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 12497 | ` *` |
|        - | 12498 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 12499 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 12500 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 12501 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 12502 | ` *` |
|        - | 12503 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 12504 | ` */` |
|      174 | 12505 | `static sxi32 VmCallObjectInvoke(` |
|        - | 12506 | `	ph7_vm *pVm,` |
|        - | 12507 | `	ph7_class_instance *pThis,` |
|        - | 12508 | `	int nArg,` |
|        - | 12509 | `	ph7_value **apArg,` |
|        - | 12510 | `	ph7_value *pResult,` |
|        - | 12511 | `	VmCallArgMap *pMap` |
|        - | 12512 | `	)` |
|        4 | 12513 |  |
|        - | 12514 | `	ph7_class_method *pMethod;` |
|      178 | 12515 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      178 | 12516 | `	if( pMethod == 0 ){` |
|       13 | 12517 | `		if( pResult ){` |
|       13 | 12518 | `			PH7_MemObjRelease(pResult);` |
|        6 | 12519 | `		}` |
|       13 | 12520 | `		return SXERR_INVALID;` |
|        - | 12521 | `	}` |
|      166 | 12522 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       91 | 12523 |  |
|        - | 12524 | `/*` |
|        - | 12525 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 12526 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 12527 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 12528 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 12529 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 12530 | ` * lookup or 'goto Exception').` |
|        - | 12531 | ` *` |
|        - | 12532 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 12533 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 12534 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 12535 | ` * reported.` |
|        - | 12536 | ` */` |
|       12 | 12537 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 12538 |  |
|        - | 12539 | `	ph7_class *pErrorClass;` |
|       13 | 12540 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 12541 | `	ph7_class_method *pCons;` |
|        - | 12542 | `	VmFrame *pThrowFrame;` |
|        - | 12543 | `	char zMsg[256];` |
|        - | 12544 | `	int nMsg;` |
|        - | 12545 | `	sxi32 rc;` |
|       25 | 12546 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 12547 | `		"Object of type %.*s is not callable",` |
|       12 | 12548 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 12549 | `		pThis->pClass->sName.zString);` |
|       13 | 12550 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 12551 | `	if( pErrorClass ){` |
|       13 | 12552 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 12553 | `	}` |
|       13 | 12554 | `	if( pErrInst == 0 ){` |
|        - | 12555 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 12556 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 12557 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 12558 | `		 * visible to the user. */` |
|      ! 0 | 12559 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 12560 | `		return SXERR_ABORT;` |
|        - | 12561 | `	}` |
|       13 | 12562 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 12563 | `	if( pCons ){` |
|        - | 12564 | `		ph7_value sArg;` |
|        - | 12565 | `		ph7_value *apMsg[1];` |
|        - | 12566 | `		SyString sMsgStr;` |
|       13 | 12567 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 12568 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 12569 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 12570 | `		apMsg[0] = &sArg;` |
|       13 | 12571 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 12572 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 12573 | `	}` |
|        - | 12574 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 12575 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 12576 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 12577 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 12578 | `	if( pThrowFrame ){` |
|       13 | 12579 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 12580 | `	}` |
|       13 | 12581 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 12582 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 12583 | `	return rc;` |
|        7 | 12584 |  |
|        - | 12585 | `/*` |
|        - | 12586 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 12587 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 12588 | ` * in the apArg[] array.` |
|        - | 12589 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12590 | ` * return value indicates failure.` |
|        - | 12591 | ` */` |
|     1238 | 12592 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 12593 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12594 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12595 | `	int nArg,          /* Total number of given arguments */` |
|        - | 12596 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 12597 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 12598 | `	)` |
|        5 | 12599 |  |
|        - | 12600 | `	ph7_value *aStack;` |
|        - | 12601 | `	VmInstr aInstr[2];` |
|        - | 12602 | `	int i;` |
|     1243 | 12603 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12604 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12605 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12606 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      141 | 12607 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12608 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12609 | `			nArg,apArg,pResult,0);` |
|        - | 12610 | `	}` |
|     1151 | 12611 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12612 | `		/* Don't bother processing,it's invalid anyway */` |
|      514 | 12613 | `		if( pResult ){` |
|        - | 12614 | `			/* Assume a null return value */` |
|      ! 0 | 12615 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12616 | `		}` |
|      514 | 12617 | `		return SXERR_INVALID;` |
|        - | 12618 | `	}` |
|      641 | 12619 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12620 | `		/* Class method */` |
|       15 | 12621 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12622 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12623 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12624 | `		ph7_class *pClass = 0;` |
|        - | 12625 | `		ph7_value *pValue;` |
|        - | 12626 | `		sxi32 rc;` |
|       15 | 12627 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12628 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12629 | `			if( pResult ){` |
|        - | 12630 | `				/* Assume a null return value */` |
|      ! 0 | 12631 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12632 | `			}` |
|      ! 0 | 12633 | `			return SXRET_OK;` |
|        - | 12634 | `		}` |
|        - | 12635 | `		/* Extract the class name or an instance of it */` |
|       15 | 12636 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12637 | `		if( pValue ){` |
|       15 | 12638 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12639 | `		}` |
|       15 | 12640 | `		if( pClass == 0 ){` |
|        - | 12641 | `			/* No such class,return NULL */` |
|      ! 0 | 12642 | `			if( pResult ){` |
|      ! 0 | 12643 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12644 | `			}` |
|      ! 0 | 12645 | `			return SXRET_OK;` |
|        - | 12646 | `		}` |
|       15 | 12647 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12648 | `			/* Point to the class instance */` |
|        9 | 12649 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12650 | `		}` |
|        - | 12651 | `		/* Try to extract the method */` |
|       15 | 12652 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12653 | `		if( pValue ){` |
|       15 | 12654 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12655 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12656 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12657 | `			}` |
|        7 | 12658 | `		}` |
|       15 | 12659 | `		if( pMethod == 0 ){` |
|        - | 12660 | `			/* No such method,return NULL */` |
|      ! 0 | 12661 | `			if( pResult ){` |
|      ! 0 | 12662 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12663 | `			}` |
|      ! 0 | 12664 | `			return SXRET_OK;` |
|        - | 12665 | `		}` |
|        - | 12666 | `		/* Call the class method */` |
|       15 | 12667 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12668 | `		return rc;` |
|        - | 12669 | `	}` |
|        - | 12670 | `	/* Create a new operand stack */` |
|      627 | 12671 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      627 | 12672 | `	if( aStack == 0 ){` |
|      ! 0 | 12673 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12674 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12675 | `		if( pResult ){` |
|        - | 12676 | `			/* Assume a null return value */` |
|      ! 0 | 12677 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12678 | `		}` |
|      ! 0 | 12679 | `		return SXERR_MEM;` |
|        - | 12680 | `	}` |
|        - | 12681 | `	/* Fill the operand stack with the given arguments */` |
|     1937 | 12682 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1315 | 12683 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12684 | `		/*` |
|        - | 12685 | `		 * Symisc eXtension:` |
|        - | 12686 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12687 | `		 */` |
|     1315 | 12688 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      660 | 12689 | `	}` |
|        - | 12690 | `	/* Push the function name */` |
|      627 | 12691 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      627 | 12692 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12693 | `	/* Emit the CALL istruction */` |
|      627 | 12694 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      627 | 12695 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      627 | 12696 | `	aInstr[0].iP2 = 0;` |
|      627 | 12697 | `	aInstr[0].p3  = 0;` |
|        - | 12698 | `	/* Emit the DONE instruction */` |
|      627 | 12699 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      627 | 12700 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      627 | 12701 | `	aInstr[1].iP2 = 0;` |
|      627 | 12702 | `	aInstr[1].p3  = 0;` |
|        - | 12703 | `	/* Execute the function body (if available) */` |
|        - | 12704 | `	{` |
|        - | 12705 | `		sxi32 rcExec;` |
|      627 | 12706 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE);` |
|        - | 12707 | `		/* Clean up the mess left behind */` |
|      627 | 12708 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12709 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      627 | 12710 | `		return rcExec;` |
|        - | 12711 | `	}` |
|      624 | 12712 |  |
|        - | 12713 | `/*` |
|        - | 12714 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12715 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12716 | ` * parameter.` |
|        - | 12717 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12718 | ` * return value indicates failure.` |
|        - | 12719 | ` */` |
|      240 | 12720 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12721 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12722 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12723 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12724 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12725 | `	)` |
|        1 | 12726 |  |
|        - | 12727 | `	ph7_value *pArg;` |
|        - | 12728 | `	SySet aArg;` |
|        - | 12729 | `	va_list ap;` |
|        - | 12730 | `	sxi32 rc;` |
|      241 | 12731 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12732 | `	/* Copy arguments one after one */` |
|      241 | 12733 | `	va_start(ap,pResult);` |
|      399 | 12734 | `	for(;;){` |
|      799 | 12735 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12736 | `		if( pArg == 0 ){` |
|      241 | 12737 | `			break;` |
|        - | 12738 | `		}` |
|      559 | 12739 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12740 | `	}` |
|        - | 12741 | `	/* Call the core routine */` |
|      241 | 12742 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12743 | `	/* Cleanup */` |
|      241 | 12744 | `	SySetRelease(&aArg);` |
|      241 | 12745 | `	return rc;` |
|        1 | 12746 |  |
|        - | 12747 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12748 | `/*` |
|        - | 12749 | ` * bool defined(string $name)` |
|        - | 12750 | ` *  Checks whether a given named constant exists.` |
|        - | 12751 | ` * Parameter:` |
|        - | 12752 | ` *  Name of the desired constant.` |
|        - | 12753 | ` * Return` |
|        - | 12754 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12755 | ` */` |
|       26 | 12756 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12757 |  |
|        - | 12758 | `	const char *zName;` |
|       28 | 12759 | `	int nLen = 0;` |
|       28 | 12760 | `	int res = 0;` |
|       28 | 12761 | `	if( nArg < 1 ){` |
|        - | 12762 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12763 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12764 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12765 | `		return SXRET_OK;` |
|        - | 12766 | `	}` |
|        - | 12767 | `	/* Extract constant name */` |
|       28 | 12768 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12769 | `	/* Perform the lookup */` |
|       28 | 12770 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12771 | `		/* Already defined */` |
|       26 | 12772 | `		res = 1;` |
|       12 | 12773 | `	}` |
|       28 | 12774 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12775 | `	return SXRET_OK;` |
|       15 | 12776 |  |
|        - | 12777 | `/*` |
|        - | 12778 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12779 | ` * below.` |
|        - | 12780 | ` */` |
|       16 | 12781 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 | 12782 |  |
|       19 | 12783 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12784 | `	/* Expand constant value */` |
|       19 | 12785 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       19 | 12786 |  |
|        - | 12787 | `/*` |
|        - | 12788 | ` * bool define(string $constant_name,expression value)` |
|        - | 12789 | ` *  Defines a named constant at runtime.` |
|        - | 12790 | ` * Parameter:` |
|        - | 12791 | ` *  $constant_name` |
|        - | 12792 | ` *   The name of the constant` |
|        - | 12793 | ` *  $value` |
|        - | 12794 | ` *   Constant value` |
|        - | 12795 | ` * Return:` |
|        - | 12796 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12797 | ` */` |
|       14 | 12798 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 12799 |  |
|        - | 12800 | `	const char *zName;  /* Constant name */` |
|        - | 12801 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       17 | 12802 | `	int nLen = 0;       /* Name length */` |
|        - | 12803 | `	sxi32 rc;` |
|       17 | 12804 | `	if( nArg < 2 ){` |
|        - | 12805 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12806 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12807 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12808 | `		return SXRET_OK;` |
|        - | 12809 | `	}` |
|       17 | 12810 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12811 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12812 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12813 | `		return SXRET_OK;` |
|        - | 12814 | `	}` |
|        - | 12815 | `	/* Extract constant name */` |
|       17 | 12816 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       17 | 12817 | `	if( nLen < 1 ){` |
|      ! 0 | 12818 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12819 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12820 | `		return SXRET_OK;` |
|        - | 12821 | `	}` |
|        - | 12822 | `	/* Duplicate constant value */` |
|       17 | 12823 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       17 | 12824 | `	if( pValue == 0 ){` |
|      ! 0 | 12825 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12826 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12827 | `		return SXRET_OK;` |
|        - | 12828 | `	}` |
|        - | 12829 | `	/* Initialize the memory object */` |
|       17 | 12830 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12831 | `	/* Register the constant */` |
|       17 | 12832 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       17 | 12833 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12834 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12835 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12836 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12837 | `		return SXRET_OK;` |
|        - | 12838 | `	}` |
|        - | 12839 | `	/* Duplicate constant value */` |
|       17 | 12840 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       17 | 12841 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12842 | `		/* Lower case the constant name */` |
|      ! 0 | 12843 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12844 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12845 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12846 | `				/* UTF-8 stream */` |
|      ! 0 | 12847 | `				zCur++;` |
|      ! 0 | 12848 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12849 | `					zCur++;` |
|      ! 0 | 12850 | `				}` |
|      ! 0 | 12851 | `				continue;` |
|        - | 12852 | `			}` |
|      ! 0 | 12853 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12854 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12855 | `				zCur[0] = (char)c;` |
|      ! 0 | 12856 | `			}` |
|      ! 0 | 12857 | `			zCur++;` |
|      ! 0 | 12858 | `		}` |
|        - | 12859 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12860 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12861 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12862 | `		{` |
|      ! 0 | 12863 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12864 | `			if( pAlias ){` |
|      ! 0 | 12865 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12866 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12867 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12868 | `			}` |
|        - | 12869 | `		}` |
|      ! 0 | 12870 | `	}` |
|        - | 12871 | `	/* All done,return TRUE */` |
|       17 | 12872 | `	ph7_result_bool(pCtx,1);` |
|       17 | 12873 | `	return SXRET_OK;` |
|       10 | 12874 |  |
|        - | 12875 | `/*` |
|        - | 12876 | ` * value constant(string $name)` |
|        - | 12877 | ` *  Returns the value of a constant` |
|        - | 12878 | ` * Parameter` |
|        - | 12879 | ` *  $name` |
|        - | 12880 | ` *    Name of the constant.` |
|        - | 12881 | ` * Return` |
|        - | 12882 | ` *  Constant value or NULL if not defined.` |
|        - | 12883 | ` */` |
|        8 | 12884 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 12885 |  |
|        - | 12886 | `	SyHashEntry *pEntry;` |
|        - | 12887 | `	ph7_constant *pCons;` |
|        - | 12888 | `	const char *zName; /* Constant name */` |
|        - | 12889 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12890 | `	int nLen;` |
|       11 | 12891 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12892 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12893 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12894 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12895 | `		return SXRET_OK;` |
|        - | 12896 | `	}` |
|        - | 12897 | `	/* Extract the constant name */` |
|       11 | 12898 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12899 | `	/* Perform the query */` |
|       11 | 12900 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       11 | 12901 | `	if( pEntry == 0 ){` |
|        3 | 12902 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12903 | `		ph7_result_null(pCtx);` |
|        3 | 12904 | `		return SXRET_OK;` |
|        - | 12905 | `	}` |
|        9 | 12906 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12907 | `	/* Point to the structure that describe the constant */` |
|        9 | 12908 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12909 | `	/* Extract constant value by calling it's associated callback */` |
|        9 | 12910 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12911 | `	/* Return that value */` |
|        9 | 12912 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12913 | `	/* Cleanup */` |
|        9 | 12914 | `	PH7_MemObjRelease(&sVal);` |
|        9 | 12915 | `	return SXRET_OK;` |
|        7 | 12916 |  |
|        - | 12917 | `/*` |
|        - | 12918 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12919 | ` * defined below.` |
|        - | 12920 | ` */` |
|      466 | 12921 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12922 |  |
|      467 | 12923 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12924 | `	ph7_value sName;` |
|        - | 12925 | `	sxi32 rc;` |
|        - | 12926 | `	/* Prepare the constant name for insertion */` |
|      467 | 12927 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12928 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12929 | `	/* Perform the insertion */` |
|      467 | 12930 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12931 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12932 | `	return rc;` |
|        1 | 12933 |  |
|        - | 12934 | `/*` |
|        - | 12935 | ` * array get_defined_constants(void)` |
|        - | 12936 | ` *  Returns an associative array with the names of all defined` |
|        - | 12937 | ` *  constants.` |
|        - | 12938 | ` * Parameters` |
|        - | 12939 | ` *  NONE.` |
|        - | 12940 | ` * Returns` |
|        - | 12941 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12942 | ` */` |
|        2 | 12943 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12944 |  |
|        - | 12945 | `	ph7_value *pArray;` |
|        - | 12946 | `	/* Create the array first*/` |
|        3 | 12947 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12948 | `	if( pArray == 0 ){` |
|      ! 0 | 12949 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12950 | `		SXUNUSED(apArg);` |
|        - | 12951 | `		/* Return NULL */` |
|      ! 0 | 12952 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12953 | `		return SXRET_OK;` |
|        - | 12954 | `	}` |
|        - | 12955 | `	/* Fill the array with the defined constants */` |
|        3 | 12956 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12957 | `	/* Return the created array */` |
|        3 | 12958 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12959 | `	return SXRET_OK;` |
|        2 | 12960 |  |
|        - | 12961 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12962 | `/*` |
|        - | 12963 | ` * Section:` |
|        - | 12964 | ` *  Random numbers/string generators.` |
|        - | 12965 | ` * Status:` |
|        - | 12966 | ` *    Stable.` |
|        - | 12967 | ` */` |
|        - | 12968 | `/*` |
|        - | 12969 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12970 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12971 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12972 | ` */` |
|     2925 | 12973 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 | 12974 |  |
|        - | 12975 | `	sxu32 iNum;` |
|     2930 | 12976 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2930 | 12977 | `	return iNum;` |
|        5 | 12978 |  |
|        - | 12979 | `/*` |
|        - | 12980 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12981 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12982 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12983 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12984 | ` */` |
|   238314 | 12985 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 | 12986 |  |
|        - | 12987 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12988 | `	int i;` |
|        - | 12989 | `	/* Generate a binary string first */` |
|   238319 | 12990 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12991 | `	/* Turn the binary string into english based alphabet */` |
|  2621627 | 12992 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2383313 | 12993 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1191659 | 12994 | `	 }` |
|   238319 | 12995 |  |
|        - | 12996 | `/*` |
|        - | 12997 | ` * int rand()` |
|        - | 12998 | ` * int mt_rand()` |
|        - | 12999 | ` * int rand(int $min,int $max)` |
|        - | 13000 | ` * int mt_rand(int $min,int $max)` |
|        - | 13001 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 13002 | ` * Parameter` |
|        - | 13003 | ` *  $min` |
|        - | 13004 | ` *    The lowest value to return (default: 0)` |
|        - | 13005 | ` *  $max` |
|        - | 13006 | ` *   The highest value to return (default: getrandmax())` |
|        - | 13007 | ` * Return` |
|        - | 13008 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 13009 | ` * Note:` |
|        - | 13010 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 13011 | ` *  by te SQLite3 library.` |
|        - | 13012 | ` */` |
|       20 | 13013 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13014 |  |
|        - | 13015 | `	sxu32 iNum;` |
|        - | 13016 | `	/* Generate the random number */` |
|       21 | 13017 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 13018 | `	if( nArg > 1 ){` |
|        - | 13019 | `		sxu32 iMin,iMax;` |
|        3 | 13020 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 13021 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 13022 | `		if( iMin < iMax ){` |
|        3 | 13023 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 13024 | `			if( iDiv > 0 ){` |
|        3 | 13025 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 13026 | `			}` |
|        1 | 13027 | `		}else if(iMax > 0 ){` |
|      ! 0 | 13028 | `			iNum %= iMax;` |
|      ! 0 | 13029 | `		}` |
|        1 | 13030 | `	}` |
|        - | 13031 | `	/* Return the number */` |
|       21 | 13032 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 13033 | `	return SXRET_OK;` |
|        1 | 13034 |  |
|        - | 13035 | `/*` |
|        - | 13036 | ` * int getrandmax(void)` |
|        - | 13037 | ` * int mt_getrandmax(void)` |
|        - | 13038 | ` * int rc4_getrandmax(void)` |
|        - | 13039 | ` *   Show largest possible random value` |
|        - | 13040 | ` * Return` |
|        - | 13041 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 13042 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 13043 | ` * Note:` |
|        - | 13044 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 13045 | ` *  by te SQLite3 library.` |
|        - | 13046 | ` */` |
|        4 | 13047 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13048 |  |
|        2 | 13049 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 13050 | `	SXUNUSED(apArg);` |
|        5 | 13051 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 13052 | `	return SXRET_OK;` |
|        1 | 13053 |  |
|        - | 13054 | `/*` |
|        - | 13055 | ` * string rand_str()` |
|        - | 13056 | ` * string rand_str(int $len)` |
|        - | 13057 | ` *  Generate a random string (English alphabet).` |
|        - | 13058 | ` * Parameter` |
|        - | 13059 | ` *  $len` |
|        - | 13060 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 13061 | ` * Return` |
|        - | 13062 | ` *   A pseudo random string.` |
|        - | 13063 | ` * Note:` |
|        - | 13064 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 13065 | ` *  by te SQLite3 library.` |
|        - | 13066 | ` *  This function is a symisc extension.` |
|        - | 13067 | ` */` |
|      120 | 13068 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13069 |  |
|        - | 13070 | `	char zString[1024];` |
|      122 | 13071 | `	int iLen = 0x10;` |
|      122 | 13072 | `	if( nArg > 0 ){` |
|        - | 13073 | `		/* Get the desired length */` |
|      122 | 13074 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 13075 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 13076 | `			/* Default length */` |
|        3 | 13077 | `			iLen = 0x10;` |
|        1 | 13078 | `		}` |
|       60 | 13079 | `	}` |
|        - | 13080 | `	/* Generate the random string */` |
|      122 | 13081 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 13082 | `	/* Return the generated string */` |
|      122 | 13083 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 13084 | `	return SXRET_OK;` |
|        2 | 13085 |  |
|        - | 13086 | `/*` |
|        - | 13087 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 13088 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 13089 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 13090 | ` */` |
|      488 | 13091 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 13092 |  |
|      488 | 13093 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 13094 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 13095 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13096 | `			"TypeError",` |
|        - | 13097 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 13098 | `			zFunc,iArgPos,zParamName,` |
|        3 | 13099 | `			ph7_type_name(pArg)` |
|        - | 13100 | `			);` |
|        - | 13101 | `	}` |
|      483 | 13102 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 13103 | `		int len;` |
|        9 | 13104 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 13105 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 13106 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13107 | `				"TypeError",` |
|        - | 13108 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 13109 | `				zFunc,iArgPos,zParamName` |
|        - | 13110 | `				);` |
|        - | 13111 | `		}` |
|        2 | 13112 | `	}` |
|      479 | 13113 | `	return SXRET_OK;` |
|      245 | 13114 |  |
|        - | 13115 | `/*` |
|        - | 13116 | ` * int random_int(int $min, int $max)` |
|        - | 13117 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 13118 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 13119 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 13120 | ` *  power-of-two mask covering the range.` |
|        - | 13121 | ` */` |
|      242 | 13122 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13123 |  |
|        - | 13124 | `	sxi64 iMin,iMax;` |
|        - | 13125 | `	sxu64 uRange,uMask,uResult;` |
|        - | 13126 | `	unsigned int nAttempt;` |
|        - | 13127 | `	int rc;` |
|      243 | 13128 | `	if( nArg != 2 ){` |
|       10 | 13129 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13130 | `			"ArgumentCountError",` |
|        - | 13131 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 13132 | `			nArg` |
|        - | 13133 | `			);` |
|        - | 13134 | `	}` |
|      237 | 13135 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 13136 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 13137 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 13138 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 13139 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 13140 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 13141 | `	if( iMin > iMax ){` |
|        3 | 13142 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13143 | `			"ValueError",` |
|        - | 13144 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 13145 | `			);` |
|        - | 13146 | `	}` |
|      229 | 13147 | `	if( iMin == iMax ){` |
|        5 | 13148 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 13149 | `		return SXRET_OK;` |
|        - | 13150 | `	}` |
|      225 | 13151 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 13152 | `	uMask = uRange;` |
|      225 | 13153 | `	uMask \|= uMask >> 1;` |
|      225 | 13154 | `	uMask \|= uMask >> 2;` |
|      225 | 13155 | `	uMask \|= uMask >> 4;` |
|      225 | 13156 | `	uMask \|= uMask >> 8;` |
|      225 | 13157 | `	uMask \|= uMask >> 16;` |
|      225 | 13158 | `	uMask \|= uMask >> 32;` |
|      225 | 13159 | `	uResult = 0;` |
|      339 | 13160 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 13161 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 13162 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 13163 | `		 * and the low-half mask would always read 0). */` |
|        - | 13164 | `		sxu64 uDraw;` |
|      339 | 13165 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 13166 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 13167 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 13168 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13169 | `				"Exception",` |
|        - | 13170 | `				"Cannot gather sufficient random data"` |
|        - | 13171 | `				);` |
|        - | 13172 | `		}` |
|      339 | 13173 | `		uDraw &= uMask;` |
|      339 | 13174 | `		if( uDraw <= uRange ){` |
|      225 | 13175 | `			uResult = uDraw;` |
|      225 | 13176 | `			break;` |
|        - | 13177 | `		}` |
|       50 | 13178 | `	}` |
|      225 | 13179 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 13180 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13181 | `			"Exception",` |
|        - | 13182 | `			"Cannot gather sufficient random data"` |
|        - | 13183 | `			);` |
|        - | 13184 | `	}` |
|      225 | 13185 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 13186 | `	return SXRET_OK;` |
|      122 | 13187 |  |
|        - | 13188 | `/*` |
|        - | 13189 | ` * string random_bytes(int $length)` |
|        - | 13190 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 13191 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 13192 | ` */` |
|       24 | 13193 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13194 |  |
|        - | 13195 | `	sxi64 iLen;` |
|        - | 13196 | `	unsigned char zStack[256];` |
|        - | 13197 | `	void *pBuf;` |
|        - | 13198 | `	int rc;` |
|       25 | 13199 | `	int bHeap = 0;` |
|       25 | 13200 | `	if( nArg != 1 ){` |
|        7 | 13201 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13202 | `			"ArgumentCountError",` |
|        - | 13203 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 13204 | `			nArg` |
|        - | 13205 | `			);` |
|        - | 13206 | `	}` |
|       21 | 13207 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 13208 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 13209 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 13210 | `	if( iLen < 1 ){` |
|        5 | 13211 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13212 | `			"ValueError",` |
|        - | 13213 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 13214 | `			);` |
|        - | 13215 | `	}` |
|        - | 13216 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 13217 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 13218 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 13219 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 13220 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13221 | `			"ValueError",` |
|        - | 13222 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 13223 | `			);` |
|        - | 13224 | `	}` |
|       13 | 13225 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 13226 | `		pBuf = zStack;` |
|        7 | 13227 | `	}else{` |
|      ! 0 | 13228 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 13229 | `		if( pBuf == 0 ){` |
|      ! 0 | 13230 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13231 | `				"Exception",` |
|        - | 13232 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 13233 | `				iLen` |
|        - | 13234 | `				);` |
|        - | 13235 | `		}` |
|      ! 0 | 13236 | `		bHeap = 1;` |
|        - | 13237 | `	}` |
|       13 | 13238 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 13239 | `		if( bHeap ){` |
|      ! 0 | 13240 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13241 | `		}` |
|      ! 0 | 13242 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13243 | `			"Exception",` |
|        - | 13244 | `			"Cannot gather sufficient random data"` |
|        - | 13245 | `			);` |
|        - | 13246 | `	}` |
|       13 | 13247 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 13248 | `	if( bHeap ){` |
|      ! 0 | 13249 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13250 | `	}` |
|       13 | 13251 | `	return SXRET_OK;` |
|       13 | 13252 |  |
|        - | 13253 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13254 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13255 | `/* Unique ID private data */` |
|        - | 13256 | `struct unique_id_data` |
|        - | 13257 |  |
|        - | 13258 | `	ph7_context *pCtx; /* Call context */` |
|        - | 13259 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 13260 | `};` |
|        - | 13261 | `/*` |
|        - | 13262 | ` * Binary to hex consumer callback.` |
|        - | 13263 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 13264 | ` * defined below.` |
|        - | 13265 | ` */` |
|      192 | 13266 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 13267 |  |
|      193 | 13268 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 13269 | `	sxu32 nBuflen;` |
|        - | 13270 | `	/* Extract result buffer length */` |
|      193 | 13271 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 13272 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 13273 | `			/*` |
|        - | 13274 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 13275 | `			 * string will be 13 characters long` |
|        - | 13276 | `			 */` |
|       25 | 13277 | `		return SXERR_ABORT;` |
|        - | 13278 | `	}` |
|      169 | 13279 | `	if( nBuflen > 22 ){` |
|      ! 0 | 13280 | `		return SXERR_ABORT;` |
|        - | 13281 | `	}` |
|        - | 13282 | `	/* Safely Consume the hex stream */` |
|      169 | 13283 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 13284 | `	return SXRET_OK;` |
|       97 | 13285 |  |
|        - | 13286 | `/*` |
|        - | 13287 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 13288 | ` *  Generate a unique ID` |
|        - | 13289 | ` * Parameter` |
|        - | 13290 | ` * $prefix` |
|        - | 13291 | ` *  Append this prefix to the generated unique ID.` |
|        - | 13292 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 13293 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 13294 | ` * $more_entropy` |
|        - | 13295 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 13296 | ` *  that the result will be unique.` |
|        - | 13297 | ` * Return` |
|        - | 13298 | ` *  Returns the unique identifier, as a string.` |
|        - | 13299 | ` */` |
|       24 | 13300 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13301 |  |
|        - | 13302 | `	struct unique_id_data sUniq;` |
|        - | 13303 | `	unsigned char zDigest[20];` |
|       25 | 13304 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13305 | `	const char *zPrefix;` |
|        - | 13306 | `	SHA1Context sCtx;` |
|        - | 13307 | `	char zRandom[7];` |
|        - | 13308 | `	int nPrefix;` |
|        - | 13309 | `	int entropy;` |
|        - | 13310 | `	/* Generate a random string first */` |
|       25 | 13311 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 13312 | `	/* Initialize fields */` |
|       25 | 13313 | `	zPrefix = 0;` |
|       25 | 13314 | `	nPrefix = 0;` |
|       25 | 13315 | `	entropy = 0;` |
|       25 | 13316 | `	if( nArg > 0 ){` |
|        - | 13317 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 13318 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 13319 | `		if( nArg > 1 ){` |
|      ! 0 | 13320 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13321 | `		}` |
|      ! 0 | 13322 | `	}` |
|       25 | 13323 | `	SHA1Init(&sCtx);` |
|        - | 13324 | `	/* Generate the random ID */` |
|       25 | 13325 | `	if( nPrefix > 0 ){` |
|      ! 0 | 13326 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 13327 | `	}` |
|        - | 13328 | `	/* Append the random ID */` |
|       25 | 13329 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 13330 | `	/* Append the random string */` |
|       25 | 13331 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 13332 | `	/* Increment the number */` |
|       25 | 13333 | `	pVm->unique_id++;` |
|       25 | 13334 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 13335 | `	/* Hexify the digest */` |
|       25 | 13336 | `	sUniq.pCtx = pCtx;` |
|       25 | 13337 | `	sUniq.entropy = entropy;` |
|       25 | 13338 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 13339 | `	/* All done */` |
|       25 | 13340 | `	return PH7_OK;` |
|        1 | 13341 |  |
|        - | 13342 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13343 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13344 | `/*` |
|        - | 13345 | ` * Section:` |
|        - | 13346 | ` *  Language construct implementation as foreign functions.` |
|        - | 13347 | ` * Status:` |
|        - | 13348 | ` *    Stable.` |
|        - | 13349 | ` */` |
|        - | 13350 | `/*` |
|        - | 13351 | ` * void echo($string...)` |
|        - | 13352 | ` *  Output one or more messages.` |
|        - | 13353 | ` * Parameters` |
|        - | 13354 | ` *  $string` |
|        - | 13355 | ` *   Message to output.` |
|        - | 13356 | ` * Return` |
|        - | 13357 | ` *  NULL.` |
|        - | 13358 | ` */` |
|      ! 0 | 13359 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13360 |  |
|        - | 13361 | `	const char *zData;` |
|      ! 0 | 13362 | `	int nDataLen = 0;` |
|        - | 13363 | `	ph7_vm *pVm;` |
|        - | 13364 | `	int i,rc;` |
|        - | 13365 | `	/* Point to the target VM */` |
|      ! 0 | 13366 | `	pVm = pCtx->pVm;` |
|        - | 13367 | `	/* Output */` |
|      ! 0 | 13368 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 13369 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 13370 | `		if( nDataLen > 0 ){` |
|      ! 0 | 13371 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 13372 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 13373 | `			if( rc == SXERR_ABORT ){` |
|        - | 13374 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13375 | `				return PH7_ABORT;` |
|        - | 13376 | `			}` |
|      ! 0 | 13377 | `		}` |
|      ! 0 | 13378 | `	}` |
|      ! 0 | 13379 | `	return SXRET_OK;` |
|      ! 0 | 13380 |  |
|        - | 13381 | `/*` |
|        - | 13382 | ` * int print($string...)` |
|        - | 13383 | ` *  Output one or more messages.` |
|        - | 13384 | ` * Parameters` |
|        - | 13385 | ` *  $string` |
|        - | 13386 | ` *   Message to output.` |
|        - | 13387 | ` * Return` |
|        - | 13388 | ` *  1 always.` |
|        - | 13389 | ` */` |
|        2 | 13390 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13391 |  |
|        - | 13392 | `	const char *zData;` |
|        3 | 13393 | `	int nDataLen = 0;` |
|        - | 13394 | `	ph7_vm *pVm;` |
|        - | 13395 | `	int i,rc;` |
|        - | 13396 | `	/* Point to the target VM */` |
|        3 | 13397 | `	pVm = pCtx->pVm;` |
|        - | 13398 | `	/* Output */` |
|        5 | 13399 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 13400 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 13401 | `		if( nDataLen > 0 ){` |
|        3 | 13402 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 13403 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 13404 | `			if( rc == SXERR_ABORT ){` |
|        - | 13405 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13406 | `				return PH7_ABORT;` |
|        - | 13407 | `			}` |
|        1 | 13408 | `		}` |
|        2 | 13409 | `	}` |
|        - | 13410 | `	/* Return 1 */` |
|        3 | 13411 | `	ph7_result_int(pCtx,1);` |
|        3 | 13412 | `	return SXRET_OK;` |
|        2 | 13413 |  |
|        - | 13414 | `/*` |
|        - | 13415 | ` * void exit(string $msg)` |
|        - | 13416 | ` * void exit(int $status)` |
|        - | 13417 | ` * void die(string $ms)` |
|        - | 13418 | ` * void die(int $status)` |
|        - | 13419 | ` *   Output a message and terminate program execution.` |
|        - | 13420 | ` * Parameter` |
|        - | 13421 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 13422 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 13423 | ` *  and not printed` |
|        - | 13424 | ` * Return` |
|        - | 13425 | ` *  NULL` |
|        - | 13426 | ` */` |
|      ! 0 | 13427 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13428 |  |
|      ! 0 | 13429 | `	if( nArg > 0 ){` |
|      ! 0 | 13430 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 13431 | `			const char *zData;` |
|      ! 0 | 13432 | `			int iLen = 0;` |
|        - | 13433 | `			/* Print exit message */` |
|      ! 0 | 13434 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 13435 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 13436 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 13437 | `			sxi32 iExitStatus;` |
|        - | 13438 | `			/* Record exit status code */` |
|      ! 0 | 13439 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 13440 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 13441 | `		}` |
|      ! 0 | 13442 | `	}` |
|        - | 13443 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 13444 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 13445 | `	 */` |
|      ! 0 | 13446 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 13447 | `	return PH7_ABORT;` |
|      ! 0 | 13448 |  |
|        - | 13449 | `/*` |
|        - | 13450 | ` * bool isset($var,...)` |
|        - | 13451 | ` *  Finds out whether a variable is set.` |
|        - | 13452 | ` * Parameters` |
|        - | 13453 | ` *  One or more variable to check.` |
|        - | 13454 | ` * Return` |
|        - | 13455 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 13456 | ` */` |
|    94356 | 13457 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13458 |  |
|        - | 13459 | `	ph7_value *pObj;` |
|    94361 | 13460 | `	int res = 0;` |
|        - | 13461 | `	int i;` |
|    94361 | 13462 | `	if( nArg < 1 ){` |
|        - | 13463 | `		/* Missing arguments,return false */` |
|      ! 0 | 13464 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 13465 | `		return SXRET_OK;` |
|        - | 13466 | `	}` |
|        - | 13467 | `	/* Iterate over available arguments */` |
|   123279 | 13468 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    94371 | 13469 | `		pObj = apArg[i];` |
|    94371 | 13470 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 13471 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 13472 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 13473 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    64403 | 13474 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 13475 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 13476 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 13477 | `			}` |
|    32199 | 13478 | `		}` |
|    94371 | 13479 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    94371 | 13480 | `		if( !res ){` |
|        - | 13481 | `			/* Variable not set,return FALSE */` |
|    65453 | 13482 | `			ph7_result_bool(pCtx,0);` |
|    65453 | 13483 | `			return SXRET_OK;` |
|        - | 13484 | `		}` |
|    14464 | 13485 | `	}` |
|        - | 13486 | `	/* All given variable are set,return TRUE */` |
|    28913 | 13487 | `	ph7_result_bool(pCtx,1);` |
|    28913 | 13488 | `	return SXRET_OK;` |
|    47183 | 13489 |  |
|        - | 13490 | `/*` |
|        - | 13491 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 13492 | ` * frame,the reference table and discard it's contents.` |
|        - | 13493 | ` * This function never fail and always return SXRET_OK.` |
|        - | 13494 | ` */` |
|  3176080 | 13495 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 | 13496 |  |
|        - | 13497 | `	ph7_value *pObj;` |
|        - | 13498 | `	VmRefObj *pRef;` |
|  3176085 | 13499 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3176085 | 13500 | `	if( pObj ){` |
|        - | 13501 | `		/* Release the object */` |
|  3176085 | 13502 | `		PH7_MemObjRelease(pObj);` |
|  1588040 | 13503 | `	}` |
|        - | 13504 | `	/* Remove old reference links */` |
|  3176085 | 13505 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3176085 | 13506 | `	if( pRef ){` |
|  3176079 | 13507 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 13508 | `		/* Unlink from the reference table */` |
|  3176079 | 13509 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3176079 | 13510 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 13511 | `			VmSlot sFree;` |
|        - | 13512 | `			/* Restore to the free list */` |
|  3176071 | 13513 | `			sFree.nIdx = nObjIdx;` |
|  3176071 | 13514 | `			sFree.pUserData = 0;` |
|  3176071 | 13515 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1588033 | 13516 | `		}` |
|  1588037 | 13517 | `	}` |
|  3176085 | 13518 | `	return SXRET_OK;` |
|        5 | 13519 |  |
|        - | 13520 | `/*` |
|        - | 13521 | ` * void unset($var,...)` |
|        - | 13522 | ` *   Unset one or more given variable.` |
|        - | 13523 | ` * Parameters` |
|        - | 13524 | ` *  One or more variable to unset.` |
|        - | 13525 | ` * Return` |
|        - | 13526 | ` *  Nothing.` |
|        - | 13527 | ` */` |
|     7588 | 13528 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13529 |  |
|        - | 13530 | `	ph7_value *pObj;` |
|        - | 13531 | `	ph7_vm *pVm;` |
|        - | 13532 | `	int i;` |
|        - | 13533 | `	/* Point to the target VM */` |
|     7593 | 13534 | `	pVm = pCtx->pVm;` |
|        - | 13535 | `	/* Iterate and unset */` |
|    15181 | 13536 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7593 | 13537 | `		pObj = apArg[i];` |
|     7593 | 13538 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      841 | 13539 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 13540 | `				/* Throw an error */` |
|      ! 0 | 13541 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 13542 | `			}` |
|      423 | 13543 | `		}else{` |
|     6757 | 13544 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 13545 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6757 | 13546 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6751 | 13547 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3373 | 13548 | `			}` |
|        - | 13549 | `		}` |
|     3799 | 13550 | `	}` |
|     7593 | 13551 | `	return SXRET_OK;` |
|        5 | 13552 |  |
|        - | 13553 | `/*` |
|        - | 13554 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 13555 | ` */` |
|      120 | 13556 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 13557 |  |
|      121 | 13558 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      121 | 13559 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13560 | `	ph7_value *pObj;` |
|        - | 13561 | `	sxu32 nIdx;` |
|        - | 13562 | `	/* Extract the memory object */` |
|      121 | 13563 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      121 | 13564 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      121 | 13565 | `	if( pObj ){` |
|      121 | 13566 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      119 | 13567 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 13568 | `				SyString sName;` |
|        - | 13569 | `				ph7_value sKey;` |
|        - | 13570 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 13571 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 13572 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      119 | 13573 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      119 | 13574 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      119 | 13575 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      119 | 13576 | `				PH7_MemObjRelease(&sKey);` |
|       59 | 13577 | `			}` |
|       59 | 13578 | `		}` |
|       60 | 13579 | `	}` |
|      121 | 13580 | `	return SXRET_OK;` |
|        1 | 13581 |  |
|        - | 13582 | `/*` |
|        - | 13583 | ` * array get_defined_vars(void)` |
|        - | 13584 | ` *  Returns an array of all defined variables.` |
|        - | 13585 | ` * Parameter` |
|        - | 13586 | ` *  None` |
|        - | 13587 | ` * Return` |
|        - | 13588 | ` *  An array with all the variables defined in the current scope.` |
|        - | 13589 | ` */` |
|        2 | 13590 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13591 |  |
|        3 | 13592 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13593 | `	ph7_value *pArray;` |
|        - | 13594 | `	/* Create a new array */` |
|        3 | 13595 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13596 | ` 	if( pArray == 0 ){` |
|      ! 0 | 13597 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13598 | `		SXUNUSED(apArg);` |
|        - | 13599 | `		/* Return NULL */` |
|      ! 0 | 13600 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13601 | `		return SXRET_OK;` |
|        - | 13602 | `	}` |
|        - | 13603 | `	/* Superglobals first */` |
|        3 | 13604 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13605 | `	/* Then variable defined in the current frame */` |
|        3 | 13606 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13607 | `	/* Finally,return the created array */` |
|        3 | 13608 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13609 | `	return SXRET_OK;` |
|        2 | 13610 |  |
|        - | 13611 | `/*` |
|        - | 13612 | ` * bool gettype($var)` |
|        - | 13613 | ` *  Get the type of a variable` |
|        - | 13614 | ` * Parameters` |
|        - | 13615 | ` *   $var` |
|        - | 13616 | ` *    The variable being type checked.` |
|        - | 13617 | ` * Return` |
|        - | 13618 | ` *   String representation of the given variable type.` |
|        - | 13619 | ` */` |
|       34 | 13620 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 13621 |  |
|       37 | 13622 | `	const char *zType = "Empty";` |
|       37 | 13623 | `	if( nArg > 0 ){` |
|       37 | 13624 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       17 | 13625 | `	}` |
|        - | 13626 | `	/* Return the variable type */` |
|       37 | 13627 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       37 | 13628 | `	return SXRET_OK;` |
|        3 | 13629 |  |
|        - | 13630 | `/*` |
|        - | 13631 | ` * string get_resource_type(resource $handle)` |
|        - | 13632 | ` *  This function gets the type of the given resource.` |
|        - | 13633 | ` * Parameters` |
|        - | 13634 | ` *  $handle` |
|        - | 13635 | ` *  The evaluated resource handle.` |
|        - | 13636 | ` * Return` |
|        - | 13637 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13638 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13639 | ` *  the return value will be the string Unknown.` |
|        - | 13640 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13641 | ` *  is not a resource.` |
|        - | 13642 | ` */` |
|        2 | 13643 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13644 |  |
|        3 | 13645 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13646 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13647 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13648 | `		return PH7_OK;` |
|        - | 13649 | `	}` |
|        3 | 13650 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13651 | `	return SXRET_OK;` |
|        2 | 13652 |  |
|        - | 13653 | `/*` |
|        - | 13654 | ` * void var_dump(expression,....)` |
|        - | 13655 | ` *   var_dump � Dumps information about a variable` |
|        - | 13656 | ` * Parameters` |
|        - | 13657 | ` *   One or more expression to dump.` |
|        - | 13658 | ` * Returns` |
|        - | 13659 | ` *  Nothing.` |
|        - | 13660 | ` */` |
|      218 | 13661 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 13662 |  |
|        - | 13663 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13664 | `	int i;` |
|      221 | 13665 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13666 | `	/* Dump one or more expressions */` |
|      445 | 13667 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      227 | 13668 | `		ph7_value *pObj = apArg[i];` |
|        - | 13669 | `		/* Reset the working buffer */` |
|      227 | 13670 | `		SyBlobReset(&sDump);` |
|        - | 13671 | `		/* Dump the given expression */` |
|      227 | 13672 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13673 | `		/* Output */` |
|      227 | 13674 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      227 | 13675 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13676 | `		}` |
|      115 | 13677 | `	}` |
|        - | 13678 | `	/* Release the working buffer */` |
|      221 | 13679 | `	SyBlobRelease(&sDump);` |
|      221 | 13680 | `	return SXRET_OK;` |
|        3 | 13681 |  |
|        - | 13682 | `/*` |
|        - | 13683 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13684 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13685 | ` * Parameters` |
|        - | 13686 | ` *   expression: Expression to dump` |
|        - | 13687 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13688 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13689 | ` *            print_r() will return the information rather than print it.` |
|        - | 13690 | ` * Return` |
|        - | 13691 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13692 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13693 | ` */` |
|       16 | 13694 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13695 |  |
|       17 | 13696 | `	int ret_string = 0;` |
|        - | 13697 | `	SyBlob sDump;` |
|       17 | 13698 | `	if( nArg < 1 ){` |
|        - | 13699 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13700 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13701 | `		return SXRET_OK;` |
|        - | 13702 | `	}` |
|       17 | 13703 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13704 | `	if ( nArg > 1 ){` |
|        - | 13705 | `		/* Where to redirect output */` |
|       11 | 13706 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13707 | `	}` |
|        - | 13708 | `	/* Generate dump */` |
|       17 | 13709 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13710 | `	if( !ret_string ){` |
|        - | 13711 | `		/* Output dump */` |
|        7 | 13712 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13713 | `		/* Return true */` |
|        7 | 13714 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13715 | `	}else{` |
|        - | 13716 | `		/* Generated dump as return value */` |
|       11 | 13717 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13718 | `	}` |
|        - | 13719 | `	/* Release the working buffer */` |
|       17 | 13720 | `	SyBlobRelease(&sDump);` |
|       17 | 13721 | `	return SXRET_OK;` |
|        9 | 13722 |  |
|        - | 13723 | `/*` |
|        - | 13724 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13725 | ` * Same job as print_r. (see coment above)` |
|        - | 13726 | ` */` |
|        2 | 13727 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13728 |  |
|        3 | 13729 | `	int ret_string = 0;` |
|        - | 13730 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13731 | `	if( nArg < 1 ){` |
|        - | 13732 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13733 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13734 | `		return SXRET_OK;` |
|        - | 13735 | `	}` |
|        3 | 13736 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13737 | `	if ( nArg > 1 ){` |
|        - | 13738 | `		/* Where to redirect output */` |
|        3 | 13739 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13740 | `	}` |
|        - | 13741 | `	/* Generate dump */` |
|        3 | 13742 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13743 | `	if( !ret_string ){` |
|        - | 13744 | `		/* Output dump */` |
|      ! 0 | 13745 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13746 | `		/* Return NULL */` |
|      ! 0 | 13747 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13748 | `	}else{` |
|        - | 13749 | `		/* Generated dump as return value */` |
|        3 | 13750 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13751 | `	}` |
|        - | 13752 | `	/* Release the working buffer */` |
|        3 | 13753 | `	SyBlobRelease(&sDump);` |
|        3 | 13754 | `	return SXRET_OK;` |
|        2 | 13755 |  |
|        - | 13756 | `/*` |
|        - | 13757 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13758 | ` *  Set/get the various assert flags.` |
|        - | 13759 | ` * Parameter` |
|        - | 13760 | ` * $what` |
|        - | 13761 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13762 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13763 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13764 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13765 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13766 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13767 | ` * $value` |
|        - | 13768 | ` *   An optional new value for the option.` |
|        - | 13769 | ` * Return` |
|        - | 13770 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13771 | ` */` |
|       28 | 13772 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13773 |  |
|       33 | 13774 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13775 | `	int iOption;` |
|        - | 13776 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       33 | 13777 | `	if( nArg < 1 ){` |
|        3 | 13778 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13779 | `			"ArgumentCountError",` |
|        - | 13780 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13781 | `			);` |
|        - | 13782 | `	}` |
|        - | 13783 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13784 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       31 | 13785 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13786 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13787 | `			"TypeError",` |
|        - | 13788 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13789 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13790 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13791 | `			);` |
|        - | 13792 | `	}` |
|       31 | 13793 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13794 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13795 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13796 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       31 | 13797 | `	switch( iOption ){` |
|        5 | 13798 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13799 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13800 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13801 | `		if( nArg > 1 ){` |
|        5 | 13802 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13803 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13804 | `			}else{` |
|        3 | 13805 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13806 | `			}` |
|        2 | 13807 | `		}` |
|       12 | 13808 | `		break;` |
|        1 | 13809 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13810 | `		/* Return old callback or null */` |
|        3 | 13811 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13812 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13813 | `		}else{` |
|        3 | 13814 | `			ph7_result_null(pCtx);` |
|        - | 13815 | `		}` |
|        3 | 13816 | `		if( nArg > 1 ){` |
|      ! 0 | 13817 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13818 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13819 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13820 | `			}else{` |
|      ! 0 | 13821 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13822 | `			}` |
|      ! 0 | 13823 | `		}` |
|        3 | 13824 | `		break;` |
|        5 | 13825 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13826 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13827 | `		if( nArg > 1 ){` |
|        5 | 13828 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13829 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13830 | `			}else{` |
|        3 | 13831 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13832 | `			}` |
|        2 | 13833 | `		}` |
|       11 | 13834 | `		break;` |
|      ! 0 | 13835 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13836 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13837 | `		break;` |
|        1 | 13838 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13839 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13840 | `		break;` |
|      ! 0 | 13841 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13842 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13843 | `		break;` |
|        1 | 13844 | `	default:` |
|        - | 13845 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13846 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13847 | `			"ValueError",` |
|        - | 13848 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13849 | `			);` |
|        - | 13850 | `	}` |
|       29 | 13851 | `	return PH7_OK;` |
|       19 | 13852 |  |
|        - | 13853 | `/*` |
|        - | 13854 | ` * bool assert(mixed $assertion)` |
|        - | 13855 | ` *  Checks if assertion is FALSE.` |
|        - | 13856 | ` * Parameter` |
|        - | 13857 | ` *  $assertion` |
|        - | 13858 | ` *    The assertion to test.` |
|        - | 13859 | ` * Return` |
|        - | 13860 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13861 | ` */` |
|       24 | 13862 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13863 |  |
|       29 | 13864 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13865 | `	int iFlags,iResult;` |
|        - | 13866 | `	const char *zDesc;` |
|        - | 13867 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       29 | 13868 | `	if( nArg < 1 ){` |
|        3 | 13869 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13870 | `			"ArgumentCountError",` |
|        - | 13871 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13872 | `			);` |
|        - | 13873 | `	}` |
|       27 | 13874 | `	iFlags = pVm->iAssertFlags;` |
|       27 | 13875 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13876 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13877 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13878 | `		return PH7_OK;` |
|        - | 13879 | `	}` |
|        - | 13880 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       27 | 13881 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       27 | 13882 | `	if( !iResult ){` |
|        - | 13883 | `		/* Assertion failed */` |
|        - | 13884 | `		/* Extract optional description */` |
|       16 | 13885 | `		zDesc = 0;` |
|       16 | 13886 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13887 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13888 | `		}` |
|       16 | 13889 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13890 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13891 | `			ph7_value sFile,sLine;` |
|        - | 13892 | `			ph7_value *apCbArg[3];` |
|        - | 13893 | `			SyString *pFile;` |
|        - | 13894 | `			/* Extract the processed script */` |
|      ! 0 | 13895 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13896 | `			if( pFile == 0 ){` |
|      ! 0 | 13897 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13898 | `			}` |
|        - | 13899 | `			/* Invoke the callback */` |
|      ! 0 | 13900 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13901 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13902 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13903 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13904 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13905 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13906 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13907 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13908 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13909 | `		}` |
|       16 | 13910 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13911 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13912 | `			return PH7_ABORT;` |
|        - | 13913 | `		}` |
|        - | 13914 | `		/* PHP 8: throw AssertionError by default */` |
|       16 | 13915 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13916 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13917 | `				"AssertionError",` |
|        - | 13918 | `				"%s",` |
|        1 | 13919 | `				zDesc` |
|        - | 13920 | `				);` |
|      ! 0 | 13921 | `		}else{` |
|       13 | 13922 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13923 | `				"AssertionError",` |
|        - | 13924 | `				"assert(false)"` |
|        - | 13925 | `				);` |
|        - | 13926 | `		}` |
|        - | 13927 | `	}` |
|        - | 13928 | `	/* Assertion passed */` |
|       11 | 13929 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13930 | `	return PH7_OK;` |
|       17 | 13931 |  |
|        - | 13932 | `/*` |
|        - | 13933 | ` * Section:` |
|        - | 13934 | ` *  Error reporting functions.` |
|        - | 13935 | ` * Status:` |
|        - | 13936 | ` *    Stable.` |
|        - | 13937 | ` */` |
|        - | 13938 | `/*` |
|        - | 13939 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13940 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13941 | ` * Parameters` |
|        - | 13942 | ` *  $error_msg` |
|        - | 13943 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13944 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13945 | ` * $error_type` |
|        - | 13946 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13947 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13948 | ` * Return` |
|        - | 13949 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13950 | ` */` |
|       12 | 13951 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13952 |  |
|       17 | 13953 | `	int nErr = PH7_CTX_NOTICE;` |
|       17 | 13954 | `	int rc = PH7_OK;` |
|       17 | 13955 | `	if( nArg > 0 ){` |
|        - | 13956 | `		const char *zErr;` |
|        - | 13957 | `		int nLen;` |
|        - | 13958 | `		/* Extract the error message */` |
|       14 | 13959 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 13960 | `		if( nArg > 1 ){` |
|        - | 13961 | `			/* Extract the error type */` |
|       14 | 13962 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       14 | 13963 | `			switch( nErr ){` |
|        1 | 13964 | `			case 1:   /* E_ERROR */` |
|        - | 13965 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13966 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13967 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13968 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13969 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13970 | `				break;` |
|        1 | 13971 | `			case 2:   /* E_WARNING */` |
|        - | 13972 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13973 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13974 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13975 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13976 | `				break;` |
|        3 | 13977 | `			default:` |
|        9 | 13978 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13979 | `				break;` |
|        - | 13980 | `			}` |
|        5 | 13981 | `		}` |
|        - | 13982 | `		/* Report error */` |
|       14 | 13983 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       14 | 13984 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13985 | `			return rc;` |
|        - | 13986 | `		}` |
|        - | 13987 | `		/* Return true */` |
|       14 | 13988 | `		ph7_result_bool(pCtx,1);` |
|        9 | 13989 | `	}else{` |
|        - | 13990 | `		/* Missing arguments,return FALSE */` |
|        3 | 13991 | `		ph7_result_bool(pCtx,0);` |
|        - | 13992 | `	}` |
|       17 | 13993 | `	return rc;` |
|       11 | 13994 |  |
|        - | 13995 | `/*` |
|        - | 13996 | ` * int error_reporting([int $level])` |
|        - | 13997 | ` *  Sets which PHP errors are reported.` |
|        - | 13998 | ` * Parameters` |
|        - | 13999 | ` *  $level` |
|        - | 14000 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 14001 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 14002 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 14003 | ` *   levels will not always behave as expected.` |
|        - | 14004 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 14005 | ` *   in the predefined constants.` |
|        - | 14006 | ` * Return` |
|        - | 14007 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 14008 | ` *   parameter is given.` |
|        - | 14009 | ` */` |
|       32 | 14010 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 14011 |  |
|       37 | 14012 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14013 | `	int nOld;` |
|        - | 14014 | `	/* Extract the old reporting level */` |
|       37 | 14015 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       37 | 14016 | `	if( nArg > 0 ){` |
|        - | 14017 | `		int nNew;` |
|        - | 14018 | `		/* Extract the desired error reporting level */` |
|       31 | 14019 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       31 | 14020 | `		if( !nNew ){` |
|        - | 14021 | `			/* Do not report errors at all */` |
|        5 | 14022 | `			pVm->bErrReport = 0;` |
|        3 | 14023 | `		}else{` |
|        - | 14024 | `			/* Report all errors */` |
|       27 | 14025 | `			pVm->bErrReport = 1;` |
|        - | 14026 | `		}` |
|       13 | 14027 | `	}` |
|        - | 14028 | `	/* Return the old level */` |
|       37 | 14029 | `	ph7_result_int(pCtx,nOld);` |
|       37 | 14030 | `	return PH7_OK;` |
|        5 | 14031 |  |
|        - | 14032 | `/*` |
|        - | 14033 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 14034 | ` *  Send an error message somewhere.` |
|        - | 14035 | ` * Parameter` |
|        - | 14036 | ` *  $message` |
|        - | 14037 | ` *   The error message that should be logged.` |
|        - | 14038 | ` *  $message_type` |
|        - | 14039 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 14040 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 14041 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 14042 | ` *       This is the default option.` |
|        - | 14043 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 14044 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 14045 | ` *    2  No longer an option.` |
|        - | 14046 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 14047 | ` *       to the end of the message string.` |
|        - | 14048 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 14049 | ` *  $destination` |
|        - | 14050 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 14051 | ` *  $extra_headers` |
|        - | 14052 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 14053 | ` * Return` |
|        - | 14054 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14055 | ` * NOTE:` |
|        - | 14056 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 14057 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 14058 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 14059 | ` *  Otherwise this function is no-op.` |
|        - | 14060 | ` */` |
|        4 | 14061 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14062 |  |
|        - | 14063 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 14064 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 14065 | `	int iType = 0;` |
|        5 | 14066 | `	if( nArg < 1 ){` |
|        - | 14067 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 14068 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14069 | `		return PH7_OK;` |
|        - | 14070 | `	}` |
|        5 | 14071 | `	if( pVm->xErrLog  ){` |
|        - | 14072 | `		/* Invoke the user callback */` |
|      ! 0 | 14073 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 14074 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 14075 | `		if( nArg > 1 ){` |
|      ! 0 | 14076 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 14077 | `			if( nArg > 2 ){` |
|      ! 0 | 14078 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 14079 | `				if( nArg > 3 ){` |
|      ! 0 | 14080 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 14081 | `				}` |
|      ! 0 | 14082 | `			}` |
|      ! 0 | 14083 | `		}` |
|      ! 0 | 14084 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 14085 | `	}` |
|        - | 14086 | `	/* Retun TRUE */` |
|        5 | 14087 | `	ph7_result_bool(pCtx,1);` |
|        5 | 14088 | `	return PH7_OK;` |
|        3 | 14089 |  |
|        - | 14090 | `/*` |
|        - | 14091 | ` * bool restore_exception_handler(void)` |
|        - | 14092 | ` *  Restores the previously defined exception handler function.` |
|        - | 14093 | ` * Parameter` |
|        - | 14094 | ` *  None` |
|        - | 14095 | ` * Return` |
|        - | 14096 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 14097 | ` */` |
|        4 | 14098 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14099 |  |
|        5 | 14100 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14101 | `	ph7_value *pOld,*pNew;` |
|        - | 14102 | `	/* Point to the old and the new handler */` |
|        5 | 14103 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 14104 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 14105 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 14106 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 14107 | `		SXUNUSED(apArg);` |
|        - | 14108 | `		/* No installed handler,return FALSE */` |
|        5 | 14109 | `		ph7_result_bool(pCtx,0);` |
|        5 | 14110 | `		return PH7_OK;` |
|        - | 14111 | `	}` |
|        - | 14112 | `	/* Copy the old handler */` |
|      ! 0 | 14113 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 14114 | `	PH7_MemObjRelease(pOld);` |
|        - | 14115 | `	/* Return TRUE */` |
|      ! 0 | 14116 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 14117 | `	return PH7_OK;` |
|        3 | 14118 |  |
|        - | 14119 | `/*` |
|        - | 14120 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 14121 | ` *  Sets a user-defined exception handler function.` |
|        - | 14122 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 14123 | ` * NOTE` |
|        - | 14124 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 14125 | ` *  the satndard PHP engine.` |
|        - | 14126 | ` * Parameters` |
|        - | 14127 | ` *  $exception_handler` |
|        - | 14128 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 14129 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 14130 | ` *   that was thrown.` |
|        - | 14131 | ` *  Note:` |
|        - | 14132 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 14133 | ` * Return` |
|        - | 14134 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 14135 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 14136 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 14137 | ` */` |
|        4 | 14138 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14139 |  |
|        6 | 14140 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14141 | `	ph7_value *pOld,*pNew;` |
|        - | 14142 | `	/* Point to the old and the new handler */` |
|        6 | 14143 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 14144 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 14145 | `	/* Return the old handler */` |
|        6 | 14146 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 14147 | `	if( nArg > 0 ){` |
|        6 | 14148 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14149 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 14150 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 14151 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14152 | `		}else{` |
|        6 | 14153 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14154 | `			/* Install the new handler */` |
|        6 | 14155 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14156 | `		}` |
|        2 | 14157 | `	}` |
|        6 | 14158 | `	return PH7_OK;` |
|        2 | 14159 |  |
|        - | 14160 | `/*` |
|        - | 14161 | ` * bool restore_error_handler(void)` |
|        - | 14162 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 14163 | ` * Parameters:` |
|        - | 14164 | ` *  None.` |
|        - | 14165 | ` * Return` |
|        - | 14166 | ` *  Always TRUE.` |
|        - | 14167 | ` */` |
|        6 | 14168 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14169 |  |
|        8 | 14170 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14171 | `	ph7_value *pOld,*pNew;` |
|        - | 14172 | `	/* Point to the old and the new handler */` |
|        8 | 14173 | `	pOld = &pVm->aErrCB[0];` |
|        8 | 14174 | `	pNew = &pVm->aErrCB[1];` |
|        8 | 14175 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 14176 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 14177 | `		SXUNUSED(apArg);` |
|        - | 14178 | `		/* No installed callback,return FALSE */` |
|        8 | 14179 | `		ph7_result_bool(pCtx,0);` |
|        8 | 14180 | `		return PH7_OK;` |
|        - | 14181 | `	}` |
|        - | 14182 | `	/* Copy the old callback */` |
|      ! 0 | 14183 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 14184 | `	PH7_MemObjRelease(pOld);` |
|        - | 14185 | `	/* Return TRUE */` |
|      ! 0 | 14186 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 14187 | `	return PH7_OK;` |
|        5 | 14188 |  |
|        - | 14189 | `/*` |
|        - | 14190 | ` * value set_error_handler(callable $error_handler)` |
|        - | 14191 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 14192 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 14193 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 14194 | ` *  Sets a user-defined error handler function.` |
|        - | 14195 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 14196 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 14197 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 14198 | ` *  conditions (using trigger_error()).` |
|        - | 14199 | ` * Parameters` |
|        - | 14200 | ` *  $error_handler` |
|        - | 14201 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 14202 | ` *   describing the error.` |
|        - | 14203 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 14204 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 14205 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 14206 | ` *   The function can be shown as:` |
|        - | 14207 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 14208 | ` *     errno` |
|        - | 14209 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 14210 | ` *   errstr` |
|        - | 14211 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 14212 | ` *   errfile` |
|        - | 14213 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 14214 | ` *     was raised in, as a string.` |
|        - | 14215 | ` *  Note:` |
|        - | 14216 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 14217 | ` * Return` |
|        - | 14218 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 14219 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 14220 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 14221 | ` */` |
|    11024 | 14222 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 14223 |  |
|    11027 | 14224 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14225 | `	ph7_value *pOld,*pNew;` |
|        - | 14226 | `	/* Point to the old and the new handler */` |
|    11027 | 14227 | `	pOld = &pVm->aErrCB[0];` |
|    11027 | 14228 | `	pNew = &pVm->aErrCB[1];` |
|        - | 14229 | `	/* Return the old handler */` |
|    11027 | 14230 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    11027 | 14231 | `	if( nArg > 0 ){` |
|    11027 | 14232 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14233 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5507 | 14234 | `			PH7_MemObjRelease(pNew);` |
|     5507 | 14235 | `			ph7_result_bool(pCtx,1);` |
|     2754 | 14236 | `		}else{` |
|     5521 | 14237 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14238 | `			/* Install the new handler */` |
|     5521 | 14239 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14240 | `		}` |
|     5512 | 14241 | `	}` |
|    11027 | 14242 | `	return PH7_OK;` |
|        3 | 14243 |  |
|        - | 14244 | `/*` |
|        - | 14245 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 14246 | ` *  Generates a backtrace.` |
|        - | 14247 | ` * Paramaeter` |
|        - | 14248 | ` *  $options` |
|        - | 14249 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 14250 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 14251 | ` *   all the function/method arguments, to save memory.` |
|        - | 14252 | ` * $limit` |
|        - | 14253 | ` *   (Not Used)` |
|        - | 14254 | ` * Return` |
|        - | 14255 | ` *  An array.The possible returned elements are as follows:` |
|        - | 14256 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 14257 | ` *          Name        Type      Description` |
|        - | 14258 | ` *          ------      ------     -----------` |
|        - | 14259 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 14260 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 14261 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 14262 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 14263 | ` *          object      object    The current object.` |
|        - | 14264 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 14265 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 14266 | ` */` |
|      996 | 14267 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 14268 |  |
|     1001 | 14269 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14270 | `	ph7_value *pArray;` |
|        - | 14271 | `	ph7_class *pClass;` |
|        - | 14272 | `	ph7_value *pValue;` |
|        - | 14273 | `	SyString *pFile;` |
|        - | 14274 | `	/* Create a new array */` |
|     1001 | 14275 | `	pArray = ph7_context_new_array(pCtx);` |
|     1001 | 14276 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     1001 | 14277 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14278 | `		/* Out of memory,return NULL */` |
|      ! 0 | 14279 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 14280 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14281 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14282 | `		SXUNUSED(apArg);` |
|      ! 0 | 14283 | `		return PH7_OK;` |
|        - | 14284 | `	}` |
|        - | 14285 | `	/* Dump running function name and it's arguments  */` |
|     1001 | 14286 | `	if( pVm->pFrame->pParent ){` |
|     1001 | 14287 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 14288 | `		ph7_vm_func *pFunc;` |
|        - | 14289 | `		ph7_value *pArg;` |
|     1001 | 14290 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     1001 | 14291 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     1001 | 14292 | `		if( pFrame->pParent && pFunc ){` |
|     1001 | 14293 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|     1001 | 14294 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|     1001 | 14295 | `			ph7_value_reset_string_cursor(pValue);` |
|      498 | 14296 | `		}` |
|        - | 14297 | `		/* Function arguments */` |
|     1001 | 14298 | `		pArg = ph7_context_new_array(pCtx);` |
|     1001 | 14299 | `		if( pArg  ){` |
|        - | 14300 | `			ph7_value *pObj;` |
|        - | 14301 | `			VmSlot *aSlot;` |
|        - | 14302 | `			sxu32 n;` |
|        - | 14303 | `			/* Start filling the array with the given arguments */` |
|     1001 | 14304 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3993 | 14305 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2997 | 14306 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2997 | 14307 | `				if( pObj ){` |
|     2997 | 14308 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1496 | 14309 | `				}` |
|     1501 | 14310 | `			}` |
|        - | 14311 | `			/* Save the array */` |
|     1001 | 14312 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      498 | 14313 | `		}` |
|      498 | 14314 | `	}` |
|     1001 | 14315 | `	ph7_value_int(pValue,1);` |
|        - | 14316 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 14317 | `	 * line numbers at run-time. )` |
|        - | 14318 | `	 */` |
|     1001 | 14319 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 14320 | `	/* Current processed script */` |
|     1001 | 14321 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1001 | 14322 | `	if( pFile ){` |
|     1001 | 14323 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|     1001 | 14324 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|     1001 | 14325 | `		ph7_value_reset_string_cursor(pValue);` |
|      498 | 14326 | `	}` |
|        - | 14327 | `	/* Top class */` |
|     1001 | 14328 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     1001 | 14329 | `	if( pClass ){` |
|      997 | 14330 | `		ph7_value_reset_string_cursor(pValue);` |
|      997 | 14331 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      997 | 14332 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      496 | 14333 | `	}` |
|        - | 14334 | `	/* Return the freshly created array */` |
|     1001 | 14335 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14336 | `	/*` |
|        - | 14337 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 14338 | `	 * as soon we return from this function.` |
|        - | 14339 | `	 */` |
|     1001 | 14340 | `	return PH7_OK;` |
|      503 | 14341 |  |
|        - | 14342 | `/*` |
|        - | 14343 | ` * Generate a small backtrace.` |
|        - | 14344 | ` * Store the generated dump in the given BLOB` |
|        - | 14345 | ` */` |
|        4 | 14346 | `static int VmMiniBacktrace(` |
|        - | 14347 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14348 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 14349 | `	)` |
|        1 | 14350 |  |
|        5 | 14351 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14352 | `	ph7_vm_func *pFunc;` |
|        - | 14353 | `	ph7_class *pClass;` |
|        - | 14354 | `	SyString *pFile;` |
|        - | 14355 | `	/* Called function */` |
|        5 | 14356 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 14357 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 14358 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14359 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 14360 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 14361 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 14362 | `	}else{` |
|      ! 0 | 14363 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 14364 | `	}` |
|        5 | 14365 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 14366 | `	/* Current processed script */` |
|        5 | 14367 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 14368 | `	if( pFile ){` |
|        5 | 14369 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14370 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 14371 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 14372 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 14373 | `	}` |
|        - | 14374 | `	/* Top class */` |
|        5 | 14375 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 14376 | `	if( pClass ){` |
|      ! 0 | 14377 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 14378 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 14379 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 14380 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 14381 | `	}` |
|        5 | 14382 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 14383 | `	/* All done */` |
|        5 | 14384 | `	return SXRET_OK;` |
|        1 | 14385 |  |
|        - | 14386 | `/*` |
|        - | 14387 | ` * void debug_print_backtrace()` |
|        - | 14388 | ` *  Prints a backtrace` |
|        - | 14389 | ` * Parameters` |
|        - | 14390 | ` * None` |
|        - | 14391 | ` * Return` |
|        - | 14392 | ` * NULL` |
|        - | 14393 | ` */` |
|        2 | 14394 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14395 |  |
|        3 | 14396 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14397 | `	SyBlob sDump;` |
|        3 | 14398 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14399 | `	/* Generate the backtrace */` |
|        3 | 14400 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14401 | `	/* Output backtrace */` |
|        3 | 14402 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 14403 | `	/* All done,cleanup */` |
|        3 | 14404 | `	SyBlobRelease(&sDump);` |
|        1 | 14405 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14406 | `	SXUNUSED(apArg);` |
|        3 | 14407 | `	return PH7_OK;` |
|        1 | 14408 |  |
|        - | 14409 | `/*` |
|        - | 14410 | ` * string debug_string_backtrace()` |
|        - | 14411 | ` *  Generate a backtrace` |
|        - | 14412 | ` * Parameters` |
|        - | 14413 | ` * None` |
|        - | 14414 | ` * Return` |
|        - | 14415 | ` *  A mini backtrace().` |
|        - | 14416 | ` * Note that this is a symisc extension.` |
|        - | 14417 | ` */` |
|        2 | 14418 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14419 |  |
|        3 | 14420 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14421 | `	SyBlob sDump;` |
|        3 | 14422 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14423 | `	/* Generate the backtrace */` |
|        3 | 14424 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14425 | `	/* Return the backtrace */` |
|        3 | 14426 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 14427 | `	/* All done,cleanup */` |
|        3 | 14428 | `	SyBlobRelease(&sDump);` |
|        1 | 14429 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14430 | `	SXUNUSED(apArg);` |
|        3 | 14431 | `	return PH7_OK;` |
|        1 | 14432 |  |
|        - | 14433 | `/*` |
|        - | 14434 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 14435 | ` * exception is triggered.` |
|        - | 14436 | ` */` |
|      512 | 14437 | `static sxi32 VmUncaughtException(` |
|        - | 14438 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14439 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14440 | `	)` |
|        4 | 14441 |  |
|        - | 14442 | `	ph7_value *apArg[2],sArg;` |
|      516 | 14443 | `	int nArg = 1;` |
|        - | 14444 | `	sxi32 rc;` |
|      516 | 14445 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 14446 | `		/* Nesting limit reached */` |
|      ! 0 | 14447 | `		return SXRET_OK;` |
|        - | 14448 | `	}` |
|        - | 14449 | `	/* Call any exception handler if available */` |
|      516 | 14450 | `	PH7_MemObjInit(pVm,&sArg);` |
|      516 | 14451 | `	if( pThis ){` |
|        - | 14452 | `		/* Load the exception instance */` |
|      516 | 14453 | `		sArg.x.pOther = pThis;` |
|      516 | 14454 | `		pThis->iRef++;` |
|      516 | 14455 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      260 | 14456 | `	}else{` |
|      ! 0 | 14457 | `		nArg = 0;` |
|        - | 14458 | `	}` |
|      516 | 14459 | `	apArg[0] = &sArg;` |
|        - | 14460 | `	/* Call the exception handler if available */` |
|      516 | 14461 | `	pVm->nExceptDepth++;` |
|      516 | 14462 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      516 | 14463 | `	pVm->nExceptDepth--;` |
|      516 | 14464 | `	if( rc != SXRET_OK ){` |
|        - | 14465 | `		SyBlob sMsgBuf;` |
|      514 | 14466 | `		const char *zClass = "Exception";` |
|      514 | 14467 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 14468 | `		const char *zMsg;` |
|        - | 14469 | `		sxu32 nMsg;` |
|        - | 14470 | `		const char *zFuncName;` |
|        - | 14471 | `		int nFuncLen;` |
|      514 | 14472 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      514 | 14473 | `		if( pThis ){` |
|        - | 14474 | `			ph7_class_method *pGetMessage;` |
|        - | 14475 | `			ph7_value sMsg;` |
|        - | 14476 | `			const char *zTmp;` |
|        - | 14477 | `			int nTmp;` |
|      514 | 14478 | `			zClass = pThis->pClass->sName.zString;` |
|      514 | 14479 | `			nClass = pThis->pClass->sName.nByte;` |
|      514 | 14480 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      514 | 14481 | `			if( pGetMessage ){` |
|      514 | 14482 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      514 | 14483 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      514 | 14484 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      514 | 14485 | `					if( zTmp && nTmp > 0 ){` |
|      514 | 14486 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 14487 | `					}` |
|      255 | 14488 | `				}` |
|      514 | 14489 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 14490 | `			}` |
|      255 | 14491 | `		}` |
|      514 | 14492 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      514 | 14493 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      514 | 14494 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      514 | 14495 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      514 | 14496 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 14497 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      514 | 14498 | `		rc = SXERR_ABORT;` |
|      255 | 14499 | `	}` |
|      516 | 14500 | `	PH7_MemObjRelease(&sArg);` |
|      516 | 14501 | `	return rc;` |
|      260 | 14502 |  |
|        - | 14503 | `/*` |
|        - | 14504 | ` * Throw a user exception.` |
|        - | 14505 | ` *` |
|        - | 14506 | ` * Exception dispatch follows this sequence:` |
|        - | 14507 | ` *` |
|        - | 14508 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 14509 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 14510 | ` *` |
|        - | 14511 | ` * 2. If NO catch matches:` |
|        - | 14512 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 14513 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 14514 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 14515 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 14516 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 14517 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 14518 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 14519 | ` *` |
|        - | 14520 | ` * 3. If a catch DOES match:` |
|        - | 14521 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 14522 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 14523 | ` *       inside the catch body from immediately propagating past our` |
|        - | 14524 | ` *       finally block.` |
|        - | 14525 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 14526 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 14527 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 14528 | ` *       in pPendingException (step 2c).` |
|        - | 14529 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 14530 | ` *    d. Run finally (if present).` |
|        - | 14531 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 14532 | ` *       that handlers are restored and finally has run.` |
|        - | 14533 | ` */` |
|      926 | 14534 | `static sxi32 VmThrowException(` |
|        - | 14535 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 14536 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14537 | `	)` |
|        5 | 14538 |  |
|        - | 14539 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 14540 | `	ph7_exception **apException;` |
|        - | 14541 | `	ph7_exception *pException;` |
|        - | 14542 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 14543 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 14544 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      931 | 14545 | `	VmCoalesceDisarm(pVm);` |
|        - | 14546 | `	/* A fresh throw supersedes any pending catch/finally return (PHP: an` |
|        - | 14547 | ``	 * exception thrown in a catch/finally discards an earlier `return`). */`` |
|      931 | 14548 | `	if( pVm->bReturnRequested ){` |
|      ! 0 | 14549 | `		pVm->bReturnRequested = 0;` |
|      ! 0 | 14550 | `		PH7_MemObjRelease(&pVm->sCatchReturn);` |
|      ! 0 | 14551 | `	}` |
|        - | 14552 | `	/* Point to the stack of loaded exceptions */` |
|      931 | 14553 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      931 | 14554 | `	pException = 0;` |
|      931 | 14555 | `	pCatch = 0;` |
|      931 | 14556 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14557 | `		ph7_exception_block *aCatch;` |
|        - | 14558 | `		ph7_class *pClass;` |
|        - | 14559 | `		SyString *aNames;` |
|        - | 14560 | `		sxu32 nNames;` |
|        - | 14561 | `		int matched;` |
|        - | 14562 | `		sxu32 j,k;` |
|        - | 14563 | `		/* Locate the appropriate block to execute */` |
|      409 | 14564 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      409 | 14565 | `		(void)SySetPop(&pVm->aException);` |
|      409 | 14566 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      417 | 14567 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 14568 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      415 | 14569 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      415 | 14570 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      415 | 14571 | `			matched = 0;` |
|      441 | 14572 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 14573 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 14574 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 14575 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      433 | 14576 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      433 | 14577 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 14578 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 14579 | `					continue;` |
|        - | 14580 | `				}` |
|      433 | 14581 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      407 | 14582 | `					matched = 1;` |
|      407 | 14583 | `					break;` |
|        - | 14584 | `				}` |
|       14 | 14585 | `			}` |
|      415 | 14586 | `			if( matched ){` |
|        - | 14587 | `				/* Catch block found,break immediately */` |
|      407 | 14588 | `				pCatch = &aCatch[j];` |
|      407 | 14589 | `				break;` |
|        - | 14590 | `			}` |
|        5 | 14591 | `		}` |
|      202 | 14592 | `	}` |
|        - | 14593 | `	/* Execute the cached block if available */` |
|      931 | 14594 | `	if( pCatch == 0 ){` |
|        - | 14595 | `		sxi32 rc;` |
|        - | 14596 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      529 | 14597 | `		if( pException && pException->iHasFinally ){` |
|        3 | 14598 | `			pException->iFinallyDone = 1;` |
|        3 | 14599 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|        3 | 14600 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14601 | `				return SXERR_ABORT;` |
|        - | 14602 | `			}` |
|        1 | 14603 | `		}` |
|        - | 14604 | `		/* Check if there is an outer exception handler on the stack */` |
|      529 | 14605 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14606 | `			/* Re-throw to the outer handler */` |
|        3 | 14607 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14608 | `		}` |
|        - | 14609 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14610 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14611 | `		 * exception instead of reporting it uncaught.` |
|        - | 14612 | `		 */` |
|      527 | 14613 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14614 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14615 | `			 * by looking for a catch frame on the stack.` |
|        - | 14616 | `			 */` |
|      527 | 14617 | `			VmFrame *pF = pVm->pFrame;` |
|      527 | 14618 | `			int inCatch = 0;` |
|     1055 | 14619 | `			while( pF ){` |
|      543 | 14620 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|       11 | 14621 | `					inCatch = 1;` |
|       11 | 14622 | `					break;` |
|        - | 14623 | `				}` |
|      532 | 14624 | `				pF = pF->pParent;` |
|        4 | 14625 | `			}` |
|      527 | 14626 | `			if( inCatch ){` |
|        - | 14627 | `				/* Defer — will be re-thrown after finally runs */` |
|       11 | 14628 | `				pThis->iRef++;` |
|       11 | 14629 | `				pVm->pPendingException = pThis;` |
|       11 | 14630 | `				return SXRET_OK;` |
|        - | 14631 | `			}` |
|      256 | 14632 | `		}` |
|        - | 14633 | `		/* Truly uncaught */` |
|      516 | 14634 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      516 | 14635 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14636 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14637 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14638 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14639 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14640 | `			}` |
|      ! 0 | 14641 | `		}` |
|      516 | 14642 | `		return rc;` |
|      ! 0 | 14643 | `	}else{` |
|      407 | 14644 | `		VmFrame *pFrame = pVm->pFrame;` |
|      407 | 14645 | `		ph7_exception **apSaved = 0;` |
|        - | 14646 | `		sxu32 nSavedCount;` |
|        - | 14647 | `		sxi32 rc;` |
|      407 | 14648 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      407 | 14649 | `		if( pException->pFrame == pFrame ){` |
|      291 | 14650 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      143 | 14651 | `		}` |
|        - | 14652 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14653 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14654 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14655 | `		 */` |
|      407 | 14656 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      407 | 14657 | `		if( nSavedCount > 0 ){` |
|       22 | 14658 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        7 | 14659 | `				nSavedCount * sizeof(ph7_exception *));` |
|       15 | 14660 | `			if( apSaved ){` |
|       22 | 14661 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        7 | 14662 | `					nSavedCount * sizeof(ph7_exception *));` |
|       15 | 14663 | `				SySetReset(&pVm->aException);` |
|        7 | 14664 | `			}` |
|        7 | 14665 | `		}` |
|        - | 14666 | `		/* Create the catch frame (made transparent below) */` |
|      407 | 14667 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      407 | 14668 | `		if( rc == SXRET_OK ){` |
|        - | 14669 | `			ph7_value *pObj;` |
|        - | 14670 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 14671 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 14672 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 14673 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 14674 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 14675 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 14676 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 14677 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|      407 | 14678 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|      407 | 14679 | `			pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      407 | 14680 | `			if( pObj ){` |
|        - | 14681 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 14682 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 14683 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 14684 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 14685 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 14686 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|      407 | 14687 | `				pThis->iRef++;` |
|      407 | 14688 | `				PH7_MemObjRelease(pObj);` |
|      407 | 14689 | `				pObj->x.pOther = pThis;` |
|      407 | 14690 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      201 | 14691 | `			}` |
|        - | 14692 | `			/* Execute the catch block */` |
|      407 | 14693 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);` |
|        - | 14694 | `			/* Leave the frame */` |
|      407 | 14695 | `			VmLeaveFrame(&(*pVm));` |
|      201 | 14696 | `		}` |
|        - | 14697 | `		/* Restore the outer exception handlers */` |
|      407 | 14698 | `		if( apSaved ){` |
|        - | 14699 | `			sxu32 k;` |
|        - | 14700 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14701 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14702 | `			 * Restore the original outer entries.` |
|        - | 14703 | `			 */` |
|       15 | 14704 | `			SySetReset(&pVm->aException);` |
|       29 | 14705 | `			for(k = 0; k < nSavedCount; k++){` |
|       15 | 14706 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        8 | 14707 | `			}` |
|       15 | 14708 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        7 | 14709 | `		}` |
|        - | 14710 | `		/* Execute the finally block after catch */` |
|      407 | 14711 | `		if( pException->iHasFinally ){` |
|       25 | 14712 | `			pException->iFinallyDone = 1;` |
|        - | 14713 | `			{` |
|       25 | 14714 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|       25 | 14715 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14716 | `					return SXERR_ABORT;` |
|        - | 14717 | `				}` |
|        - | 14718 | `			}` |
|       11 | 14719 | `		}` |
|      407 | 14720 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14721 | `			return SXERR_ABORT;` |
|        - | 14722 | `		}` |
|        - | 14723 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14724 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14725 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 14726 | ``		 * unless the finally itself issued a `return`, which swallows the`` |
|        - | 14727 | `		 * in-flight exception (PHP semantics).` |
|        - | 14728 | `		 */` |
|      407 | 14729 | `		if( pVm->pPendingException ){` |
|       11 | 14730 | `			if( !pVm->bReturnRequested ){` |
|        9 | 14731 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14732 | `				pVm->pPendingException = 0;` |
|        9 | 14733 | `				return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14734 | `			}` |
|        - | 14735 | `			/* Swallowed by finally's return: drop the deferred exception. */` |
|        3 | 14736 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        3 | 14737 | `			pVm->pPendingException = 0;` |
|        1 | 14738 | `		}` |
|        - | 14739 | `	}` |
|        - | 14740 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14741 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14742 | `	 */` |
|      399 | 14743 | `	return SXRET_OK;` |
|      468 | 14744 |  |
|        - | 14745 | `/*` |
|        - | 14746 | ` * Section:` |
|        - | 14747 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14748 | ` * Status:` |
|        - | 14749 | ` *    Stable.` |
|        - | 14750 | ` */` |
|        - | 14751 | `/*` |
|        - | 14752 | ` * string ph7version(void)` |
|        - | 14753 | ` *  Returns the running version of the PH7 version.` |
|        - | 14754 | ` * Parameters` |
|        - | 14755 | ` *  None` |
|        - | 14756 | ` * Return` |
|        - | 14757 | ` * Current PH7 version.` |
|        - | 14758 | ` */` |
|        2 | 14759 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14760 |  |
|        1 | 14761 | `	SXUNUSED(nArg);` |
|        1 | 14762 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14763 | `	/* Current engine version */` |
|        3 | 14764 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14765 | `	return PH7_OK;` |
|        1 | 14766 |  |
|        - | 14767 | `/*` |
|        - | 14768 | ` * string phpversion([ string $extension ])` |
|        - | 14769 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14770 | ` * Parameters` |
|        - | 14771 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14772 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14773 | ` * Return` |
|        - | 14774 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14775 | ` */` |
|        4 | 14776 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14777 |  |
|        2 | 14778 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14779 | `	if( nArg > 0 ){` |
|      ! 0 | 14780 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14781 | `		return PH7_OK;` |
|        - | 14782 | `	}` |
|        5 | 14783 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14784 | `	return PH7_OK;` |
|        3 | 14785 |  |
|        - | 14786 | `/*` |
|        - | 14787 | ` * string php_sapi_name(void)` |
|        - | 14788 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14789 | ` * Parameters` |
|        - | 14790 | ` *  None` |
|        - | 14791 | ` * Return` |
|        - | 14792 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14793 | ` */` |
|        2 | 14794 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14795 |  |
|        3 | 14796 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14797 | `	SXUNUSED(nArg);` |
|        1 | 14798 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14799 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14800 | `	return PH7_OK;` |
|        1 | 14801 |  |
|        - | 14802 | `/*` |
|        - | 14803 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14804 | ` */` |
|        - | 14805 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14806 | ` "<html><head>"\` |
|        - | 14807 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14808 | ` "<style type=\"text/css\">"\` |
|        - | 14809 | ` "div {"\` |
|        - | 14810 | `     "border: 1px solid #cccccc;"\` |
|        - | 14811 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14812 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14813 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14814 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14815 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14816 | `     "-o-border-radius: 10px;"\` |
|        - | 14817 | `     "border-radius: 10px;"\` |
|        - | 14818 | `     "padding-left: 2em;"\` |
|        - | 14819 | `     "background-color: white;"\` |
|        - | 14820 | `     "margin-left: auto;"\` |
|        - | 14821 | `     "font-family: verdana;"\` |
|        - | 14822 | `     "padding-right: 2em;"\` |
|        - | 14823 | `     "margin-right: auto;"\` |
|        - | 14824 | `     "}"\` |
|        - | 14825 | `     "body {"\` |
|        - | 14826 | `     "padding: 0.2em;"\` |
|        - | 14827 | `     "font-style: normal;"\` |
|        - | 14828 | `     "font-size: medium;"\` |
|        - | 14829 | `     "background-color: #f2f2f2;"\` |
|        - | 14830 | `     "}"\` |
|        - | 14831 | `     "hr {"\` |
|        - | 14832 | `     "border-style: solid none none;"\` |
|        - | 14833 | `     "border-width: 1px medium medium;"\` |
|        - | 14834 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14835 | `     "height: 1px;"\` |
|        - | 14836 | `     "}"\` |
|        - | 14837 | `     "a {"\` |
|        - | 14838 | `     "color: #3366cc;"\` |
|        - | 14839 | `     "text-decoration: none;"\` |
|        - | 14840 | `     "}"\` |
|        - | 14841 | `     "a:hover {"\` |
|        - | 14842 | `     "color: #999999;"\` |
|        - | 14843 | `     "}"\` |
|        - | 14844 | `     "a:active {"\` |
|        - | 14845 | `     "color: #663399;"\` |
|        - | 14846 | `     "}"\` |
|        - | 14847 | `     "h1 {"\` |
|        - | 14848 | `     "margin: 0;"\` |
|        - | 14849 | `     "padding: 0;"\` |
|        - | 14850 | `     "font-family: Verdana;"\` |
|        - | 14851 | `     "font-weight: bold;"\` |
|        - | 14852 | `     "font-style: normal;"\` |
|        - | 14853 | `     "font-size: medium;"\` |
|        - | 14854 | `     "text-transform: capitalize;"\` |
|        - | 14855 | `     "color: #0a328c;"\` |
|        - | 14856 | `     "}"\` |
|        - | 14857 | `     "p {"\` |
|        - | 14858 | `     "margin: 0 auto;"\` |
|        - | 14859 | `     "font-size: medium;"\` |
|        - | 14860 | `     "font-style: normal;"\` |
|        - | 14861 | `     "font-family: verdana;"\` |
|        - | 14862 | `     "}"\` |
|        - | 14863 | `"</style></head><body>"\` |
|        - | 14864 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14865 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14866 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14867 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14868 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14869 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14870 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14871 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14872 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14873 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14874 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14875 |  |
|        - | 14876 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14877 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14878 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14879 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14880 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14881 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14882 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14883 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14884 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14885 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14886 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14887 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14888 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14889 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14890 |  |
|        - | 14891 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14892 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14893 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14894 | `"&nbsp;*<br>"\` |
|        - | 14895 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14896 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14897 | `"&nbsp;* are met:<br>"\` |
|        - | 14898 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14899 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14900 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14901 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14902 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14903 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14904 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14905 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14906 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14907 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14908 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14909 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14910 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14911 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14912 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14913 | `"&nbsp;*<br>"\` |
|        - | 14914 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14915 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14916 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14917 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14918 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14919 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14920 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14921 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14922 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14923 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14924 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14925 | `"&nbsp;*/<br>"\` |
|        - | 14926 | `"</span></small></small></p>"\` |
|        - | 14927 | `"</div></body></html>"` |
|        - | 14928 | `/*` |
|        - | 14929 | ` * bool ph7credits(void)` |
|        - | 14930 | ` * bool ph7info(void)` |
|        - | 14931 | ` * bool ph7copyright(void)` |
|        - | 14932 | ` *  Prints out the credits for PH7 engine` |
|        - | 14933 | ` * Parameters` |
|        - | 14934 | ` *  None` |
|        - | 14935 | ` * Return` |
|        - | 14936 | ` *  Always TRUE` |
|        - | 14937 | ` */` |
|        2 | 14938 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14939 |  |
|        3 | 14940 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14941 | `	/* Expand the HTML page above*/` |
|        3 | 14942 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14943 | `	ph7_context_output_format(` |
|        1 | 14944 | `		pCtx,` |
|        - | 14945 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14946 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14947 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14948 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14949 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14950 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14951 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14952 | `#ifdef __WINNT__` |
|        - | 14953 | `		"Windows NT"` |
|        - | 14954 | `#elif defined(__UNIXES__)` |
|        - | 14955 | `		"UNIX-Like"` |
|        - | 14956 | `#else` |
|        - | 14957 | `		"Other OS"` |
|        - | 14958 | `#endif` |
|        - | 14959 | `		);` |
|        3 | 14960 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14961 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14962 | `	SXUNUSED(apArg);` |
|        - | 14963 | `	/* Return TRUE */` |
|        - | 14964 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14965 | `	return PH7_OK;` |
|        1 | 14966 |  |
|        - | 14967 | `/*` |
|        - | 14968 | ` * Section:` |
|        - | 14969 | ` *    URL related routines.` |
|        - | 14970 | ` * Status:` |
|        - | 14971 | ` *    Stable.` |
|        - | 14972 | ` */` |
|        - | 14973 | `/*` |
|        - | 14974 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14975 | ` *  Parse a URL and return its fields.` |
|        - | 14976 | ` * Parameters` |
|        - | 14977 | ` *  $url` |
|        - | 14978 | ` *   The URL to parse.` |
|        - | 14979 | ` * $component` |
|        - | 14980 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14981 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14982 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14983 | ` *  in which case the return value will be an integer).` |
|        - | 14984 | ` * Return` |
|        - | 14985 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14986 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14987 | ` *  this array are:` |
|        - | 14988 | ` *   scheme - e.g. http` |
|        - | 14989 | ` *   host` |
|        - | 14990 | ` *   port` |
|        - | 14991 | ` *   user` |
|        - | 14992 | ` *   pass` |
|        - | 14993 | ` *   path` |
|        - | 14994 | ` *   query - after the question mark ?` |
|        - | 14995 | ` *   fragment - after the hashmark #` |
|        - | 14996 | ` * Note:` |
|        - | 14997 | ` *  FALSE is returned on failure.` |
|        - | 14998 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14999 | ` *  with the standard PHP engine.` |
|        - | 15000 | ` */` |
|       28 | 15001 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15002 |  |
|        - | 15003 | `	const char *zStr; /* Input string */` |
|        - | 15004 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 15005 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 15006 | `	int nLen;` |
|        - | 15007 | `	sxi32 rc;` |
|       29 | 15008 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 15009 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 15010 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15011 | `		return PH7_OK;` |
|        - | 15012 | `	}` |
|        - | 15013 | `	/* Extract the given URI */` |
|       29 | 15014 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 15015 | `	if( nLen < 1 ){` |
|        - | 15016 | `		/* Nothing to process,return FALSE */` |
|        3 | 15017 | `		ph7_result_bool(pCtx,0);` |
|        3 | 15018 | `		return PH7_OK;` |
|        - | 15019 | `	}` |
|        - | 15020 | `	/* Get a parse */` |
|       27 | 15021 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 15022 | `	if( rc != SXRET_OK ){` |
|        - | 15023 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 15024 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15025 | `		return PH7_OK;` |
|        - | 15026 | `	}` |
|       27 | 15027 | `	if( nArg > 1 ){` |
|      ! 0 | 15028 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 15029 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 15030 | `		switch(nComponent){` |
|      ! 0 | 15031 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 15032 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 15033 | `			if( pComp->nByte < 1 ){` |
|        - | 15034 | `				/* No available value,return NULL */` |
|      ! 0 | 15035 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15036 | `			}else{` |
|      ! 0 | 15037 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15038 | `			}` |
|      ! 0 | 15039 | `			break;` |
|      ! 0 | 15040 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 15041 | `			pComp = &sURI.sHost;` |
|      ! 0 | 15042 | `			if( pComp->nByte < 1 ){` |
|        - | 15043 | `				/* No available value,return NULL */` |
|      ! 0 | 15044 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15045 | `			}else{` |
|      ! 0 | 15046 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15047 | `			}` |
|      ! 0 | 15048 | `			break;` |
|      ! 0 | 15049 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 15050 | `			pComp = &sURI.sPort;` |
|      ! 0 | 15051 | `			if( pComp->nByte < 1 ){` |
|        - | 15052 | `				/* No available value,return NULL */` |
|      ! 0 | 15053 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15054 | `			}else{` |
|      ! 0 | 15055 | `				int iPort = 0;` |
|        - | 15056 | `				/* Cast the value to integer */` |
|      ! 0 | 15057 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 15058 | `				ph7_result_int(pCtx,iPort);` |
|        - | 15059 | `			}` |
|      ! 0 | 15060 | `			break;` |
|      ! 0 | 15061 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 15062 | `			pComp = &sURI.sUser;` |
|      ! 0 | 15063 | `			if( pComp->nByte < 1 ){` |
|        - | 15064 | `				/* No available value,return NULL */` |
|      ! 0 | 15065 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15066 | `			}else{` |
|      ! 0 | 15067 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15068 | `			}` |
|      ! 0 | 15069 | `			break;` |
|      ! 0 | 15070 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 15071 | `			pComp = &sURI.sPass;` |
|      ! 0 | 15072 | `			if( pComp->nByte < 1 ){` |
|        - | 15073 | `				/* No available value,return NULL */` |
|      ! 0 | 15074 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15075 | `			}else{` |
|      ! 0 | 15076 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15077 | `			}` |
|      ! 0 | 15078 | `			break;` |
|      ! 0 | 15079 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 15080 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 15081 | `			if( pComp->nByte < 1 ){` |
|        - | 15082 | `				/* No available value,return NULL */` |
|      ! 0 | 15083 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15084 | `			}else{` |
|      ! 0 | 15085 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15086 | `			}` |
|      ! 0 | 15087 | `			break;` |
|      ! 0 | 15088 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 15089 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 15090 | `			if( pComp->nByte < 1 ){` |
|        - | 15091 | `				/* No available value,return NULL */` |
|      ! 0 | 15092 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15093 | `			}else{` |
|      ! 0 | 15094 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15095 | `			}` |
|      ! 0 | 15096 | `			break;` |
|      ! 0 | 15097 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 15098 | `			pComp = &sURI.sPath;` |
|      ! 0 | 15099 | `			if( pComp->nByte < 1 ){` |
|        - | 15100 | `				/* No available value,return NULL */` |
|      ! 0 | 15101 | `				ph7_result_null(pCtx);` |
|      ! 0 | 15102 | `			}else{` |
|      ! 0 | 15103 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 15104 | `			}` |
|      ! 0 | 15105 | `			break;` |
|      ! 0 | 15106 | `		default:` |
|        - | 15107 | `			/* No such entry,return NULL */` |
|      ! 0 | 15108 | `			ph7_result_null(pCtx);` |
|      ! 0 | 15109 | `			break;` |
|        - | 15110 | `		}` |
|      ! 0 | 15111 | `	}else{` |
|        - | 15112 | `		ph7_value *pArray,*pValue;` |
|        - | 15113 | `		/* Return an associative array */` |
|       27 | 15114 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 15115 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 15116 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 15117 | `			/* Out of memory */` |
|      ! 0 | 15118 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 15119 | `			/* Return false */` |
|      ! 0 | 15120 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 15121 | `			return PH7_OK;` |
|        - | 15122 | `		}` |
|        - | 15123 | `		/* Fill the array */` |
|       27 | 15124 | `		pComp = &sURI.sScheme;` |
|       27 | 15125 | `		if( pComp->nByte > 0 ){` |
|       19 | 15126 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 15127 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 15128 | `		}` |
|        - | 15129 | `		/* Reset the string cursor */` |
|       27 | 15130 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15131 | `		pComp = &sURI.sHost;` |
|       27 | 15132 | `		if( pComp->nByte > 0 ){` |
|       25 | 15133 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 15134 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 15135 | `		}` |
|        - | 15136 | `		/* Reset the string cursor */` |
|       27 | 15137 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15138 | `		pComp = &sURI.sPort;` |
|       27 | 15139 | `		if( pComp->nByte > 0 ){` |
|       11 | 15140 | `			int iPort = 0;/* cc warning */` |
|        - | 15141 | `			/* Convert to integer */` |
|       11 | 15142 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 15143 | `			ph7_value_int(pValue,iPort);` |
|       11 | 15144 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 15145 | `		}` |
|        - | 15146 | `		/* Reset the string cursor */` |
|       27 | 15147 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15148 | `		pComp = &sURI.sUser;` |
|       27 | 15149 | `		if( pComp->nByte > 0 ){` |
|        7 | 15150 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 15151 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 15152 | `		}` |
|        - | 15153 | `		/* Reset the string cursor */` |
|       27 | 15154 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15155 | `		pComp = &sURI.sPass;` |
|       27 | 15156 | `		if( pComp->nByte > 0 ){` |
|        7 | 15157 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 15158 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 15159 | `		}` |
|        - | 15160 | `		/* Reset the string cursor */` |
|       27 | 15161 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15162 | `		pComp = &sURI.sPath;` |
|       27 | 15163 | `		if( pComp->nByte > 0 ){` |
|       17 | 15164 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 15165 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 15166 | `		}` |
|        - | 15167 | `		/* Reset the string cursor */` |
|       27 | 15168 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15169 | `		pComp = &sURI.sQuery;` |
|       27 | 15170 | `		if( pComp->nByte > 0 ){` |
|        5 | 15171 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 15172 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 15173 | `		}` |
|        - | 15174 | `		/* Reset the string cursor */` |
|       27 | 15175 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15176 | `		pComp = &sURI.sFragment;` |
|       27 | 15177 | `		if( pComp->nByte > 0 ){` |
|        5 | 15178 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 15179 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 15180 | `		}` |
|        - | 15181 | `		/* Return the created array */` |
|       27 | 15182 | `		ph7_result_value(pCtx,pArray);` |
|        - | 15183 | `		/* NOTE:` |
|        - | 15184 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 15185 | `		 * automatically as soon we return from this function.` |
|        - | 15186 | `		 */` |
|        - | 15187 | `	}` |
|        - | 15188 | `	/* All done */` |
|       27 | 15189 | `	return PH7_OK;` |
|       15 | 15190 |  |
|        - | 15191 | `/*` |
|        - | 15192 | ` * Section:` |
|        - | 15193 | ` *   Array related routines.` |
|        - | 15194 | ` * Status:` |
|        - | 15195 | ` *    Stable.` |
|        - | 15196 | ` * Note 2012-5-21 01:04:15:` |
|        - | 15197 | ` *  Array related functions that need access to the underlying` |
|        - | 15198 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 15199 | ` */` |
|        - | 15200 | `/*` |
|        - | 15201 | ` * The [compact()] function store it's state information in an instance` |
|        - | 15202 | ` * of the following structure.` |
|        - | 15203 | ` */` |
|        - | 15204 | `struct compact_data` |
|        - | 15205 |  |
|        - | 15206 | `	ph7_value *pArray;  /* Target array */` |
|        - | 15207 | `	int nRecCount;      /* Recursion count */` |
|        - | 15208 | `};` |
|        - | 15209 | `/*` |
|        - | 15210 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 15211 | ` */` |
|      ! 0 | 15212 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 15213 |  |
|      ! 0 | 15214 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 15215 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 15216 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 15217 | `	/* Act according to the hashmap value */` |
|      ! 0 | 15218 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 15219 | `		SyString sVar;` |
|      ! 0 | 15220 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 15221 | `		if( sVar.nByte > 0 ){` |
|        - | 15222 | `			/* Query the current frame */` |
|      ! 0 | 15223 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 15224 | `			/* ^` |
|        - | 15225 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 15226 | `			 */` |
|      ! 0 | 15227 | `			if( pKey ){` |
|        - | 15228 | `				/* Perform the insertion */` |
|      ! 0 | 15229 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 15230 | `			}` |
|      ! 0 | 15231 | `		}` |
|      ! 0 | 15232 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 15233 | `		int rc;` |
|        - | 15234 | `		/* Recursively traverse this array */` |
|      ! 0 | 15235 | `		pData->nRecCount++;` |
|      ! 0 | 15236 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 15237 | `		pData->nRecCount--;` |
|      ! 0 | 15238 | `		return rc;` |
|        - | 15239 | `	}` |
|      ! 0 | 15240 | `	return SXRET_OK;` |
|      ! 0 | 15241 |  |
|        - | 15242 | `/*` |
|        - | 15243 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 15244 | ` *  Create array containing variables and their values.` |
|        - | 15245 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 15246 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 15247 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 15248 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 15249 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 15250 | ` * Parameters` |
|        - | 15251 | ` *  $varname` |
|        - | 15252 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 15253 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 15254 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 15255 | ` *   it recursively.` |
|        - | 15256 | ` * Return` |
|        - | 15257 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 15258 | ` */` |
|        2 | 15259 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15260 |  |
|        - | 15261 | `	ph7_value *pArray,*pObj;` |
|        3 | 15262 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15263 | `	const char *zName;` |
|        - | 15264 | `	SyString sVar;` |
|        - | 15265 | `	int i,nLen;` |
|        3 | 15266 | `	if( nArg < 1 ){` |
|        - | 15267 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 15268 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15269 | `		return PH7_OK;` |
|        - | 15270 | `	}` |
|        - | 15271 | `	/* Create the array */` |
|        3 | 15272 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 15273 | `	if( pArray == 0 ){` |
|        - | 15274 | `		/* Out of memory */` |
|      ! 0 | 15275 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 15276 | `		/* Return NULL */` |
|      ! 0 | 15277 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15278 | `		return PH7_OK;` |
|        - | 15279 | `	}` |
|        - | 15280 | `	/* Perform the requested operation */` |
|        7 | 15281 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 15282 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 15283 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 15284 | `				struct compact_data sData;` |
|      ! 0 | 15285 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 15286 | `				/* Recursively walk the array */` |
|      ! 0 | 15287 | `				sData.nRecCount = 0;` |
|      ! 0 | 15288 | `				sData.pArray = pArray;` |
|      ! 0 | 15289 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 15290 | `			}` |
|      ! 0 | 15291 | `		}else{` |
|        - | 15292 | `			/* Extract variable name */` |
|        5 | 15293 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 15294 | `			if( nLen > 0 ){` |
|        5 | 15295 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 15296 | `				/* Check if the variable is available in the current frame */` |
|        5 | 15297 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 15298 | `				if( pObj ){` |
|        5 | 15299 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 15300 | `				}` |
|        2 | 15301 | `			}` |
|        - | 15302 | `		}` |
|        3 | 15303 | `	}` |
|        - | 15304 | `	/* Return the array */` |
|        3 | 15305 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 15306 | `	return PH7_OK;` |
|        2 | 15307 |  |
|        - | 15308 | `/*` |
|        - | 15309 | ` * The [extract()] function store it's state information in an instance` |
|        - | 15310 | ` * of the following structure.` |
|        - | 15311 | ` */` |
|        - | 15312 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 15313 | `struct extract_aux_data` |
|        - | 15314 |  |
|        - | 15315 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 15316 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 15317 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 15318 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 15319 | `	int iFlags;           /* Control flags */` |
|        - | 15320 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 15321 | `};` |
|        - | 15322 | `/* Forward declaration */` |
|        - | 15323 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 15324 | `/*` |
|        - | 15325 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 15326 | ` *   Import variables into the current symbol table from an array.` |
|        - | 15327 | ` * Parameters` |
|        - | 15328 | ` * $var_array` |
|        - | 15329 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 15330 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 15331 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 15332 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 15333 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 15334 | ` * $extract_type` |
|        - | 15335 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 15336 | ` *  It can be one of the following values:` |
|        - | 15337 | ` *   EXTR_OVERWRITE` |
|        - | 15338 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 15339 | ` *   EXTR_SKIP` |
|        - | 15340 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 15341 | ` *   EXTR_PREFIX_SAME` |
|        - | 15342 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 15343 | ` *   EXTR_PREFIX_ALL` |
|        - | 15344 | ` *       Prefix all variable names with prefix.` |
|        - | 15345 | ` *   EXTR_PREFIX_INVALID` |
|        - | 15346 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 15347 | ` *   EXTR_IF_EXISTS` |
|        - | 15348 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 15349 | ` *       otherwise do nothing.` |
|        - | 15350 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 15351 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 15352 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 15353 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 15354 | ` *      the current symbol table.` |
|        - | 15355 | ` * $prefix` |
|        - | 15356 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 15357 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 15358 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 15359 | ` *  underscore character.` |
|        - | 15360 | ` * Return` |
|        - | 15361 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 15362 | ` */` |
|        4 | 15363 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15364 |  |
|        - | 15365 | `	extract_aux_data sAux;` |
|        - | 15366 | `	ph7_hashmap *pMap;` |
|        5 | 15367 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 15368 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 15369 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15370 | `		return PH7_OK;` |
|        - | 15371 | `	}` |
|        - | 15372 | `	/* Point to the target hashmap */` |
|        5 | 15373 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 15374 | `	if( pMap->nEntry < 1 ){` |
|        - | 15375 | `		/* Empty map,return  0 */` |
|      ! 0 | 15376 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15377 | `		return PH7_OK;` |
|        - | 15378 | `	}` |
|        - | 15379 | `	/* Prepare the aux data */` |
|        5 | 15380 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 15381 | `	if( nArg > 1 ){` |
|        3 | 15382 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 15383 | `		if( nArg > 2 ){` |
|      ! 0 | 15384 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 15385 | `		}` |
|        1 | 15386 | `	}` |
|        5 | 15387 | `	sAux.pVm = pCtx->pVm;` |
|        - | 15388 | `	/* Invoke the worker callback */` |
|        5 | 15389 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 15390 | `	/* Number of variables successfully imported */` |
|        5 | 15391 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 15392 | `	return PH7_OK;` |
|        3 | 15393 |  |
|        - | 15394 | `/*` |
|        - | 15395 | ` * Worker callback for the [extract()] function defined` |
|        - | 15396 | ` * below.` |
|        - | 15397 | ` */` |
|        8 | 15398 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15399 |  |
|        9 | 15400 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 15401 | `	int iFlags = pAux->iFlags;` |
|        9 | 15402 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15403 | `	ph7_value *pObj;` |
|        - | 15404 | `	SyString sVar;` |
|        9 | 15405 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 15406 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 15407 | `	}` |
|        - | 15408 | `	/* Perform a string cast */` |
|        9 | 15409 | `	PH7_MemObjToString(pKey);` |
|        9 | 15410 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15411 | `		/* Unavailable variable name */` |
|      ! 0 | 15412 | `		return SXRET_OK;` |
|        - | 15413 | `	}` |
|        9 | 15414 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 15415 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 15416 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15417 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15418 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15419 | `			);` |
|      ! 0 | 15420 | `	}else{` |
|       13 | 15421 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 15422 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15423 | `	}` |
|        9 | 15424 | `	sVar.zString = pAux->zWorker;` |
|        - | 15425 | `	/* Try to extract the variable */` |
|        9 | 15426 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 15427 | `	if( pObj ){` |
|        - | 15428 | `		/* Collision */` |
|        5 | 15429 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 15430 | `			return SXRET_OK;` |
|        - | 15431 | `		}` |
|        5 | 15432 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 15433 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 15434 | `				/* Already prefixed */` |
|      ! 0 | 15435 | `				return SXRET_OK;` |
|        - | 15436 | `			}` |
|      ! 0 | 15437 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15438 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15439 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15440 | `				);` |
|      ! 0 | 15441 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 15442 | `		}` |
|        3 | 15443 | `	}else{` |
|        - | 15444 | `		/* Create the variable */` |
|        5 | 15445 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 15446 | `	}` |
|        9 | 15447 | `	if( pObj ){` |
|        - | 15448 | `		/* Overwrite the old value */` |
|        9 | 15449 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 15450 | `		/* Increment counter */` |
|        9 | 15451 | `		pAux->iCount++;` |
|        4 | 15452 | `	}` |
|        9 | 15453 | `	return SXRET_OK;` |
|        5 | 15454 |  |
|        - | 15455 | `/*` |
|        - | 15456 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 15457 | ` * defined below.` |
|        - | 15458 | ` */` |
|        2 | 15459 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15460 |  |
|        3 | 15461 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 15462 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15463 | `	ph7_value *pObj;` |
|        - | 15464 | `	SyString sVar;` |
|        - | 15465 | `	/* Perform a string cast */` |
|        3 | 15466 | `	PH7_MemObjToString(pKey);` |
|        3 | 15467 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15468 | `		/* Unavailable variable name */` |
|      ! 0 | 15469 | `		return SXRET_OK;` |
|        - | 15470 | `	}` |
|        3 | 15471 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 15472 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 15473 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 15474 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 15475 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15476 | `			);` |
|        2 | 15477 | `	}else{` |
|      ! 0 | 15478 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 15479 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15480 | `	}` |
|        3 | 15481 | `	sVar.zString = pAux->zWorker;` |
|        - | 15482 | `	/* Extract the variable */` |
|        3 | 15483 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 15484 | `	if( pObj ){` |
|        3 | 15485 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 15486 | `	}` |
|        3 | 15487 | `	return SXRET_OK;` |
|        2 | 15488 |  |
|        - | 15489 | `/*` |
|        - | 15490 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 15491 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 15492 | ` * Parameters` |
|        - | 15493 | ` * $types` |
|        - | 15494 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 15495 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 15496 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 15497 | ` *  POST includes the POST uploaded file information.` |
|        - | 15498 | ` *  Note:` |
|        - | 15499 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 15500 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 15501 | ` * $prefix` |
|        - | 15502 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 15503 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 15504 | ` *  variable named $pref_userid.` |
|        - | 15505 | ` * Return` |
|        - | 15506 | ` *  TRUE on success or FALSE on failure.` |
|        - | 15507 | ` */` |
|        2 | 15508 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15509 |  |
|        - | 15510 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 15511 | `	extract_aux_data sAux;` |
|        - | 15512 | `	int nLen,nPrefixLen;` |
|        - | 15513 | `	ph7_value *pSuper;` |
|        - | 15514 | `	ph7_vm *pVm;` |
|        - | 15515 | `	/* By default import only $_GET variables  */` |
|        3 | 15516 | `	zImport = "G";` |
|        3 | 15517 | `	nLen = (int)sizeof(char);` |
|        3 | 15518 | `	zPrefix = 0;` |
|        3 | 15519 | `	nPrefixLen = 0;` |
|        3 | 15520 | `	if( nArg > 0 ){` |
|        3 | 15521 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 15522 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 15523 | `		}` |
|        3 | 15524 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 15525 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 15526 | `		}` |
|        1 | 15527 | `	}` |
|        - | 15528 | `	/* Point to the underlying VM */` |
|        3 | 15529 | `	pVm = pCtx->pVm;` |
|        - | 15530 | `	/* Initialize the aux data */` |
|        3 | 15531 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 15532 | `	sAux.zPrefix = zPrefix;` |
|        3 | 15533 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 15534 | `	sAux.pVm = pVm;` |
|        - | 15535 | `	/* Extract */` |
|        3 | 15536 | `	zEnd = &zImport[nLen];` |
|        5 | 15537 | `	while( zImport < zEnd ){` |
|        3 | 15538 | `		int c = zImport[0];` |
|        3 | 15539 | `		pSuper = 0;` |
|        3 | 15540 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 15541 | `			/* Import $_GET variables */` |
|        3 | 15542 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 15543 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 15544 | `			/* Import $_POST variables */` |
|      ! 0 | 15545 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 15546 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 15547 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 15548 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 15549 | `		}` |
|        3 | 15550 | `		if( pSuper ){` |
|        - | 15551 | `			/* Iterate throw array entries */` |
|        3 | 15552 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 15553 | `		}` |
|        - | 15554 | `		/* Advance the cursor */` |
|        3 | 15555 | `		zImport++;` |
|        1 | 15556 | `	}` |
|        - | 15557 | `	/* All done,return TRUE*/` |
|        3 | 15558 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15559 | `	return PH7_OK;` |
|        1 | 15560 |  |
|        - | 15561 | `/*` |
|        - | 15562 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 15563 | ` * Refer to the eval() language construct implementation for more` |
|        - | 15564 | ` * information.` |
|        - | 15565 | ` */` |
|    12952 | 15566 | `static sxi32 VmEvalChunk(` |
|        - | 15567 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 15568 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 15569 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 15570 | `	int iFlags,         /* Compile flag */` |
|        - | 15571 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 15572 | `	)` |
|        5 | 15573 |  |
|        - | 15574 | `	SySet *pByteCode,aByteCode;` |
|        - | 15575 | `	SyBlob sSavedNs;` |
|    12957 | 15576 | `	ProcConsumer xErr = 0;` |
|    12957 | 15577 | `	void *pErrData = 0;` |
|        - | 15578 | `	/* Initialize bytecode container */` |
|    12957 | 15579 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12957 | 15580 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 15581 | `	/* Reset the code generator */` |
|    12957 | 15582 | `	if( bTrueReturn ){` |
|        - | 15583 | `		/* Included file,log compile-time errors */` |
|     9734 | 15584 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9734 | 15585 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4865 | 15586 | `	}` |
|    12957 | 15587 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 15588 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 15589 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 15590 | `	 * the caller's namespace is restored. */` |
|    12957 | 15591 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12957 | 15592 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12957 | 15593 | `	if( bTrueReturn ){` |
|        - | 15594 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9734 | 15595 | `		SyBlobReset(&pVm->sNamespace);` |
|     4865 | 15596 | `	}` |
|        - | 15597 | `	/* Swap bytecode container */` |
|    12957 | 15598 | `	pByteCode = pVm->pByteContainer;` |
|    12957 | 15599 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 15600 | `	/* Compile the chunk */` |
|    12957 | 15601 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19431 | 15602 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 15603 | `		/* Compilation error,return false */` |
|        3 | 15604 | `		if( pCtx ){` |
|        3 | 15605 | `			ph7_result_bool(pCtx,0);` |
|        1 | 15606 | `		}` |
|        2 | 15607 | `	}else{` |
|        - | 15608 | `		/* Mount any newly defined classes */` |
|        - | 15609 | `		SyHashEntry *pEntry;` |
|        - | 15610 | `		ph7_class *pClass;` |
|        - | 15611 | `		ph7_value sResult; /* Return value */` |
|        - | 15612 | `		sxi32 rc;` |
|    12955 | 15613 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|  1022898 | 15614 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  1003475 | 15615 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 15616 | `			/* Only mount classes that haven't been mounted yet */` |
|  1003475 | 15617 | `			if( !pClass->bMounted ){` |
|   251743 | 15618 | `				rc = VmMountUserClass(pVm,pClass);` |
|   251743 | 15619 | `				if( rc != SXRET_OK ){` |
|        - | 15620 | `					/* Mount failure (likely memory error) */` |
|        3 | 15621 | `					if( pCtx ){` |
|        3 | 15622 | `						ph7_result_bool(pCtx,0);` |
|        1 | 15623 | `					}` |
|        4 | 15624 | `					goto Cleanup;` |
|        - | 15625 | `				}` |
|   125868 | 15626 | `			}` |
|        5 | 15627 | `		}` |
|    12953 | 15628 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15629 | `			/* Out of memory */` |
|      ! 0 | 15630 | `			if( pCtx ){` |
|      ! 0 | 15631 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15632 | `			}` |
|      ! 0 | 15633 | `			goto Cleanup;` |
|        - | 15634 | `		}` |
|    12953 | 15635 | `		if( bTrueReturn ){` |
|        - | 15636 | `			/* Assume a boolean true return value */` |
|     9734 | 15637 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4869 | 15638 | `		}else{` |
|        - | 15639 | `			/* Assume a null return value */` |
|     3223 | 15640 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15641 | `		}` |
|        - | 15642 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here,` |
|        - | 15643 | `		 * a path the OP_CALL cap check can't see; bound it under the same limit` |
|        - | 15644 | `		 * so a recursive include/eval can't overflow the native stack. */` |
|    12953 | 15645 | `		if( VmRecursionExceeded(pVm) ){` |
|        3 | 15646 | `			PH7_MemObjRelease(&sResult);` |
|        3 | 15647 | `			VmRecursionFatal(pVm);` |
|        3 | 15648 | `			goto Cleanup;` |
|        - | 15649 | `		}` |
|    12951 | 15650 | `		pVm->nRecursionDepth++;` |
|    12951 | 15651 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    12951 | 15652 | `		pVm->nRecursionDepth--;` |
|    12951 | 15653 | `		if( pCtx ){` |
|        - | 15654 | `			/* Set the execution result */` |
|     9787 | 15655 | `			ph7_result_value(pCtx,&sResult);` |
|     4891 | 15656 | `		}` |
|    12951 | 15657 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15658 | `	}` |
|     6476 | 15659 | `Cleanup:` |
|        - | 15660 | `	/* Cleanup the mess left behind */` |
|    12957 | 15661 | `	pVm->pByteContainer = pByteCode;` |
|    12957 | 15662 | `	SySetRelease(&aByteCode);` |
|        - | 15663 | `	/* Restore caller's namespace state */` |
|    12957 | 15664 | `	SyBlobReset(&pVm->sNamespace);` |
|    12957 | 15665 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12957 | 15666 | `	SyBlobRelease(&sSavedNs);` |
|    12957 | 15667 | `	return SXRET_OK;` |
|        5 | 15668 |  |
|        - | 15669 | `/*` |
|        - | 15670 | ` * value eval(string $code)` |
|        - | 15671 | ` *   Evaluate a string as PHP code.` |
|        - | 15672 | ` * Parameter` |
|        - | 15673 | ` *  code: PHP code to evaluate.` |
|        - | 15674 | ` * Return` |
|        - | 15675 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15676 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15677 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15678 | ` */` |
|       60 | 15679 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 15680 |  |
|        - | 15681 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       64 | 15682 | `	if( nArg < 1 ){` |
|        - | 15683 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15684 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15685 | `		return SXRET_OK;` |
|        - | 15686 | `	}` |
|        - | 15687 | `	/* Chunk to evaluate */` |
|       64 | 15688 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       64 | 15689 | `	if( sChunk.nByte < 1 ){` |
|        - | 15690 | `		/* Empty string,return NULL */` |
|        3 | 15691 | `		ph7_result_null(pCtx);` |
|        3 | 15692 | `		return SXRET_OK;` |
|        - | 15693 | `	}` |
|        - | 15694 | `	/* Eval the chunk */` |
|       62 | 15695 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       62 | 15696 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15697 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       40 | 15698 | `		return PH7_ABORT;` |
|        - | 15699 | `	}` |
|       23 | 15700 | `	return SXRET_OK;` |
|       34 | 15701 |  |
|        - | 15702 | `/*` |
|        - | 15703 | ` * Check if a file path is already included.` |
|        - | 15704 | ` */` |
|    19458 | 15705 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        4 | 15706 |  |
|        - | 15707 | `	SyString *aEntries;` |
|        - | 15708 | `	sxu32 n;` |
|    19462 | 15709 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15710 | `	/* Perform a linear search */` |
| 94459498 | 15711 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 94440050 | 15712 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15713 | `			/* Already included */` |
|       11 | 15714 | `			return TRUE;` |
|        - | 15715 | `		}` |
| 47220022 | 15716 | `	}` |
|    19452 | 15717 | `	return FALSE;` |
|     9733 | 15718 |  |
|        - | 15719 | `/*` |
|        - | 15720 | ` * Push a file path in the appropriate VM container.` |
|        - | 15721 | ` */` |
|    22614 | 15722 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 | 15723 |  |
|        - | 15724 | `	SyString sPath;` |
|        - | 15725 | `	char *zDup;` |
|        - | 15726 | `#ifdef __WINNT__` |
|        - | 15727 | `	char *zCur;` |
|        - | 15728 | `#endif` |
|        - | 15729 | `	sxi32 rc;` |
|    22619 | 15730 | `	if( nLen < 0 ){` |
|     3161 | 15731 | `		nLen = SyStrlen(zPath);` |
|     1578 | 15732 | `	}` |
|        - | 15733 | `	/* Duplicate the file path first */` |
|    22619 | 15734 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22619 | 15735 | `	if( zDup == 0 ){` |
|      ! 0 | 15736 | `		return SXERR_MEM;` |
|        - | 15737 | `	}` |
|        - | 15738 | `#ifdef __WINNT__` |
|        - | 15739 | `	/* Normalize path on windows` |
|        - | 15740 | `	 * Example:` |
|        - | 15741 | `	 *    Path/To/File.php` |
|        - | 15742 | `	 * becomes` |
|        - | 15743 | `	 *   path\to\file.php` |
|        - | 15744 | `	 */` |
|        5 | 15745 | `	zCur = zDup;` |
|        5 | 15746 | `	while( zCur[0] != 0 ){` |
|        5 | 15747 | `		if( zCur[0] == '/' ){` |
|        5 | 15748 | `			zCur[0] = '\\';` |
|        5 | 15749 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15750 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15751 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15752 | `		}` |
|        5 | 15753 | `		zCur++;` |
|        5 | 15754 | `	}` |
|        - | 15755 | `#endif` |
|        - | 15756 | `	/* Install the file path */` |
|    22619 | 15757 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22619 | 15758 | `	if( !bMain ){` |
|    19462 | 15759 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15760 | `			/* Already included */` |
|       11 | 15761 | `			*pNew = 0;` |
|        6 | 15762 | `		}else{` |
|        - | 15763 | `			/* Insert in the corresponding container */` |
|    19452 | 15764 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19452 | 15765 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15766 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15767 | `				return rc;` |
|        - | 15768 | `			}` |
|    19452 | 15769 | `			*pNew = 1;` |
|        - | 15770 | `		}` |
|     9729 | 15771 | `	}` |
|    22619 | 15772 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22619 | 15773 | `	return SXRET_OK;` |
|    11312 | 15774 |  |
|        - | 15775 | `/*` |
|        - | 15776 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15777 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15778 | ` * indicates failure.` |
|        - | 15779 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15780 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15781 | ` * operations.` |
|        - | 15782 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15783 | ` * this function is a no-op.` |
|        - | 15784 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15785 | ` * constructs for more information.` |
|        - | 15786 | ` */` |
|     9744 | 15787 | `static sxi32 VmExecIncludedFile(` |
|        - | 15788 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15789 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15790 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15791 | `	 )` |
|        4 | 15792 |  |
|        - | 15793 | `	sxi32 rc;` |
|        - | 15794 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15795 | `	const ph7_io_stream *pStream;` |
|        - | 15796 | `	SyBlob sContents;` |
|        - | 15797 | `	void *pHandle;` |
|        - | 15798 | `	ph7_vm *pVm;` |
|        - | 15799 | `	int isNew;` |
|        - | 15800 | `	/* Initialize fields */` |
|     9748 | 15801 | `	pVm = pCtx->pVm;` |
|     9748 | 15802 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9748 | 15803 | `	isNew = 0;` |
|        - | 15804 | `	/* Extract the associated stream */` |
|     9748 | 15805 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15806 | `	/*` |
|        - | 15807 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15808 | `	 * in a read-only mode.` |
|        - | 15809 | `	 */` |
|     9748 | 15810 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9748 | 15811 | `	if( pHandle == 0 ){` |
|        8 | 15812 | `		return SXERR_IO;` |
|        - | 15813 | `	}` |
|     9742 | 15814 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9742 | 15815 | `	if( IncludeOnce && !isNew ){` |
|        - | 15816 | `		/* Already included */` |
|        9 | 15817 | `		rc = SXERR_EXISTS;` |
|        5 | 15818 | `	}else{` |
|        - | 15819 | `		/* Read the whole file contents */` |
|     9734 | 15820 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9734 | 15821 | `		if( rc == SXRET_OK ){` |
|        - | 15822 | `			SyString sScript;` |
|        - | 15823 | `			/* Compile and execute the script */` |
|     9734 | 15824 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9734 | 15825 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4865 | 15826 | `		}` |
|        - | 15827 | `	}` |
|        - | 15828 | `	/* Pop from the set of included file */` |
|     9742 | 15829 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15830 | `	/* Close the handle */` |
|     9742 | 15831 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15832 | `	/* Release the working buffer */` |
|     9742 | 15833 | `	SyBlobRelease(&sContents);` |
|        - | 15834 | `#else` |
|        - | 15835 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15836 | `	SXUNUSED(pPath);` |
|        - | 15837 | `	SXUNUSED(IncludeOnce);` |
|        - | 15838 | `	rc = SXERR_IO;` |
|        - | 15839 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9742 | 15840 | `	return rc;` |
|     4876 | 15841 |  |
|        - | 15842 | `/*` |
|        - | 15843 | ` * string get_include_path(void)` |
|        - | 15844 | ` *  Gets the current include_path configuration option.` |
|        - | 15845 | ` * Parameter` |
|        - | 15846 | ` *  None` |
|        - | 15847 | ` * Return` |
|        - | 15848 | ` *  Included paths as a string` |
|        - | 15849 | ` */` |
|        2 | 15850 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15851 |  |
|        3 | 15852 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15853 | `	SyString *aEntry;` |
|        - | 15854 | `	int dir_sep;` |
|        - | 15855 | `	sxu32 n;` |
|        - | 15856 | `#ifdef __WINNT__` |
|        1 | 15857 | `	dir_sep = ';';` |
|        - | 15858 | `#else` |
|        - | 15859 | `	/* Assume UNIX path separator */` |
|        2 | 15860 | `	dir_sep = ':';` |
|        - | 15861 | `#endif` |
|        1 | 15862 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15863 | `	SXUNUSED(apArg);` |
|        - | 15864 | `	/* Point to the list of import paths */` |
|        3 | 15865 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15866 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15867 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15868 | `		if( n > 0 ){` |
|        - | 15869 | `			/* Append dir seprator */` |
|      ! 0 | 15870 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15871 | `		}` |
|        - | 15872 | `		/* Append path */` |
|        3 | 15873 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15874 | `	}` |
|        3 | 15875 | `	return PH7_OK;` |
|        1 | 15876 |  |
|        - | 15877 | `/*` |
|        - | 15878 | ` * string get_get_included_files(void)` |
|        - | 15879 | ` *  Gets the current include_path configuration option.` |
|        - | 15880 | ` * Parameter` |
|        - | 15881 | ` *  None` |
|        - | 15882 | ` * Return` |
|        - | 15883 | ` *  Included paths as a string` |
|        - | 15884 | ` */` |
|        2 | 15885 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15886 |  |
|        3 | 15887 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15888 | `	ph7_value *pArray,*pWorker;` |
|        - | 15889 | `	SyString *pEntry;` |
|        - | 15890 | `	int c,d;` |
|        - | 15891 | `	/* Create an array and a working value */` |
|        3 | 15892 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15893 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15894 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15895 | `		/* Out of memory,return null */` |
|      ! 0 | 15896 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15897 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15898 | `		SXUNUSED(apArg);` |
|      ! 0 | 15899 | `		return PH7_OK;` |
|        - | 15900 | `	}` |
|        3 | 15901 | `	c = d = '/';` |
|        - | 15902 | `#ifdef __WINNT__` |
|        1 | 15903 | `	d = '\\';` |
|        - | 15904 | `#endif` |
|        - | 15905 | `	/* Iterate throw entries */` |
|        3 | 15906 | `	SySetResetCursor(pFiles);` |
|     3917 | 15907 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15908 | `		const char *zBase,*zEnd;` |
|        - | 15909 | `		int iLen;` |
|        - | 15910 | `		/* reset the string cursor */` |
|     3915 | 15911 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15912 | `		/* Extract base name */` |
|     3915 | 15913 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15914 | `		/* Ignore trailing '/' */` |
|     5872 | 15915 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15916 | `			zEnd--;` |
|      ! 0 | 15917 | `		}` |
|     3915 | 15918 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15919 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15920 | `			zEnd--;` |
|        1 | 15921 | `		}` |
|     3915 | 15922 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15923 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15924 | `		/* Copy entry name */` |
|     3915 | 15925 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15926 | `		/* Perform the insertion */` |
|     3915 | 15927 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15928 | `	}` |
|        - | 15929 | `	/* All done,return the created array */` |
|        3 | 15930 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15931 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15932 | `	 * by the engine as soon we return from this foreign` |
|        - | 15933 | `	 * function.` |
|        - | 15934 | `	 */` |
|        3 | 15935 | `	return PH7_OK;` |
|        2 | 15936 |  |
|        - | 15937 | `/*` |
|        - | 15938 | ` * include:` |
|        - | 15939 | ` * According to the PHP reference manual.` |
|        - | 15940 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15941 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15942 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15943 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15944 | ` *  and the current working directory before failing. The include()` |
|        - | 15945 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15946 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15947 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15948 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15949 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15950 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15951 | ` *  directory to find the requested file.` |
|        - | 15952 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15953 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15954 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15955 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15956 | ` */` |
|     9720 | 15957 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 15958 |  |
|        - | 15959 | `	SyString sFile;` |
|        - | 15960 | `	sxi32 rc;` |
|     9723 | 15961 | `	if( nArg < 1 ){` |
|        - | 15962 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15963 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15964 | `		return SXRET_OK;` |
|        - | 15965 | `	}` |
|        - | 15966 | `	/* File to include */` |
|     9723 | 15967 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9723 | 15968 | `	if( sFile.nByte < 1 ){` |
|        - | 15969 | `		/* Empty string,return NULL */` |
|      ! 0 | 15970 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15971 | `		return SXRET_OK;` |
|        - | 15972 | `	}` |
|        - | 15973 | `	/* Open,compile and execute the desired script */` |
|     9723 | 15974 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9723 | 15975 | `	if( rc != SXRET_OK ){` |
|        - | 15976 | `		/* Emit a warning and return false */` |
|        3 | 15977 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15978 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15979 | `	}` |
|     9723 | 15980 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15981 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 | 15982 | `		return PH7_ABORT;` |
|        - | 15983 | `	}` |
|     9718 | 15984 | `	return SXRET_OK;` |
|     4863 | 15985 |  |
|        - | 15986 | `/*` |
|        - | 15987 | ` * include_once:` |
|        - | 15988 | ` *  According to the PHP reference manual.` |
|        - | 15989 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15990 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15991 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15992 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15993 | ` *   just once.` |
|        - | 15994 | ` */` |
|       10 | 15995 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15996 |  |
|        - | 15997 | `	SyString sFile;` |
|        - | 15998 | `	sxi32 rc;` |
|       11 | 15999 | `	if( nArg < 1 ){` |
|        - | 16000 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16001 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16002 | `		return SXRET_OK;` |
|        - | 16003 | `	}` |
|        - | 16004 | `	/* File to include */` |
|       11 | 16005 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 16006 | `	if( sFile.nByte < 1 ){` |
|        - | 16007 | `		/* Empty string,return NULL */` |
|      ! 0 | 16008 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16009 | `		return SXRET_OK;` |
|        - | 16010 | `	}` |
|        - | 16011 | `	/* Open,compile and execute the desired script */` |
|       11 | 16012 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 16013 | `	if( rc == SXERR_EXISTS ){` |
|        - | 16014 | `		/* File already included,return TRUE */` |
|        7 | 16015 | `		ph7_result_bool(pCtx,1);` |
|        7 | 16016 | `		return SXRET_OK;` |
|        - | 16017 | `	}` |
|        5 | 16018 | `	if( rc != SXRET_OK ){` |
|        - | 16019 | `		/* Emit a warning and return false */` |
|      ! 0 | 16020 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 16021 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16022 | ` 	}` |
|        5 | 16023 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16024 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 16025 | `		return PH7_ABORT;` |
|        - | 16026 | `	}` |
|        5 | 16027 | `	return SXRET_OK;` |
|        6 | 16028 |  |
|        - | 16029 | `/*` |
|        - | 16030 | ` * require.` |
|        - | 16031 | ` *  According to the PHP reference manual.` |
|        - | 16032 | ` *   require() is identical to include() except upon failure it will` |
|        - | 16033 | ` *   also produce a fatal level error.` |
|        - | 16034 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 16035 | ` *   emits a warning  which allows the script to continue.` |
|        - | 16036 | ` */` |
|        6 | 16037 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 16038 |  |
|        - | 16039 | `	SyString sFile;` |
|        - | 16040 | `	sxi32 rc;` |
|        8 | 16041 | `	if( nArg < 1 ){` |
|        - | 16042 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16043 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16044 | `		return SXRET_OK;` |
|        - | 16045 | `	}` |
|        - | 16046 | `	/* File to include */` |
|        8 | 16047 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 16048 | `	if( sFile.nByte < 1 ){` |
|        - | 16049 | `		/* Empty string,return NULL */` |
|      ! 0 | 16050 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16051 | `		return SXRET_OK;` |
|        - | 16052 | `	}` |
|        - | 16053 | `	/* Open,compile and execute the desired script */` |
|        8 | 16054 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 16055 | `	if( rc != SXRET_OK ){` |
|        - | 16056 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 16057 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 16058 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16059 | `		return PH7_ABORT;` |
|        - | 16060 | `	}` |
|        8 | 16061 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16062 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 16063 | `		return PH7_ABORT;` |
|        - | 16064 | `	}` |
|        8 | 16065 | `	return SXRET_OK;` |
|        5 | 16066 |  |
|        - | 16067 | `/*` |
|        - | 16068 | ` * require_once:` |
|        - | 16069 | ` *  According to the PHP reference manual.` |
|        - | 16070 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 16071 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 16072 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 16073 | ` *   and how it differs from its non _once siblings.` |
|        - | 16074 | ` */` |
|        4 | 16075 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16076 |  |
|        - | 16077 | `	SyString sFile;` |
|        - | 16078 | `	sxi32 rc;` |
|        5 | 16079 | `	if( nArg < 1 ){` |
|        - | 16080 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 16081 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16082 | `		return SXRET_OK;` |
|        - | 16083 | `	}` |
|        - | 16084 | `	/* File to include */` |
|        5 | 16085 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 16086 | `	if( sFile.nByte < 1 ){` |
|        - | 16087 | `		/* Empty string,return NULL */` |
|      ! 0 | 16088 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16089 | `		return SXRET_OK;` |
|        - | 16090 | `	}` |
|        - | 16091 | `	/* Open,compile and execute the desired script */` |
|        5 | 16092 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 16093 | `	if( rc == SXERR_EXISTS ){` |
|        - | 16094 | `		/* File already included,return TRUE */` |
|        3 | 16095 | `		ph7_result_bool(pCtx,1);` |
|        3 | 16096 | `		return SXRET_OK;` |
|        - | 16097 | `	}` |
|        3 | 16098 | `	if( rc != SXRET_OK ){` |
|        - | 16099 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 16100 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 16101 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16102 | `		return PH7_ABORT;` |
|        - | 16103 | `	}` |
|        3 | 16104 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 16105 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 16106 | `		return PH7_ABORT;` |
|        - | 16107 | `	}` |
|        3 | 16108 | `	return SXRET_OK;` |
|        3 | 16109 |  |
|        - | 16110 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 16111 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 16112 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 16113 | `/*` |
|        - | 16114 | ` * Section:` |
|        - | 16115 | ` *  SPL Autoloading functions.` |
|        - | 16116 | ` * Status:` |
|        - | 16117 | ` *  Stable.` |
|        - | 16118 | ` */` |
|        - | 16119 | `/*` |
|        - | 16120 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 16121 | ` *  Register given function as __autoload() implementation.` |
|        - | 16122 | ` * Parameters` |
|        - | 16123 | ` *  callback` |
|        - | 16124 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 16125 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 16126 | ` *  throw` |
|        - | 16127 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 16128 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 16129 | ` *  prepend` |
|        - | 16130 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 16131 | ` *   autoload stack instead of appending it.` |
|        - | 16132 | ` * Return` |
|        - | 16133 | ` *  TRUE on success, FALSE on failure.` |
|        - | 16134 | ` */` |
|       34 | 16135 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 16136 |  |
|        - | 16137 | `	VmAutoloadCB sEntry;` |
|       39 | 16138 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 | 16139 | `	int iPrepend = 0;` |
|        - | 16140 | `	sxu32 n;` |
|       39 | 16141 | `	if( nArg < 1 ){` |
|        - | 16142 | `		/* No callback provided — register default spl_autoload.` |
|        - | 16143 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 16144 | `		/* Check for duplicates first */` |
|        9 | 16145 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 16146 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 16147 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 16148 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 16149 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 16150 | `				ph7_result_bool(pCtx,1);` |
|        5 | 16151 | `				return SXRET_OK;` |
|        - | 16152 | `			}` |
|      ! 0 | 16153 | `		}` |
|        5 | 16154 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 16155 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 16156 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 16157 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 16158 | `		ph7_result_bool(pCtx,1);` |
|        5 | 16159 | `		return SXRET_OK;` |
|        - | 16160 | `	}` |
|        - | 16161 | `	/* Validate that the callback is callable */` |
|       31 | 16162 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 16163 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 16164 | `		if( nArg >= 2 ){` |
|      ! 0 | 16165 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 16166 | `		}` |
|      ! 0 | 16167 | `		if( iThrow ){` |
|      ! 0 | 16168 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 16169 | `				"Argument is not callable");` |
|      ! 0 | 16170 | `		}` |
|      ! 0 | 16171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16172 | `		return SXRET_OK;` |
|        - | 16173 | `	}` |
|        - | 16174 | `	/* Check for duplicates */` |
|       49 | 16175 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 16176 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 16177 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 16178 | `			/* Already registered */` |
|      ! 0 | 16179 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 16180 | `			return SXRET_OK;` |
|        - | 16181 | `		}` |
|       11 | 16182 | `	}` |
|        - | 16183 | `	/* Check prepend flag */` |
|       31 | 16184 | `	if( nArg >= 3 ){` |
|        3 | 16185 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 16186 | `	}` |
|        - | 16187 | `	/* Store the callback */` |
|       31 | 16188 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       31 | 16189 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       31 | 16190 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       32 | 16191 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 16192 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 16193 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 16194 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 16195 | `		VmAutoloadCB *aBase;` |
|        3 | 16196 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 16197 | `		/* Rotate: move last entry to front */` |
|        3 | 16198 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 16199 | `		if( aBase ){` |
|        - | 16200 | `			VmAutoloadCB sTemp;` |
|        - | 16201 | `			sxu32 i;` |
|        3 | 16202 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 16203 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 16204 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 16205 | `			}` |
|        3 | 16206 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 16207 | `		}` |
|        2 | 16208 | `	}else{` |
|       29 | 16209 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 16210 | `	}` |
|       31 | 16211 | `	ph7_result_bool(pCtx,1);` |
|       31 | 16212 | `	return SXRET_OK;` |
|       22 | 16213 |  |
|        - | 16214 | `/*` |
|        - | 16215 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 16216 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 16217 | ` * Parameters` |
|        - | 16218 | ` *  callback` |
|        - | 16219 | ` *   The autoload function being unregistered.` |
|        - | 16220 | ` * Return` |
|        - | 16221 | ` *  TRUE on success, FALSE on failure.` |
|        - | 16222 | ` */` |
|       32 | 16223 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 16224 |  |
|       37 | 16225 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16226 | `	sxu32 n,nEntry;` |
|       37 | 16227 | `	if( nArg < 1 ){` |
|      ! 0 | 16228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16229 | `		return SXRET_OK;` |
|        - | 16230 | `	}` |
|       37 | 16231 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 | 16232 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 | 16233 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 | 16234 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 16235 | `			/* Found — remove by shifting remaining entries down */` |
|       35 | 16236 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 16237 | `			sxu32 i;` |
|       35 | 16238 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 | 16239 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 16240 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 16241 | `			}` |
|        - | 16242 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 | 16243 | `			SySetPop(&pVm->aAutoload);` |
|       35 | 16244 | `			ph7_result_bool(pCtx,1);` |
|       35 | 16245 | `			return SXRET_OK;` |
|        - | 16246 | `		}` |
|        3 | 16247 | `	}` |
|        3 | 16248 | `	ph7_result_bool(pCtx,0);` |
|        3 | 16249 | `	return SXRET_OK;` |
|       21 | 16250 |  |
|        - | 16251 | `/*` |
|        - | 16252 | ` * array spl_autoload_functions(void)` |
|        - | 16253 | ` *  Return all registered __autoload() functions.` |
|        - | 16254 | ` * Return` |
|        - | 16255 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 16256 | ` *  an empty array is returned.` |
|        - | 16257 | ` */` |
|       20 | 16258 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16259 |  |
|       21 | 16260 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16261 | `	ph7_value *pArray;` |
|        - | 16262 | `	sxu32 n,nEntry;` |
|       10 | 16263 | `	SXUNUSED(nArg);` |
|       10 | 16264 | `	SXUNUSED(apArg);` |
|       21 | 16265 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 16266 | `	if( pArray == 0 ){` |
|      ! 0 | 16267 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16268 | `		return SXRET_OK;` |
|        - | 16269 | `	}` |
|       21 | 16270 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 16271 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 16272 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 16273 | `		if( pEntry ){` |
|       15 | 16274 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 16275 | `		}` |
|        8 | 16276 | `	}` |
|       21 | 16277 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 16278 | `	return SXRET_OK;` |
|       11 | 16279 |  |
|        - | 16280 | `/*` |
|        - | 16281 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 16282 | ` *  Default implementation of __autoload().` |
|        - | 16283 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 16284 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 16285 | ` * Parameters` |
|        - | 16286 | ` *  class` |
|        - | 16287 | ` *   The class name being searched.` |
|        - | 16288 | ` *  file_extensions` |
|        - | 16289 | ` *   Comma-separated list of file extensions to try.` |
|        - | 16290 | ` */` |
|        2 | 16291 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16292 |  |
|        - | 16293 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 16294 | `	SyBlob sPath;` |
|        - | 16295 | `	int nClass;` |
|        - | 16296 | `	sxi32 rc;` |
|        3 | 16297 | `	if( nArg < 1 ){` |
|      ! 0 | 16298 | `		return SXRET_OK;` |
|        - | 16299 | `	}` |
|        3 | 16300 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 16301 | `	if( nClass < 1 ){` |
|      ! 0 | 16302 | `		return SXRET_OK;` |
|        - | 16303 | `	}` |
|        - | 16304 | `	/* Default extensions */` |
|        3 | 16305 | `	zExt = ".php,.inc";` |
|        3 | 16306 | `	if( nArg >= 2 ){` |
|        - | 16307 | `		int nExt;` |
|      ! 0 | 16308 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 16309 | `		if( nExt < 1 ){` |
|      ! 0 | 16310 | `			zExt = ".php,.inc";` |
|      ! 0 | 16311 | `		}` |
|      ! 0 | 16312 | `	}` |
|        3 | 16313 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 16314 | `	/* Iterate over comma-separated extensions */` |
|        3 | 16315 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 16316 | `	zCur = zExt;` |
|        7 | 16317 | `	while( zCur < zEnd ){` |
|        - | 16318 | `		const char *zComma;` |
|        - | 16319 | `		SyString sFile;` |
|        - | 16320 | `		int i;` |
|        - | 16321 | `		/* Find next comma or end */` |
|        5 | 16322 | `		zComma = zCur;` |
|       21 | 16323 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 16324 | `			zComma++;` |
|        1 | 16325 | `		}` |
|        - | 16326 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 16327 | `		SyBlobReset(&sPath);` |
|       69 | 16328 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 16329 | `			char c = zClass[i];` |
|       65 | 16330 | `			if( c == '\\' ){` |
|      ! 0 | 16331 | `				c = '/';` |
|       65 | 16332 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 16333 | `				c = c + ('a' - 'A');` |
|        6 | 16334 | `			}` |
|       65 | 16335 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 16336 | `		}` |
|        - | 16337 | `		/* Append extension */` |
|        5 | 16338 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 16339 | `		/* Try to include the file */` |
|        5 | 16340 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 16341 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 16342 | `		if( rc == SXRET_OK ){` |
|        - | 16343 | `			/* File included successfully */` |
|      ! 0 | 16344 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 16345 | `			return SXRET_OK;` |
|        - | 16346 | `		}` |
|        - | 16347 | `		/* Move past the comma */` |
|        5 | 16348 | `		zCur = zComma;` |
|        5 | 16349 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 16350 | `			zCur++;` |
|        1 | 16351 | `		}` |
|        1 | 16352 | `	}` |
|        3 | 16353 | `	SyBlobRelease(&sPath);` |
|        3 | 16354 | `	return SXRET_OK;` |
|        2 | 16355 |  |
|        - | 16356 | `/* Table of built-in VM functions. */` |
|        - | 16357 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 16358 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 16359 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 16360 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 16361 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 16362 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 16363 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 16364 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 16365 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 16366 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 16367 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 16368 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 16369 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 16370 | `	    /* Constants management */` |
|        - | 16371 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 16372 | `	{ "define",   vm_builtin_define               },` |
|        - | 16373 | `	{ "constant", vm_builtin_constant             },` |
|        - | 16374 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 16375 | `	   /* Class/Object functions */` |
|        - | 16376 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 16377 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 16378 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 16379 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 16380 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 16381 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 16382 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 16383 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 16384 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 16385 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 16386 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 16387 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 16388 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 16389 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 16390 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 16391 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 16392 | `	   /* SPL Autoloading */` |
|        - | 16393 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 16394 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 16395 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 16396 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 16397 | `	   /* Random numbers/strings generators */` |
|        - | 16398 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 16399 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 16400 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 16401 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 16402 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 16403 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 16404 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 16405 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16406 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 16407 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 16408 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 16409 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16410 | `	   /* Language constructs functions */` |
|        - | 16411 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 16412 | `	{ "print", vm_builtin_print                   },` |
|        - | 16413 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 16414 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 16415 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 16416 | `	  /* Variable handling functions */` |
|        - | 16417 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 16418 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 16419 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 16420 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 16421 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 16422 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 16423 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 16424 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 16425 | `	  /* Ouput control functions */` |
|        - | 16426 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 16427 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 16428 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 16429 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 16430 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 16431 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 16432 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 16433 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 16434 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 16435 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 16436 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 16437 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 16438 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 16439 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 16440 | `	  /* Assertion functions */` |
|        - | 16441 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 16442 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 16443 | `	  /* Error reporting functions */` |
|        - | 16444 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 16445 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 16446 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 16447 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 16448 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 16449 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 16450 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 16451 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 16452 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 16453 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 16454 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 16455 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 16456 | `	  /* Release info */` |
|        - | 16457 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 16458 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 16459 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 16460 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 16461 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 16462 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 16463 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 16464 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 16465 | `	  /* hashmap */` |
|        - | 16466 | `	{"compact",          vm_builtin_compact       },` |
|        - | 16467 | `	{"extract",          vm_builtin_extract       },` |
|        - | 16468 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 16469 | `	  /* URL related function */` |
|        - | 16470 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 16471 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 16472 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16473 | `	   /* XML processing functions */` |
|        - | 16474 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 16475 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 16476 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 16477 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 16478 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 16479 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 16480 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 16481 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 16482 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 16483 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 16484 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 16485 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 16486 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 16487 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 16488 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 16489 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 16490 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 16491 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 16492 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 16493 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 16494 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 16495 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16496 | `	   /* UTF-8 encoding/decoding */` |
|        - | 16497 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 16498 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 16499 | `	   /* Command line processing */` |
|        - | 16500 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 16501 | `	   /* JSON encoding/decoding */` |
|        - | 16502 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 16503 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 16504 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 16505 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 16506 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 16507 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 16508 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 16509 | `	   /* Files/URI inclusion facility */` |
|        - | 16510 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 16511 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 16512 | `	{ "include",      vm_builtin_include          },` |
|        - | 16513 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 16514 | `	{ "require",      vm_builtin_require          },` |
|        - | 16515 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 16516 | `};` |
|        - | 16517 | `/*` |
|        - | 16518 | ` * Register the built-in VM functions defined above.` |
|        - | 16519 | ` */` |
|     2846 | 16520 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 16521 |  |
|        - | 16522 | `	sxi32 rc;` |
|        - | 16523 | `	sxu32 n;` |
|   384215 | 16524 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 16525 | `		/* Note that these special functions have access` |
|        - | 16526 | `		 * to the underlying virtual machine as their` |
|        - | 16527 | `		 * private data.` |
|        - | 16528 | `		 */` |
|   381369 | 16529 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   381369 | 16530 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 16531 | `			return rc;` |
|        - | 16532 | `		}` |
|   190687 | 16533 | `	}` |
|     2851 | 16534 | `	return SXRET_OK;` |
|     1428 | 16535 |  |
|        - | 16536 | `/*` |
|        - | 16537 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 16538 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 16539 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 16540 | ` */` |
|   187210 | 16541 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 16542 |  |
|   187215 | 16543 | `	if( !iLoadable ){` |
|   185037 | 16544 | `		return pClass;` |
|        - | 16545 | `	}` |
|     2189 | 16546 | `	while(pClass){` |
|     2183 | 16547 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2177 | 16548 | `			return pClass;` |
|        - | 16549 | `		}` |
|        7 | 16550 | `		pClass = pClass->pNextName;` |
|        1 | 16551 | `	}` |
|        7 | 16552 | `	return 0;` |
|    93610 | 16553 |  |
|        - | 16554 | `/*` |
|        - | 16555 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 16556 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 16557 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 16558 | ` * registered in the VM's class table.` |
|        - | 16559 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 16560 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 16561 | ` */` |
|       38 | 16562 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 16563 |  |
|        - | 16564 | `	VmAutoloadCB *pEntry;` |
|        - | 16565 | `	ph7_value sArg,sResult;` |
|        - | 16566 | `	SyHashEntry *pHashEntry;` |
|        - | 16567 | `	ph7_class *pClass;` |
|        - | 16568 | `	sxu32 n,nEntry;` |
|       43 | 16569 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       43 | 16570 | `	if( nEntry < 1 ){` |
|       28 | 16571 | `		return 0;` |
|        - | 16572 | `	}` |
|        - | 16573 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       19 | 16574 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 16575 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 16576 | `	}` |
|        - | 16577 | `	/* Mark this class as being autoloaded */` |
|       17 | 16578 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 16579 | `	/* Prepare the class name argument */` |
|       17 | 16580 | `	PH7_MemObjInit(pVm,&sArg);` |
|       17 | 16581 | `	PH7_MemObjInit(pVm,&sResult);` |
|       17 | 16582 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       17 | 16583 | `	pClass = 0;` |
|       31 | 16584 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 16585 | `		ph7_value *apArg[1];` |
|       27 | 16586 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       27 | 16587 | `		if( pEntry == 0 ){` |
|      ! 0 | 16588 | `			continue;` |
|        - | 16589 | `		}` |
|       27 | 16590 | `		apArg[0] = &sArg;` |
|       27 | 16591 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 16592 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 16593 | `			continue;` |
|        - | 16594 | `		}` |
|        - | 16595 | `		/* Check if the class is now available */` |
|       27 | 16596 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       27 | 16597 | `		if( pHashEntry ){` |
|       12 | 16598 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       12 | 16599 | `			if( pClass ){` |
|       12 | 16600 | `				break;` |
|        - | 16601 | `			}` |
|      ! 0 | 16602 | `		}` |
|       10 | 16603 | `	}` |
|       17 | 16604 | `	PH7_MemObjRelease(&sArg);` |
|       17 | 16605 | `	PH7_MemObjRelease(&sResult);` |
|        - | 16606 | `	/* Remove reentrancy guard */` |
|       17 | 16607 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       17 | 16608 | `	return pClass;` |
|       24 | 16609 |  |
|        - | 16610 | `/*` |
|        - | 16611 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 16612 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 16613 | ` */` |
|       18 | 16614 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 16615 |  |
|       23 | 16616 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 | 16617 |  |
|        - | 16618 | `/*` |
|        - | 16619 | ` * Check if the given name refer to an installed class.` |
|        - | 16620 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 16621 | ` */` |
|   187222 | 16622 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 16623 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 16624 | `	const char *zName,  /* Name of the target class */` |
|        - | 16625 | `	sxu32 nByte,        /* zName length */` |
|        - | 16626 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 16627 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 16628 | `						 */` |
|        - | 16629 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 16630 | `	)` |
|        5 | 16631 |  |
|        - | 16632 | `	SyHashEntry *pEntry;` |
|        - | 16633 | `	ph7_class *pClass;` |
|    93611 | 16634 | `	SXUNUSED(iNest);` |
|        - | 16635 | `	/* Exact class lookup.` |
|        - | 16636 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16637 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   187227 | 16638 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   187227 | 16639 | `	if( pEntry == 0 ){` |
|        - | 16640 | `		/* Class not found in hash table — try autoload before giving up */` |
|       23 | 16641 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16642 | `	}` |
|   187207 | 16643 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   187207 | 16644 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    93616 | 16645 |  |
|        - | 16646 | `/*` |
|        - | 16647 | ` * Reference Table Implementation` |
|        - | 16648 | ` * Status: stable <chm@symisc.net>` |
|        - | 16649 | ` * Intro` |
|        - | 16650 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16651 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16652 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16653 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16654 | ` *  Refer to the official for more information on this powerful` |
|        - | 16655 | ` *  extension.` |
|        - | 16656 | ` */` |
|        - | 16657 | `/*` |
|        - | 16658 | ` * Allocate a new reference entry.` |
|        - | 16659 | ` */` |
|  3220732 | 16660 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 16661 |  |
|        - | 16662 | `	VmRefObj *pRef;` |
|        - | 16663 | `	/* Allocate a new instance */` |
|  3220737 | 16664 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3220737 | 16665 | `	if( pRef == 0 ){` |
|      ! 0 | 16666 | `		return 0;` |
|        - | 16667 | `	}` |
|        - | 16668 | `	/* Zero the structure */` |
|  3220737 | 16669 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16670 | `	/* Initialize fields */` |
|  3220737 | 16671 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3220737 | 16672 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3220737 | 16673 | `	pRef->nIdx = nIdx;` |
|  3220737 | 16674 | `	return pRef;` |
|  1610371 | 16675 |  |
|        - | 16676 | `/*` |
|        - | 16677 | ` * Default hash function used by the reference table` |
|        - | 16678 | ` * for lookup/insertion operations.` |
|        - | 16679 | ` */` |
| 17616177 | 16680 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 16681 |  |
|        - | 16682 | `	/* Calculate the hash based on the memory object index */` |
| 17616182 | 16683 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 16684 |  |
|        - | 16685 | `/*` |
|        - | 16686 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16687 | ` * in the reference table.` |
|        - | 16688 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16689 | ` * otherwise.` |
|        - | 16690 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16691 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16692 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16693 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16694 | ` * Refer to the official for more information on this powerful` |
|        - | 16695 | ` * extension.` |
|        - | 16696 | ` */` |
|  9595954 | 16697 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 16698 |  |
|        - | 16699 | `	VmRefObj *pRef;` |
|        - | 16700 | `	sxu32 nBucket;` |
|        - | 16701 | `	/* Point to the appropriate bucket */` |
|  9595959 | 16702 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16703 | `	/* Perform the lookup */` |
|  9595959 | 16704 | `	pRef = pVm->apRefObj[nBucket];` |
| 21085786 | 16705 | `	for(;;){` |
| 42163083 | 16706 | `		if( pRef == 0 ){` |
|  3328685 | 16707 | `			break;` |
|        - | 16708 | `		}` |
| 38834403 | 16709 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16710 | `			/* Entry found */` |
|  6267279 | 16711 | `			return pRef;` |
|        - | 16712 | `		}` |
|        - | 16713 | `		/* Point to the next entry */` |
| 32567129 | 16714 | `		pRef = pRef->pNextCollide;` |
|        5 | 16715 | `	}` |
|        - | 16716 | `	/* No such entry,return NULL */` |
|  3328685 | 16717 | `	return 0;` |
|  4797982 | 16718 |  |
|        - | 16719 | `/*` |
|        - | 16720 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16721 | ` *` |
|        - | 16722 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16723 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16724 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16725 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16726 | ` * Refer to the official for more information on this powerful` |
|        - | 16727 | ` * extension.` |
|        - | 16728 | ` */` |
|  3220732 | 16729 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 16730 |  |
|        - | 16731 | `	sxu32 nBucket;` |
|  3220737 | 16732 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16733 | `		VmRefObj **apNew;` |
|        - | 16734 | `		sxu32 nNew;` |
|        - | 16735 | `		/* Allocate a larger table */` |
|     4511 | 16736 | `		nNew = pVm->nRefSize << 1;` |
|     4511 | 16737 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4511 | 16738 | `		if( apNew ){` |
|     4511 | 16739 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16740 | `			sxu32 n;` |
|        - | 16741 | `			/* Zero the structure */` |
|     4511 | 16742 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16743 | `			/* Rehash all referenced entries */` |
|  2848429 | 16744 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16745 | `				/* Remove old collision links */` |
|  2843923 | 16746 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16747 | `				/* Point to the appropriate bucket */` |
|  2843923 | 16748 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16749 | `				/* Insert the entry  */` |
|  2843923 | 16750 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843923 | 16751 | `				if( apNew[nBucket] ){` |
|  2301119 | 16752 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16753 | `				}` |
|  2843923 | 16754 | `				apNew[nBucket] = pEntry;` |
|        - | 16755 | `				/* Point to the next entry */` |
|  2843923 | 16756 | `				pEntry = pEntry->pNext;` |
|  1421964 | 16757 | `			}` |
|        - | 16758 | `			/* Release the old table */` |
|     4511 | 16759 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16760 | `			/* Install the new one */` |
|     4511 | 16761 | `			pVm->apRefObj = apNew;` |
|     4511 | 16762 | `			pVm->nRefSize = nNew;` |
|     2253 | 16763 | `		}` |
|     2253 | 16764 | `	}` |
|        - | 16765 | `	/* Point to the appropriate bucket */` |
|  3220737 | 16766 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16767 | `	/* Insert the entry */` |
|  3220737 | 16768 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3220737 | 16769 | `	if( pVm->apRefObj[nBucket] ){` |
|  2623698 | 16770 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1311849 | 16771 | `	}` |
|  3220737 | 16772 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3220737 | 16773 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3220737 | 16774 | `	pVm->nRefUsed++;` |
|  3220737 | 16775 | `	return SXRET_OK;` |
|        5 | 16776 |  |
|        - | 16777 | `/*` |
|        - | 16778 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16779 | ` * the reference table.` |
|        - | 16780 | ` * This function is invoked when the user perform an unset` |
|        - | 16781 | ` * call [i.e: unset($var); ].` |
|        - | 16782 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16783 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16784 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16785 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16786 | ` * Refer to the official for more information on this powerful` |
|        - | 16787 | ` * extension.` |
|        - | 16788 | ` */` |
|  3176272 | 16789 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 16790 |  |
|        - | 16791 | `	ph7_hashmap_node **apNode;` |
|        - | 16792 | `	SyHashEntry **apEntry;` |
|        - | 16793 | `	sxu32 n;` |
|        - | 16794 | `	/* Point to the reference table */` |
|  3176277 | 16795 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3176277 | 16796 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16797 | `	/* Unlink the entry from the reference table */` |
|  3290229 | 16798 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   113957 | 16799 | `		if( apEntry[n] ){` |
|   113907 | 16800 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    56951 | 16801 | `		}` |
|    56981 | 16802 | `	}` |
|  6238573 | 16803 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3062301 | 16804 | `		if( apNode[n] ){` |
|     7080 | 16805 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3538 | 16806 | `		}` |
|  1531153 | 16807 | `	}` |
|  3176277 | 16808 | `	if( pRef->pPrevCollide ){` |
|  1220704 | 16809 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   610623 | 16810 | `	}else{` |
|  1955578 | 16811 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16812 | `	}` |
|  3176277 | 16813 | `	if( pRef->pNextCollide ){` |
|  1810695 | 16814 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   905340 | 16815 | `	}` |
|  3176277 | 16816 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16817 | `	/* Release the node */` |
|  3176277 | 16818 | `	SySetRelease(&pRef->aReference);` |
|  3176277 | 16819 | `	SySetRelease(&pRef->aArrEntries);` |
|  3176277 | 16820 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3176277 | 16821 | `	pVm->nRefUsed--;` |
|  3176277 | 16822 | `	return SXRET_OK;` |
|        5 | 16823 |  |
|        - | 16824 | `/*` |
|        - | 16825 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16826 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16827 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16828 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16829 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16830 | ` * Refer to the official for more information on this powerful` |
|        - | 16831 | ` * extension.` |
|        - | 16832 | ` */` |
|  3256634 | 16833 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16834 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16835 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16836 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16837 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16838 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16839 | `	)` |
|        5 | 16840 |  |
|  3256639 | 16841 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16842 | `	VmRefObj *pRef;` |
|        - | 16843 | `	/* Check if the referenced object already exists */` |
|  3256639 | 16844 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3256639 | 16845 | `	if( pRef == 0 ){` |
|        - | 16846 | `		/* Create a new entry */` |
|  3220737 | 16847 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3220737 | 16848 | `		if( pRef == 0 ){` |
|      ! 0 | 16849 | `			return SXERR_MEM;` |
|        - | 16850 | `		}` |
|  3220737 | 16851 | `		pRef->iFlags = iFlags;` |
|        - | 16852 | `		/* Install the entry */` |
|  3220737 | 16853 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1610366 | 16854 | `	}` |
|  3256639 | 16855 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3256639 | 16856 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16857 | `		VmSlot sRef;` |
|        - | 16858 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16859 | `		 * be deleted when we leave this frame.` |
|        - | 16860 | `		 */` |
|   108063 | 16861 | `		sRef.nIdx = nIdx;` |
|   108063 | 16862 | `		sRef.pUserData = pEntry;` |
|   108063 | 16863 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16864 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16865 | `		}` |
|    54029 | 16866 | `	}` |
|  3256639 | 16867 | `	if( pEntry ){` |
|        - | 16868 | `		/* Address of the hash-entry */` |
|   143737 | 16869 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    71866 | 16870 | `	}` |
|  3256639 | 16871 | `	if( pMapEntry ){` |
|        - | 16872 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3101913 | 16873 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1550954 | 16874 | `	}` |
|  3256639 | 16875 | `	return SXRET_OK;` |
|  1628322 | 16876 |  |
|        - | 16877 | `/*` |
|        - | 16878 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16879 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16880 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16881 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16882 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16883 | ` * Refer to the official for more information on this powerful` |
|        - | 16884 | ` * extension.` |
|        - | 16885 | ` */` |
|  3163240 | 16886 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16887 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16888 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16889 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16890 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16891 | `	)` |
|        5 | 16892 |  |
|        - | 16893 | `	VmRefObj *pRef;` |
|        - | 16894 | `	sxu32 n;` |
|        - | 16895 | `	/* Check if the referenced object already exists */` |
|  3163245 | 16896 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3163245 | 16897 | `	if( pRef == 0 ){` |
|        - | 16898 | `		/* Not such entry */` |
|   107947 | 16899 | `		return SXERR_NOTFOUND;` |
|        - | 16900 | `	}` |
|        - | 16901 | `	/* Remove the desired entry */` |
|  3055303 | 16902 | `	if( pEntry ){` |
|        - | 16903 | `		SyHashEntry **apEntry;` |
|       77 | 16904 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 16905 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      195 | 16906 | `			if( apEntry[n] == pEntry ){` |
|        - | 16907 | `				/* Nullify the entry */` |
|       77 | 16908 | `				apEntry[n] = 0;` |
|        - | 16909 | `				/*` |
|        - | 16910 | `				 * NOTE:` |
|        - | 16911 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16912 | `				 * we avoid wasting spaces.` |
|        - | 16913 | `				 */` |
|       36 | 16914 | `			}` |
|      100 | 16915 | `		}` |
|       36 | 16916 | `	}` |
|  3055303 | 16917 | `	if( pMapEntry ){` |
|        - | 16918 | `		ph7_hashmap_node **apNode;` |
|  3055231 | 16919 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6110551 | 16920 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3055325 | 16921 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16922 | `				/* nullify the entry */` |
|  3055231 | 16923 | `				apNode[n] = 0;` |
|  1527613 | 16924 | `			}` |
|  1527665 | 16925 | `		}` |
|  1527613 | 16926 | `	}` |
|  3055303 | 16927 | `	return SXRET_OK;` |
|  1581625 | 16928 |  |
|        - | 16929 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16930 | `/*` |
|        - | 16931 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16932 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16933 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16934 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16935 | ` * For more information on how to register IO stream devices,please` |
|        - | 16936 | ` * refer to the official documentation.` |
|        - | 16937 | ` */` |
|    29562 | 16938 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16939 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16940 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16941 | `	int nByte              /* *pzDevice length*/` |
|        - | 16942 | `	)` |
|        5 | 16943 |  |
|        - | 16944 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16945 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16946 | `	SyString sDev,sCur;` |
|        - | 16947 | `	sxu32 n,nEntry;` |
|        - | 16948 | `	int rc;` |
|        - | 16949 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29567 | 16950 | `	zNext = zCur = zIn = *pzDevice;` |
|    29567 | 16951 | `	zEnd = &zIn[nByte];` |
|  1887149 | 16952 | `	while( zIn < zEnd ){` |
|  1857589 | 16953 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16954 | `			/* Got one */` |
|        3 | 16955 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16956 | `			break;` |
|        - | 16957 | `		}` |
|        - | 16958 | `		/* Advance the cursor */` |
|  1857587 | 16959 | `		zIn++;` |
|        5 | 16960 | `	}` |
|    29567 | 16961 | `	if( zIn >= zEnd ){` |
|        - | 16962 | `		/* No such scheme,return the default stream */` |
|    29565 | 16963 | `		return pVm->pDefStream;` |
|        - | 16964 | `	}` |
|        3 | 16965 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16966 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16967 | `	SyStringFullTrim(&sDev);` |
|        - | 16968 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16969 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16970 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16971 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16972 | `		pStream = apStream[n];` |
|        3 | 16973 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16974 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16975 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16976 | `		if( rc == 0 ){` |
|        - | 16977 | `			/* Stream device found */` |
|        3 | 16978 | `			*pzDevice = zNext;` |
|        3 | 16979 | `			return pStream;` |
|        - | 16980 | `		}` |
|      ! 0 | 16981 | `	}` |
|        - | 16982 | `	/* No such stream,return NULL */` |
|      ! 0 | 16983 | `	return 0;` |
|    14786 | 16984 |  |
|        - | 16985 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16986 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16987 |  |
