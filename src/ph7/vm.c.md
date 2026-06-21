# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6963/8872 lines (78.48%)

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
|   922968 |   140 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        5 |   141 |  |
|   922973 |   142 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   143 | `		return TRUE;` |
|        - |   144 | `	}` |
|   922939 |   145 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   146 | `		return TRUE;` |
|        - |   147 | `	}` |
|   922929 |   148 | `	return FALSE;` |
|   461511 |   149 |  |
|        - |   150 | `/*` |
|        - |   151 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   152 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   153 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   154 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   155 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   156 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   157 | ` * still go through the existing numeric coercion.` |
|        - |   158 | ` */` |
|   336058 |   159 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        5 |   160 |  |
|        - |   161 | `	SyString sStr;` |
|   336063 |   162 | `	sxu8 bReal = FALSE;` |
|   336063 |   163 | `	const char *zTail = 0;` |
|        - |   164 | `	const char *zEnd;` |
|   336063 |   165 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335993 |   166 | `		return FALSE;` |
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
|   168056 |   183 |  |
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
|   634828 |   202 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   634833 |   213 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   634833 |   214 | `	if( pEntry ){` |
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
|   634829 |   230 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   634829 |   231 | `	if( pCons == 0 ){` |
|      ! 0 |   232 | `		return 0;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate constant name */` |
|   634829 |   235 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   634829 |   236 | `	if( zDupName == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return 0;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Install the constant */` |
|   634829 |   241 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   634829 |   242 | `	pCons->xExpand = xExpand;` |
|   634829 |   243 | `	pCons->pUserData = pUserData;` |
|   634829 |   244 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   634829 |   245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   246 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   247 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   248 | `		return rc;` |
|        - |   249 | `	}` |
|        - |   250 | `	/* All done,constant can be invoked from PHP code */` |
|   634829 |   251 | `	return SXRET_OK;` |
|   317419 |   252 |  |
|        - |   253 | `/*` |
|        - |   254 | ` * Allocate a new foreign function instance.` |
|        - |   255 | ` * This function return SXRET_OK on success. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|  1403128 |   260 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1403133 |   271 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1403133 |   272 | `	if( pFunc == 0 ){` |
|      ! 0 |   273 | `		return SXERR_MEM;` |
|        - |   274 | `	}` |
|        - |   275 | `	/* Duplicate function name */` |
|  1403133 |   276 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1403133 |   277 | `	if( zDup == 0 ){` |
|      ! 0 |   278 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   279 | `		return SXERR_MEM;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Zero the structure */` |
|  1403133 |   282 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   283 | `	/* Initialize structure fields */` |
|  1403133 |   284 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1403133 |   285 | `	pFunc->pVm   = pVm;` |
|  1403133 |   286 | `	pFunc->xFunc = xFunc;` |
|  1403133 |   287 | `	pFunc->pUserData = pUserData;` |
|  1403133 |   288 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   289 | `	/* Write a pointer to the new function */` |
|  1403133 |   290 | `	*ppOut = pFunc;` |
|  1403133 |   291 | `	return SXRET_OK;` |
|   701569 |   292 |  |
|        - |   293 | `/*` |
|        - |   294 | ` * Install a foreign function and it's associated callback so that` |
|        - |   295 | ` * it can be invoked from the target PHP code.` |
|        - |   296 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   297 | ` * return value indicates failure.` |
|        - |   298 | ` * Please refer to the official documentation for an introduction to` |
|        - |   299 | ` * the foreign function mechanism.` |
|        - |   300 | ` */` |
|  1405962 |   301 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1405967 |   312 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1405967 |   313 | `	if( pEntry ){` |
|     2839 |   314 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2839 |   315 | `		pFunc->pUserData = pUserData;` |
|     2839 |   316 | `		pFunc->xFunc = xFunc;` |
|     2839 |   317 | `		SySetReset(&pFunc->aAux);` |
|     2839 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* Create a new user function */` |
|  1403133 |   321 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1403133 |   322 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   323 | `		return rc;` |
|        - |   324 | `	}` |
|        - |   325 | `	/* Install the function in the corresponding hashtable */` |
|  1403133 |   326 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1403133 |   327 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   328 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   329 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   330 | `		return rc;` |
|        - |   331 | `	}` |
|        - |   332 | `	/* User function successfully installed */` |
|  1403133 |   333 | `	return SXRET_OK;` |
|   702986 |   334 |  |
|        - |   335 | `/*` |
|        - |   336 | ` * Initialize a VM function.` |
|        - |   337 | ` */` |
|   279080 |   338 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   339 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   340 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   341 | `	const char *zName,  /* Function name */` |
|        - |   342 | `	sxu32 nByte,        /* zName length */` |
|        - |   343 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   344 | `	void *pUserData     /* Function private data */` |
|        - |   345 | `	)` |
|        5 |   346 |  |
|        - |   347 | `	/* Zero the structure */` |
|   279085 |   348 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   349 | `	/* Initialize structure fields */` |
|        - |   350 | `	/* Arguments container */` |
|   279085 |   351 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   352 | `	/* Static variable container */` |
|   279085 |   353 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   354 | `	/* Bytecode container */` |
|   279085 |   355 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   356 | `    /* Preallocate some instruction slots */` |
|   279085 |   357 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   358 | `	/* Closure environment */` |
|   279085 |   359 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   360 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   279085 |   361 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   279085 |   362 | `	pFunc->iFlags = iFlags;` |
|   279085 |   363 | `	pFunc->pUserData = pUserData;` |
|        - |   364 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   365 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   279085 |   366 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   279085 |   367 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   279085 |   368 | `	return SXRET_OK;` |
|        5 |   369 |  |
|        - |   370 | `/*` |
|        - |   371 | ` * Namespace-aware function lookup.` |
|        - |   372 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   373 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   374 | ` */` |
|        - |   375 | `/*` |
|        - |   376 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   377 | ` */` |
|  1461176 |   378 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   379 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   380 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   381 | `	SyString *pName     /* Function name */` |
|        - |   382 | `	)` |
|        5 |   383 |  |
|        - |   384 | `	SyHashEntry *pEntry;` |
|        - |   385 | `	sxi32 rc;` |
|  1461181 |   386 | `	if( pName == 0 ){` |
|        - |   387 | `		/* Use the built-in name */` |
|    42107 |   388 | `		pName = &pFunc->sName;` |
|    21051 |   389 | `	}` |
|        - |   390 | `	/* Check for duplicates (functions with the same name) first */` |
|  1461181 |   391 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  1461181 |   392 | `	if( pEntry ){` |
|  1264235 |   393 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  1264235 |   394 | `		if( pLink != pFunc ){` |
|        - |   395 | `			/* Link */` |
|      188 |   396 | `			pFunc->pNextName = pLink;` |
|      188 |   397 | `			pEntry->pUserData = pFunc;` |
|       93 |   398 | `		}` |
|  1264235 |   399 | `		return SXRET_OK;` |
|        - |   400 | `	}` |
|        - |   401 | `	/* First time seen */` |
|   196951 |   402 | `	pFunc->pNextName = 0;` |
|   196951 |   403 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   196951 |   404 | `	return rc;` |
|   730593 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   408 | ` */` |
|   120658 |   409 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   410 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   411 | `	ph7_class *pClass /* Target Class */` |
|        - |   412 | `	)` |
|        5 |   413 |  |
|   120663 |   414 | `	SyString *pName = &pClass->sName;` |
|        - |   415 | `	SyHashEntry *pEntry;` |
|        - |   416 | `	sxi32 rc;` |
|        - |   417 | `	/* Check for duplicates */` |
|   120663 |   418 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   120663 |   419 | `	if( pEntry ){` |
|       31 |   420 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   421 | `		/* Link entry with the same name */` |
|       31 |   422 | `		pClass->pNextName = pLink;` |
|       31 |   423 | `		pEntry->pUserData = pClass;` |
|       31 |   424 | `		return SXRET_OK;` |
|        - |   425 | `	}` |
|   120633 |   426 | `	pClass->pNextName = 0;` |
|        - |   427 | `	/* Perform a simple hashtable insertion */` |
|   120633 |   428 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   120633 |   429 | `	return rc;` |
|    60334 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Instruction builder interface.` |
|        - |   433 | ` */` |
|  4273092 |   434 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4273097 |   446 | `	sInstr.iOp = (sxu8)iOp;` |
|  4273097 |   447 | `	sInstr.iP1 = iP1;` |
|  4273097 |   448 | `	sInstr.iP2 = iP2;` |
|  4273097 |   449 | `	sInstr.p3  = p3;` |
|  4273097 |   450 | `	if( pIndex ){` |
|        - |   451 | `		/* Instruction index in the bytecode array */` |
|   232115 |   452 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   116055 |   453 | `	}` |
|        - |   454 | `	/* Finally,record the instruction */` |
|  4273097 |   455 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4273097 |   456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   457 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   458 | `		/* Fall throw */` |
|      ! 0 |   459 | `	}` |
|  4273097 |   460 | `	return rc;` |
|        5 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Swap the current bytecode container with the given one.` |
|        - |   464 | ` */` |
|   554464 |   465 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        5 |   466 |  |
|   554469 |   467 | `	if( pContainer == 0 ){` |
|        - |   468 | `		/* Point to the default container */` |
|      ! 0 |   469 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   470 | `	}else{` |
|        - |   471 | `		/* Change container */` |
|   554469 |   472 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   473 | `	}` |
|   554469 |   474 | `	return SXRET_OK;` |
|        5 |   475 |  |
|        - |   476 | `/*` |
|        - |   477 | ` * Return the current bytecode container.` |
|        - |   478 | ` */` |
|   277232 |   479 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        5 |   480 |  |
|   277237 |   481 | `	return pVm->pByteContainer;` |
|        5 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   485 | ` */` |
|   228886 |   486 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        5 |   487 |  |
|        - |   488 | `	VmInstr *pInstr;` |
|   228891 |   489 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   228891 |   490 | `	return pInstr;` |
|        5 |   491 |  |
|        - |   492 | `/*` |
|        - |   493 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   494 | ` */` |
|  1283448 |   495 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        5 |   496 |  |
|  1283453 |   497 | `	return SySetUsed(pVm->pByteContainer);` |
|        5 |   498 |  |
|        - |   499 | `/*` |
|        - |   500 | ` * Pop the last VM instruction.` |
|        - |   501 | ` */` |
|   211666 |   502 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        5 |   503 |  |
|   211671 |   504 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        5 |   505 |  |
|        - |   506 | `/*` |
|        - |   507 | ` * Peek the last VM instruction.` |
|        - |   508 | ` */` |
|   841356 |   509 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        5 |   510 |  |
|   841361 |   511 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        5 |   512 |  |
|    33700 |   513 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        5 |   514 |  |
|        - |   515 | `	VmInstr *aInstr;` |
|        - |   516 | `	sxu32 n;` |
|    33705 |   517 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33705 |   518 | `	if( n < 2 ){` |
|      ! 0 |   519 | `		return 0;` |
|        - |   520 | `	}` |
|    33705 |   521 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33705 |   522 | `	return &aInstr[n - 2];` |
|    16855 |   523 |  |
|        - |   524 | `/*` |
|        - |   525 | ` * Allocate a new virtual machine frame.` |
|        - |   526 | ` */` |
|    23308 |   527 | `static VmFrame * VmNewFrame(` |
|        - |   528 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   529 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   530 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   531 | `	)` |
|        5 |   532 |  |
|        - |   533 | `	VmFrame *pFrame;` |
|        - |   534 | `	/* Allocate a new vm frame */` |
|    23313 |   535 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    23313 |   536 | `	if( pFrame == 0 ){` |
|      ! 0 |   537 | `		return 0;` |
|        - |   538 | `	}` |
|        - |   539 | `	/* Zero the structure */` |
|    23313 |   540 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   541 | `	/* Initialize frame fields */` |
|    23313 |   542 | `	pFrame->pUserData = pUserData;` |
|    23313 |   543 | `	pFrame->pThis = pThis;` |
|    23313 |   544 | `	pFrame->pVm = pVm;` |
|    23313 |   545 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    23313 |   546 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    23313 |   547 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    23313 |   548 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    23313 |   549 | `	return pFrame;` |
|    11659 |   550 |  |
|        - |   551 | `/* Forward declaration */` |
|        - |   552 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   553 | `/*` |
|        - |   554 | ` * Enter a VM frame.` |
|        - |   555 | ` */` |
|    23236 |   556 | `static sxi32 VmEnterFrame(` |
|        - |   557 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   558 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   559 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   560 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   561 | `	)` |
|        5 |   562 |  |
|        - |   563 | `	VmFrame *pFrame;` |
|        - |   564 | `	/* Allocate a new frame */` |
|    23241 |   565 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    23241 |   566 | `	if( pFrame == 0 ){` |
|      ! 0 |   567 | `		return SXERR_MEM;` |
|        - |   568 | `	}` |
|        - |   569 | `	/* Link to the list of active VM frame */` |
|    23241 |   570 | `	pFrame->pParent = pVm->pFrame;` |
|    23241 |   571 | `	pVm->pFrame = pFrame;` |
|    23241 |   572 | `	if( ppFrame ){` |
|        - |   573 | `		/* Write a pointer to the new VM frame */` |
|    20087 |   574 | `		*ppFrame = pFrame;` |
|    10041 |   575 | `	}` |
|    23241 |   576 | `	return SXRET_OK;` |
|    11623 |   577 |  |
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
|    20078 |   621 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        5 |   622 |  |
|    20083 |   623 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    20083 |   624 | `	if( pCurFrame ){` |
|        - |   625 | `		/* Unlink from the list of active VM frame */` |
|    20083 |   626 | `		pVm->pFrame = pCurFrame->pParent;` |
|    20083 |   627 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   628 | `			VmSlot  *aSlot;` |
|        - |   629 | `			sxu32 n;` |
|        - |   630 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    19231 |   631 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   125887 |   632 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   633 | `				/* Unset the local variable */` |
|   106661 |   634 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    53333 |   635 | `			}` |
|        - |   636 | `			/* Remove local reference */` |
|    19231 |   637 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   125961 |   638 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   106735 |   639 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    53370 |   640 | `			}` |
|     9613 |   641 | `		}` |
|        - |   642 | `		/* Release internal containers */` |
|    20083 |   643 | `		SyHashRelease(&pCurFrame->hVar);` |
|    20083 |   644 | `		SySetRelease(&pCurFrame->sArg);` |
|    20083 |   645 | `		SySetRelease(&pCurFrame->sLocal);` |
|    20083 |   646 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   647 | `		/* Release the whole structure */` |
|    20083 |   648 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|    10039 |   649 | `	}` |
|    20083 |   650 |  |
|        - |   651 | `/*` |
|        - |   652 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   653 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   654 | ` * should be skipped when looking for the real execution context.` |
|        - |   655 | ` */` |
|  7160990 |   656 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        5 |   657 |  |
|  7165697 |   658 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     4707 |   659 | `		pFrame = pFrame->pParent;` |
|        5 |   660 | `	}` |
|  7160995 |   661 | `	return pFrame;` |
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
|    45968 |   691 | `static sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|        5 |   692 |  |
|        - |   693 | `	sxu32 nUsed;` |
|    45983 |   694 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
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
|    45973 |   707 | `	return SXRET_OK;` |
|    22989 |   708 |  |
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
|        - |   843 | `/*` |
|        - |   844 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   845 | ` * it can be instanciated from the executed PHP script.` |
|        - |   846 | ` */` |
|        - |   847 | `/*` |
|        - |   848 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|        - |   849 | ` * This is the per-execution part of mounting a class: every static/const` |
|        - |   850 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|        - |   851 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|        - |   852 | ` * properties register their enforcement slot. It is factored out of` |
|        - |   853 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|        - |   854 | ` * reuse without re-installing the (compile-time) methods.` |
|        - |   855 | ` */` |
|   356990 |   856 | `static sxi32 VmMountUserClassAttrs(` |
|        - |   857 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   858 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|        - |   859 | `	)` |
|        5 |   860 |  |
|        - |   861 | `	ph7_class_attr *pAttr;` |
|        - |   862 | `	SyHashEntry *pEntry;` |
|        - |   863 | `	/* Reset the loop cursor */` |
|   356995 |   864 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   865 | `	/* Process only static and constant attribute */` |
|  1407450 |   866 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   867 | `		/* Extract the current attribute */` |
|   871965 |   868 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   871965 |   869 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   870 | `			ph7_value *pMemObj;` |
|        - |   871 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1847 |   872 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1847 |   873 | `			if( pMemObj == 0 ){` |
|      ! 0 |   874 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   875 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   876 | `					&pClass->sName,&pAttr->sName` |
|        - |   877 | `					);` |
|      ! 0 |   878 | `				return SXERR_MEM;` |
|        - |   879 | `			}` |
|     1847 |   880 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   881 | `				/* Initialize attribute default value (any complex expression) */` |
|     1843 |   882 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);` |
|      919 |   883 | `			}` |
|        - |   884 | `			/* Record attribute index */` |
|     1847 |   885 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   886 | `			/* Install static attribute in the reference table */` |
|     1847 |   887 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   888 | `			/* If this is a typed static property, register the slot so the` |
|        - |   889 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   890 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   891 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1847 |   892 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       17 |   893 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       17 |   894 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   895 | `					return SXERR_MEM;` |
|        - |   896 | `				}` |
|       17 |   897 | `				pVmAttrS->pAttr = pAttr;` |
|       17 |   898 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       17 |   899 | `				pVmAttrS->iState = 0;` |
|       17 |   900 | `				pVmAttrS->pOwner = pClass;` |
|        - |   901 | `				/* Static typed property with no default starts uninitialized */` |
|       14 |   902 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|       12 |   903 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   904 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   905 | `				}` |
|       17 |   906 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   907 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   908 | `					return SXERR_MEM;` |
|        - |   909 | `				}` |
|        7 |   910 | `			}` |
|      921 |   911 | `		}` |
|        5 |   912 | `	}` |
|   356995 |   913 | `	return SXRET_OK;` |
|   178500 |   914 |  |
|   356758 |   915 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   916 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   917 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   918 | `	)` |
|        5 |   919 |  |
|        - |   920 | `	ph7_class_method *pMeth;` |
|        - |   921 | `	SyHashEntry *pEntry;` |
|        - |   922 | `	sxi32 rc;` |
|        - |   923 | `	/* Reserve/initialize the static and constant attribute slots */` |
|   356763 |   924 | `	rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   356763 |   925 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   926 | `		return rc;` |
|        - |   927 | `	}` |
|        - |   928 | `	/* Install class methods */` |
|   356763 |   929 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   930 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   931 | `		 */` |
|   194375 |   932 | `		return SXRET_OK;` |
|        - |   933 | `	}` |
|        - |   934 | `	/* Create constructor alias if not yet done */` |
|   162393 |   935 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   936 | `		/* User constructor with the same base class name */` |
|     6711 |   937 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6711 |   938 | `		if( pEntry ){` |
|      ! 0 |   939 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   940 | `			/* Create the alias */` |
|      ! 0 |   941 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   942 | `		}` |
|     3353 |   943 | `	}` |
|        - |   944 | `	/* Install the methods now */` |
|   162393 |   945 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  1662669 |   946 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  1419087 |   947 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  1419087 |   948 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  1419079 |   949 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  1419079 |   950 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   951 | `				return rc;` |
|        - |   952 | `			}` |
|   709537 |   953 | `		}` |
|        5 |   954 | `	}` |
|        - |   955 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   162393 |   956 | `	pClass->bMounted = TRUE;` |
|   162393 |   957 | `	return SXRET_OK;` |
|   178384 |   958 |  |
|        - |   959 | `/*` |
|        - |   960 | ` * Allocate a private frame for attributes of the given` |
|        - |   961 | ` * class instance (Object in the PHP jargon).` |
|        - |   962 | ` */` |
|     2218 |   963 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   964 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   965 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   966 | `	)` |
|        5 |   967 |  |
|     2223 |   968 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   969 | `	ph7_class_attr *pAttr;` |
|        - |   970 | `	SyHashEntry *pEntry;` |
|        - |   971 | `	sxi32 rc;` |
|        - |   972 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2223 |   973 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     9289 |   974 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   975 | `		VmClassAttr *pVmAttr;` |
|        - |   976 | `		/* Extract the current attribute */` |
|     7071 |   977 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7071 |   978 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     7071 |   979 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   980 | `			return SXERR_MEM;` |
|        - |   981 | `		}` |
|     7071 |   982 | `		pVmAttr->pAttr = pAttr;` |
|     7071 |   983 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   984 | `			ph7_value *pMemObj;` |
|        - |   985 | `			/* Reserve a memory object for this attribute */` |
|     7045 |   986 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     7045 |   987 | `			if( pMemObj == 0 ){` |
|      ! 0 |   988 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   989 | `				return SXERR_MEM;` |
|        - |   990 | `			}` |
|     7045 |   991 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     7045 |   992 | `			pVmAttr->iState = 0;` |
|     7045 |   993 | `			pVmAttr->pOwner = pClass;` |
|     7045 |   994 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   995 | `				/* Initialize attribute default value (any complex expression) */` |
|     2423 |   996 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj,FALSE);` |
|     5836 |   997 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   998 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   999 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       76 |  1000 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |  1001 | `			}` |
|     7045 |  1002 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     7045 |  1003 | `			if( rc != SXRET_OK ){` |
|        - |  1004 | `				VmSlot sSlot;` |
|        - |  1005 | `				/* Restore memory object */` |
|      ! 0 |  1006 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |  1007 | `				sSlot.pUserData = 0;` |
|      ! 0 |  1008 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |  1009 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1010 | `				return SXERR_MEM;` |
|        - |  1011 | `			}` |
|        - |  1012 | `			/* Install attribute in the reference table */` |
|     7045 |  1013 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |  1014 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |  1015 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |  1016 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     7045 |  1017 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      185 |  1018 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      185 |  1019 | `				if( rc != SXRET_OK ){` |
|        - |  1020 | `					VmSlot sSlot;` |
|      ! 0 |  1021 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |  1022 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |  1023 | `					sSlot.pUserData = 0;` |
|      ! 0 |  1024 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |  1025 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1026 | `					return SXERR_MEM;` |
|        - |  1027 | `				}` |
|       90 |  1028 | `			}` |
|     3525 |  1029 | `		}else{` |
|        - |  1030 | `			/* Install static/constant attribute */` |
|       29 |  1031 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       29 |  1032 | `			pVmAttr->iState = 0;` |
|       29 |  1033 | `			pVmAttr->pOwner = pClass;` |
|       29 |  1034 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       29 |  1035 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  1036 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |  1037 | `				return SXERR_MEM;` |
|        - |  1038 | `			}` |
|        - |  1039 | `		}` |
|        5 |  1040 | `	}` |
|     2223 |  1041 | `	return SXRET_OK;` |
|     1114 |  1042 |  |
|        - |  1043 | `/* Forward declaration */` |
|        - |  1044 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |  1045 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |  1046 | `/*` |
|        - |  1047 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |  1048 | ` */` |
|        - |  1049 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |  1050 | `/*` |
|        - |  1051 | ` * Reserve a constant memory object.` |
|        - |  1052 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1053 | ` */` |
|   457926 |  1054 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  1055 |  |
|        - |  1056 | `	ph7_value *pObj;` |
|        - |  1057 | `	sxi32 rc;` |
|   457931 |  1058 | `	if( pIndex ){` |
|        - |  1059 | `		/* Object index in the object table */` |
|   448487 |  1060 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   224241 |  1061 | `	}` |
|        - |  1062 | `	/* Reserve a slot for the new object */` |
|   457931 |  1063 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   457931 |  1064 | `	if( rc != SXRET_OK ){` |
|        - |  1065 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1066 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1067 | `		 */` |
|      ! 0 |  1068 | `		return 0;` |
|        - |  1069 | `	}` |
|   457931 |  1070 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   457931 |  1071 | `	return pObj;` |
|   228968 |  1072 |  |
|        - |  1073 | `/*` |
|        - |  1074 | ` * Reserve a memory object.` |
|        - |  1075 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1076 | ` */` |
|  2152210 |  1077 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        5 |  1078 |  |
|        - |  1079 | `	ph7_value *pObj;` |
|        - |  1080 | `	sxi32 rc;` |
|  2152215 |  1081 | `	if( pIndex ){` |
|        - |  1082 | `		/* Object index in the object table */` |
|  2152215 |  1083 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1076105 |  1084 | `	}` |
|        - |  1085 | `	/* Reserve a slot for the new object */` |
|  2152215 |  1086 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2152215 |  1087 | `	if( rc != SXRET_OK ){` |
|        - |  1088 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  1089 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  1090 | `		 */` |
|      ! 0 |  1091 | `		return 0;` |
|        - |  1092 | `	}` |
|  2152215 |  1093 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2152215 |  1094 | `	return pObj;` |
|  1076110 |  1095 |  |
|        - |  1096 | `/* Forward declaration */` |
|        - |  1097 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1098 | `/* Forward declarations for Fiber C functions */` |
|        - |  1099 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1100 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1101 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1102 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1103 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1104 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1105 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1106 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1107 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1108 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1109 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1110 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1111 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1112 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1113 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1114 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1115 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1116 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1117 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1118 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1119 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1120 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1121 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1122 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1123 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1124 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1125 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1126 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1127 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1128 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1129 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1130 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1131 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1132 | `/*` |
|        - |  1133 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1134 | ` * directly as foreign functions.` |
|        - |  1135 | ` */` |
|        - |  1136 | `#define PH7_BUILTIN_LIB \` |
|        - |  1137 | `	"interface Throwable {"\` |
|        - |  1138 | `	"public function getMessage();"\` |
|        - |  1139 | `	"public function getCode();"\` |
|        - |  1140 | `	"public function getFile();"\` |
|        - |  1141 | `	"public function getLine();"\` |
|        - |  1142 | `	"public function getTrace();"\` |
|        - |  1143 | `	"public function getTraceAsString();"\` |
|        - |  1144 | `	"public function getPrevious();"\` |
|        - |  1145 | `	"public function __toString();"\` |
|        - |  1146 | `	"}"\` |
|        - |  1147 | `	"interface Traversable {}"\` |
|        - |  1148 | `	"interface ArrayAccess {"\` |
|        - |  1149 | `	"public function offsetExists($offset);"\` |
|        - |  1150 | `	"public function offsetGet($offset);"\` |
|        - |  1151 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1152 | `	"public function offsetUnset($offset);"\` |
|        - |  1153 | `	"}"\` |
|        - |  1154 | `	"interface Countable {"\` |
|        - |  1155 | `	"public function count();"\` |
|        - |  1156 | `	"}"\` |
|        - |  1157 | `	"interface Stringable {"\` |
|        - |  1158 | `	"public function __toString();"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"interface JsonSerializable {"\` |
|        - |  1161 | `	"public function jsonSerialize();"\` |
|        - |  1162 | `	"}"\` |
|        - |  1163 | `	"interface UnitEnum {"\` |
|        - |  1164 | `	"public static function cases();"\` |
|        - |  1165 | `	"}"\` |
|        - |  1166 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1167 | `	"public static function from($value);"\` |
|        - |  1168 | `	"public static function tryFrom($value);"\` |
|        - |  1169 | `	"}"\` |
|        - |  1170 | `	"class Exception implements Throwable { "\` |
|        - |  1171 | `    "protected $message = '';"\` |
|        - |  1172 | `    "protected $code = 0;"\` |
|        - |  1173 | `    "protected $file;"\` |
|        - |  1174 | `    "protected $line;"\` |
|        - |  1175 | `    "protected $trace;"\` |
|        - |  1176 | `    "protected $previous;"\` |
|        - |  1177 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1178 | `	"   if( isset($message) ){"\` |
|        - |  1179 | `	"	  $this->message = $message;"\` |
|        - |  1180 | `	"   }"\` |
|        - |  1181 | `	"   $this->code = $code;"\` |
|        - |  1182 | `	"   $this->file = __FILE__;"\` |
|        - |  1183 | `	"   $this->line = __LINE__;"\` |
|        - |  1184 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1185 | `	"   if( isset($previous) ){"\` |
|        - |  1186 | `	"     $this->previous = $previous;"\` |
|        - |  1187 | `	"   }"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"public function getMessage(){"\` |
|        - |  1190 | `	"   return $this->message;"\` |
|        - |  1191 | `	"}"\` |
|        - |  1192 | `	" public function getCode(){"\` |
|        - |  1193 | `	"  return $this->code;"\` |
|        - |  1194 | `	"}"\` |
|        - |  1195 | `	"public function getFile(){"\` |
|        - |  1196 | `	"  return $this->file;"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"public function getLine(){"\` |
|        - |  1199 | `	"  return $this->line;"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `	"public function getTrace(){"\` |
|        - |  1202 | `	"   return $this->trace;"\` |
|        - |  1203 | `	"}"\` |
|        - |  1204 | `	"public function getTraceAsString(){"\` |
|        - |  1205 | `	"  return debug_string_backtrace();"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"public function getPrevious(){"\` |
|        - |  1208 | `	"    return $this->previous;"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"public function __toString(){"\` |
|        - |  1211 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1212 | `    "}"\` |
|        - |  1213 | `	"}"\` |
|        - |  1214 | `	"class Error implements Throwable { "\` |
|        - |  1215 | `    "protected $message = '';"\` |
|        - |  1216 | `    "protected $code = 0;"\` |
|        - |  1217 | `    "protected $file;"\` |
|        - |  1218 | `    "protected $line;"\` |
|        - |  1219 | `    "protected $trace;"\` |
|        - |  1220 | `    "protected $previous;"\` |
|        - |  1221 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1222 | `	"   if( isset($message) ){"\` |
|        - |  1223 | `	"	  $this->message = $message;"\` |
|        - |  1224 | `	"   }"\` |
|        - |  1225 | `	"   $this->code = $code;"\` |
|        - |  1226 | `	"   $this->file = __FILE__;"\` |
|        - |  1227 | `	"   $this->line = __LINE__;"\` |
|        - |  1228 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1229 | `	"   if( isset($previous) ){"\` |
|        - |  1230 | `	"     $this->previous = $previous;"\` |
|        - |  1231 | `	"   }"\` |
|        - |  1232 | `	"}"\` |
|        - |  1233 | `	"public function getMessage(){"\` |
|        - |  1234 | `	"   return $this->message;"\` |
|        - |  1235 | `	"}"\` |
|        - |  1236 | `	"public function getCode(){"\` |
|        - |  1237 | `	"  return $this->code;"\` |
|        - |  1238 | `	"}"\` |
|        - |  1239 | `	"public function getFile(){"\` |
|        - |  1240 | `	"  return $this->file;"\` |
|        - |  1241 | `	"}"\` |
|        - |  1242 | `	"public function getLine(){"\` |
|        - |  1243 | `	"  return $this->line;"\` |
|        - |  1244 | `	"}"\` |
|        - |  1245 | `	"public function getTrace(){"\` |
|        - |  1246 | `	"   return $this->trace;"\` |
|        - |  1247 | `	"}"\` |
|        - |  1248 | `	"public function getTraceAsString(){"\` |
|        - |  1249 | `	"  return debug_string_backtrace();"\` |
|        - |  1250 | `	"}"\` |
|        - |  1251 | `	"public function getPrevious(){"\` |
|        - |  1252 | `	"    return $this->previous;"\` |
|        - |  1253 | `	"}"\` |
|        - |  1254 | `	"public function __toString(){"\` |
|        - |  1255 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1256 | `	"}"\` |
|        - |  1257 | `	"}"\` |
|        - |  1258 | `	"class TypeError extends Error { }"\` |
|        - |  1259 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1260 | `	"class ValueError extends Error { }"\` |
|        - |  1261 | `	"class FiberError extends Error { }"\` |
|        - |  1262 | `	"class AssertionError extends Error { }"\` |
|        - |  1263 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1264 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1265 | `	"class ErrorException extends Exception { "\` |
|        - |  1266 | `	"protected $severity;"\` |
|        - |  1267 | `	"public function __construct(string $message = null,"\` |
|        - |  1268 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1269 | `	"   if( isset($message) ){"\` |
|        - |  1270 | `	"	  $this->message = $message;"\` |
|        - |  1271 | `	"   }"\` |
|        - |  1272 | `	"   $this->severity = $severity;"\` |
|        - |  1273 | `	"   $this->code = $code;"\` |
|        - |  1274 | `	"   $this->file = $filename;"\` |
|        - |  1275 | `	"   $this->line = $lineno;"\` |
|        - |  1276 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1277 | `	"   if( isset($previous) ){"\` |
|        - |  1278 | `	"     $this->previous = $previous;"\` |
|        - |  1279 | `	"   }"\` |
|        - |  1280 | `	"}"\` |
|        - |  1281 | `	"public function getSeverity(){"\` |
|        - |  1282 | `	"   return $this->severity;"\` |
|        - |  1283 | `    "}"\` |
|        - |  1284 | `	"}"\` |
|        - |  1285 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|        - |  1286 | `	"class LogicException extends Exception { }"\` |
|        - |  1287 | `	"class RuntimeException extends Exception { }"\` |
|        - |  1288 | `	"class BadFunctionCallException extends LogicException { }"\` |
|        - |  1289 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|        - |  1290 | `	"class DomainException extends LogicException { }"\` |
|        - |  1291 | `	"class InvalidArgumentException extends LogicException { }"\` |
|        - |  1292 | `	"class LengthException extends LogicException { }"\` |
|        - |  1293 | `	"class OutOfRangeException extends LogicException { }"\` |
|        - |  1294 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|        - |  1295 | `	"class OverflowException extends RuntimeException { }"\` |
|        - |  1296 | `	"class RangeException extends RuntimeException { }"\` |
|        - |  1297 | `	"class UnderflowException extends RuntimeException { }"\` |
|        - |  1298 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|        - |  1299 | `	"interface Iterator extends Traversable {"\` |
|        - |  1300 | `	"public function current();"\` |
|        - |  1301 | `	"public function key();"\` |
|        - |  1302 | `	"public function next();"\` |
|        - |  1303 | `	"public function rewind();"\` |
|        - |  1304 | `	"public function valid();"\` |
|        - |  1305 | `	"}"\` |
|        - |  1306 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1307 | `	"public function getIterator();"\` |
|        - |  1308 | `	"}"\` |
|        - |  1309 | `	"interface Serializable {"\` |
|        - |  1310 | `	"public function serialize();"\` |
|        - |  1311 | `	"public function unserialize(string $serialized);"\` |
|        - |  1312 | `	"}"\` |
|        - |  1313 | `	"/* Directory releated IO */"\` |
|        - |  1314 | `	"class Directory {"\` |
|        - |  1315 | `	"public $handle = null;"\` |
|        - |  1316 | `	"public $path  = null;"\` |
|        - |  1317 | `	"public function __construct(string $path)"\` |
|        - |  1318 | `	"{"\` |
|        - |  1319 | `	"   $this->handle = opendir($path);"\` |
|        - |  1320 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1321 | `	"      $this->path = $path;"\` |
|        - |  1322 | `	"   }"\` |
|        - |  1323 | `	"}"\` |
|        - |  1324 | `	"public function __destruct()"\` |
|        - |  1325 | `	"{"\` |
|        - |  1326 | `	"  if( $this->handle != null ){"\` |
|        - |  1327 | `	"       closedir($this->handle);"\` |
|        - |  1328 | `	"  }"\` |
|        - |  1329 | `	"}"\` |
|        - |  1330 | `	"public function read()"\` |
|        - |  1331 | `	"{"\` |
|        - |  1332 | `	"    return readdir($this->handle);"\` |
|        - |  1333 | `	"}"\` |
|        - |  1334 | `	"public function rewind()"\` |
|        - |  1335 | `	"{"\` |
|        - |  1336 | `	"    rewinddir($this->handle);"\` |
|        - |  1337 | `	"}"\` |
|        - |  1338 | `	"public function close()"\` |
|        - |  1339 | `	"{"\` |
|        - |  1340 | `	"    closedir($this->handle);"\` |
|        - |  1341 | `	"    $this->handle = null;"\` |
|        - |  1342 | `	"}"\` |
|        - |  1343 | `	"}"\` |
|        - |  1344 | `	"class Fiber {"\` |
|        - |  1345 | `	"  private $__ctx;"\` |
|        - |  1346 | `	"  private $__callable;"\` |
|        - |  1347 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1348 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1349 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1350 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1351 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1352 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1353 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1354 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1355 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1356 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1357 | `	"}"\` |
|        - |  1358 | `	"class Generator implements Iterator {"\` |
|        - |  1359 | `	"  private $__ctx;"\` |
|        - |  1360 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1361 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1362 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1363 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1364 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1365 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1366 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1367 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1368 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1369 | `	"}"\` |
|        - |  1370 | `	"class stdClass{"\` |
|        - |  1371 | `	"  public $value;"\` |
|        - |  1372 | `	" /* Magic methods */"\` |
|        - |  1373 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1374 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1375 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1376 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1377 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1378 | `	"}"\` |
|        - |  1379 | `	"function dir(string $path){"\` |
|        - |  1380 | `	"   return new Directory($path);"\` |
|        - |  1381 | `	"}"\` |
|        - |  1382 | `	"function Dir(string $path){"\` |
|        - |  1383 | `	"   return new Directory($path);"\` |
|        - |  1384 | `	"}"\` |
|        - |  1385 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1386 | `    "{"\` |
|        - |  1387 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1388 | `	"  $aDir = array();"\` |
|        - |  1389 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1390 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1391 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1392 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1393 | `	"   }"\` |
|        - |  1394 | `	"  closedir($pHandle);"\` |
|        - |  1395 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1396 | `	"      rsort($aDir);"\` |
|        - |  1397 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1398 | `	"      sort($aDir);"\` |
|        - |  1399 | `	"  }"\` |
|        - |  1400 | `	"  return $aDir;"\` |
|        - |  1401 | `	"}"\` |
|        - |  1402 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1403 | `	"/* Open the target directory */"\` |
|        - |  1404 | `	"$zDir = dirname($pattern);"\` |
|        - |  1405 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1406 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1407 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1408 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1409 | `	"	return FALSE;"\` |
|        - |  1410 | `	"}"\` |
|        - |  1411 | `	"$pattern = basename($pattern);"\` |
|        - |  1412 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1413 | `	"/* Loop throw available entries */"\` |
|        - |  1414 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1415 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1416 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1417 | `	"	if( $rc ){"\` |
|        - |  1418 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1419 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1420 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1421 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1422 | `	"		  }"\` |
|        - |  1423 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1424 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1425 | `	"		 continue;"\` |
|        - |  1426 | `	"	   }"\` |
|        - |  1427 | `	"	   /* Add the entry */"\` |
|        - |  1428 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1429 | `	"	}"\` |
|        - |  1430 | `	" }"\` |
|        - |  1431 | `	"/* Close the handle */"\` |
|        - |  1432 | `	"closedir($pHandle);"\` |
|        - |  1433 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1434 | `	"  /* Sort the array */"\` |
|        - |  1435 | `	"  sort($pArray);"\` |
|        - |  1436 | `	"}"\` |
|        - |  1437 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1438 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1439 | `	"  $pArray[] = $pattern;"\` |
|        - |  1440 | `	"}"\` |
|        - |  1441 | `	"/* Return the created array */"\` |
|        - |  1442 | `	"return $pArray;"\` |
|        - |  1443 | `   "}"\` |
|        - |  1444 | `   "/* Creates a temporary file */"\` |
|        - |  1445 | `   "function tmpfile(){"\` |
|        - |  1446 | `   "  /* Extract the temp directory */"\` |
|        - |  1447 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1448 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1449 | `   "    /* Use the current dir */"\` |
|        - |  1450 | `   "    $zTempDir = '.';"\` |
|        - |  1451 | `   "  }"\` |
|        - |  1452 | `   "  /* Create the file */"\` |
|        - |  1453 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1454 | `   "  return $pHandle;"\` |
|        - |  1455 | `   "}"\` |
|        - |  1456 | `   "/* Creates a temporary filename */"\` |
|        - |  1457 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1458 | `   "{"\` |
|        - |  1459 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1460 | `   "}"\` |
|        - |  1461 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1462 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1463 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1464 | `   "/* Copy arguments */"\` |
|        - |  1465 | `   "$nArgs = func_num_args();"\` |
|        - |  1466 | `   "$pNew = array();"\` |
|        - |  1467 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1468 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1469 | `    "}"\` |
|        - |  1470 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1471 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1472 | `	"/* Erase */"\` |
|        - |  1473 | `	"array_erase($pArray);"\` |
|        - |  1474 | `	"/* Unshift */"\` |
|        - |  1475 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1476 | `	"return sizeof($pArray);"\` |
|        - |  1477 | `    "}"\` |
|        - |  1478 | `	"function array_merge_recursive(){"\` |
|        - |  1479 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1480 | `    "$arrays = func_get_args();"\` |
|        - |  1481 | `    "$narrays = count($arrays);"\` |
|        - |  1482 | `    "$ret = array();"\` |
|        - |  1483 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1484 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1485 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1486 | `	 " }"\` |
|        - |  1487 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1488 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1489 | `     "  if( $keyIsInt ) {"\` |
|        - |  1490 | `     "   $ret[] = $value;"\` |
|        - |  1491 | `     "  } else {"\` |
|        - |  1492 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1493 | `     "    $cur = $ret[$key];"\` |
|        - |  1494 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1495 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1496 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1497 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1498 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1499 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1500 | `     "    } else {"\` |
|        - |  1501 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1502 | `     "    }"\` |
|        - |  1503 | `     "   } else {"\` |
|        - |  1504 | `     "    $ret[$key] = $value;"\` |
|        - |  1505 | `     "   }"\` |
|        - |  1506 | `     "  }"\` |
|        - |  1507 | `     " }"\` |
|        - |  1508 | `	 " }"\` |
|        - |  1509 | `	 " return $ret;"\` |
|        - |  1510 | `    "}"\` |
|        - |  1511 | `	"function max(){"\` |
|        - |  1512 | `    "  $pArgs = func_get_args();"\` |
|        - |  1513 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1514 | `	"  return null;"\` |
|        - |  1515 | `    " }"\` |
|        - |  1516 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1517 | `    " $pArg = $pArgs[0];"\` |
|        - |  1518 | `	" if( !is_array($pArg) ){"\` |
|        - |  1519 | `	"   return $pArg; "\` |
|        - |  1520 | `	" }"\` |
|        - |  1521 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1522 | `	"   return null;"\` |
|        - |  1523 | `	" }"\` |
|        - |  1524 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1525 | `	" reset($pArg);"\` |
|        - |  1526 | `	" $max = current($pArg);"\` |
|        - |  1527 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1528 | `	"   if( $val > $max ){"\` |
|        - |  1529 | `	"     $max = $val;"\` |
|        - |  1530 | `    " }"\` |
|        - |  1531 | `	" }"\` |
|        - |  1532 | `	" return $max;"\` |
|        - |  1533 | `    " }"\` |
|        - |  1534 | `    " $max = $pArgs[0];"\` |
|        - |  1535 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1536 | `    " $val = $pArgs[$i];"\` |
|        - |  1537 | `	"if( $val > $max ){"\` |
|        - |  1538 | `	" $max = $val;"\` |
|        - |  1539 | `	"}"\` |
|        - |  1540 | `    " }"\` |
|        - |  1541 | `	" return $max;"\` |
|        - |  1542 | `    "}"\` |
|        - |  1543 | `	"function min(){"\` |
|        - |  1544 | `    "  $pArgs = func_get_args();"\` |
|        - |  1545 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1546 | `	"  return null;"\` |
|        - |  1547 | `    " }"\` |
|        - |  1548 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1549 | `    " $pArg = $pArgs[0];"\` |
|        - |  1550 | `	" if( !is_array($pArg) ){"\` |
|        - |  1551 | `	"   return $pArg; "\` |
|        - |  1552 | `	" }"\` |
|        - |  1553 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1554 | `	"   return null;"\` |
|        - |  1555 | `	" }"\` |
|        - |  1556 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1557 | `	" reset($pArg);"\` |
|        - |  1558 | `	" $min = current($pArg);"\` |
|        - |  1559 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1560 | `	"   if( $val < $min ){"\` |
|        - |  1561 | `	"     $min = $val;"\` |
|        - |  1562 | `    " }"\` |
|        - |  1563 | `	" }"\` |
|        - |  1564 | `	" return $min;"\` |
|        - |  1565 | `    " }"\` |
|        - |  1566 | `    " $min = $pArgs[0];"\` |
|        - |  1567 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1568 | `    " $val = $pArgs[$i];"\` |
|        - |  1569 | `	"if( $val < $min ){"\` |
|        - |  1570 | `	" $min = $val;"\` |
|        - |  1571 | `	" }"\` |
|        - |  1572 | `    " }"\` |
|        - |  1573 | `	" return $min;"\` |
|        - |  1574 | `	"}"\` |
|        - |  1575 | `	"function fileowner(string $file){"\` |
|        - |  1576 | `    " $a = stat($file);"\` |
|        - |  1577 | `	" if( !is_array($a) ){"\` |
|        - |  1578 | `	"	return false;"\` |
|        - |  1579 | `	" }"\` |
|        - |  1580 | `	" return $a['uid'];"\` |
|        - |  1581 | `    "}"\` |
|        - |  1582 | `    "function filegroup(string $file){"\` |
|        - |  1583 | `	" $a = stat($file);"\` |
|        - |  1584 | `	" if( !is_array($a) ){"\` |
|        - |  1585 | `	"	return false;"\` |
|        - |  1586 | `	" }"\` |
|        - |  1587 | `	" return $a['gid'];"\` |
|        - |  1588 | `    "}"\` |
|        - |  1589 | `	 "function fileinode(string $file){"\` |
|        - |  1590 | `	" $a = stat($file);"\` |
|        - |  1591 | `	" if( !is_array($a) ){"\` |
|        - |  1592 | `	"	return false;"\` |
|        - |  1593 | `	" }"\` |
|        - |  1594 | `	" return $a['ino'];"\` |
|        - |  1595 | `    "}"` |
|        - |  1596 |  |
|        - |  1597 | `/*` |
|        - |  1598 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1599 | ` * start compiling the target PHP program.` |
|        - |  1600 | ` */` |
|     3148 |  1601 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1602 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1603 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1604 | `	 )` |
|        5 |  1605 |  |
|        - |  1606 | `	SyString sBuiltin;` |
|        - |  1607 | `	ph7_value *pObj;` |
|        - |  1608 | `	sxi32 rc;` |
|        - |  1609 | `	/* Zero the structure */` |
|     3153 |  1610 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1611 | `	/* Initialize VM fields */` |
|     3153 |  1612 | `	pVm->pEngine = &(*pEngine);` |
|     3153 |  1613 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1614 | `	/* Instructions containers */` |
|     3153 |  1615 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3153 |  1616 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3153 |  1617 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1618 | `	/* Object containers */` |
|     3153 |  1619 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3153 |  1620 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1621 | `	/* Virtual machine internal containers */` |
|     3153 |  1622 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3153 |  1623 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3153 |  1624 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3153 |  1625 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3153 |  1626 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3153 |  1627 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3153 |  1628 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3153 |  1629 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3153 |  1630 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3153 |  1631 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3153 |  1632 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3153 |  1633 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3153 |  1634 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3153 |  1635 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3153 |  1636 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3153 |  1637 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3153 |  1638 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3153 |  1639 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3153 |  1640 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3153 |  1641 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3153 |  1642 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3153 |  1643 | `	pVm->pPendingException = 0;` |
|        - |  1644 | `	/* Configuration containers */` |
|     3153 |  1645 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3153 |  1646 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3153 |  1647 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3153 |  1648 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3153 |  1649 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3153 |  1650 | `	pVm->iResponseStatus = 200;` |
|     3153 |  1651 | `	pVm->bHeadersSent = 0;` |
|     3153 |  1652 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1653 | `	/* Error callbacks containers */` |
|     3153 |  1654 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3153 |  1655 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3153 |  1656 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3153 |  1657 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3153 |  1658 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1659 | `	/* Set a default recursion limit */` |
|        - |  1660 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3153 |  1661 | `	pVm->nMaxDepth = 32;` |
|        - |  1662 | `#else` |
|        - |  1663 | `	pVm->nMaxDepth = 16;` |
|        - |  1664 | `#endif` |
|        - |  1665 | `	/* Default assertion flags */` |
|     3153 |  1666 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1667 | `	/* JSON return status */` |
|     3153 |  1668 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1669 | `	/* PRNG context */` |
|     3153 |  1670 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1671 | `	/* Install the null constant */` |
|     3153 |  1672 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3153 |  1673 | `	if( pObj == 0 ){` |
|      ! 0 |  1674 | `		rc = SXERR_MEM;` |
|      ! 0 |  1675 | `		goto Err;` |
|        - |  1676 | `	}` |
|     3153 |  1677 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1678 | `	/* Install the boolean TRUE constant */` |
|     3153 |  1679 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3153 |  1680 | `	if( pObj == 0 ){` |
|      ! 0 |  1681 | `		rc = SXERR_MEM;` |
|      ! 0 |  1682 | `		goto Err;` |
|        - |  1683 | `	}` |
|     3153 |  1684 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1685 | `	/* Install the boolean FALSE constant */` |
|     3153 |  1686 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3153 |  1687 | `	if( pObj == 0 ){` |
|      ! 0 |  1688 | `		rc = SXERR_MEM;` |
|      ! 0 |  1689 | `		goto Err;` |
|        - |  1690 | `	}` |
|     3153 |  1691 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1692 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1693 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1694 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3153 |  1695 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3153 |  1696 | `	if( pObj == 0 ){` |
|      ! 0 |  1697 | `		rc = SXERR_MEM;` |
|      ! 0 |  1698 | `		goto Err;` |
|        - |  1699 | `	}` |
|     3153 |  1700 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1701 | `	/* Create the global frame */` |
|     3153 |  1702 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3153 |  1703 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1704 | `		goto Err;` |
|        - |  1705 | `	}` |
|        - |  1706 | `	/* Initialize the code generator */` |
|     3153 |  1707 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3153 |  1708 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1709 | `		goto Err;` |
|        - |  1710 | `	}` |
|        - |  1711 | `	/* VM correctly initialized,set the magic number */` |
|     3153 |  1712 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3153 |  1713 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1714 | `	/* Compile the built-in library */` |
|     3153 |  1715 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1716 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3153 |  1717 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1718 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3153 |  1719 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3153 |  1720 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3153 |  1721 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|     3153 |  1722 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|     3153 |  1723 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|        - |  1724 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3153 |  1725 | `	pVm->pCoalesceObj = 0;` |
|     3153 |  1726 | `	pVm->bCoalesceArmed = 0;` |
|     3153 |  1727 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1728 | `	/* Register Fiber internal C functions */` |
|     3153 |  1729 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3153 |  1730 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3153 |  1731 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3153 |  1732 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3153 |  1733 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3153 |  1734 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3153 |  1735 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3153 |  1736 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3153 |  1737 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3153 |  1738 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1739 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3153 |  1740 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3153 |  1741 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3153 |  1742 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3153 |  1743 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3153 |  1744 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3153 |  1745 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3153 |  1746 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3153 |  1747 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3153 |  1748 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3153 |  1749 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1750 | `	/* Reset the code generator */` |
|     3153 |  1751 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3153 |  1752 | `	return SXRET_OK;` |
|      ! 0 |  1753 | `Err:` |
|      ! 0 |  1754 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1755 | `	return rc;` |
|     1579 |  1756 |  |
|        - |  1757 | `/*` |
|        - |  1758 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1759 | ` * routine which store the output in an internal blob.` |
|        - |  1760 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1761 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1762 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1763 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1764 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1765 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1766 | ` * to finish executing and extracting the output.` |
|        - |  1767 | ` */` |
|       56 |  1768 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1769 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1770 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1771 | `	void *pUserData     /* User private data */` |
|        - |  1772 | `	)` |
|      ! 0 |  1773 |  |
|        - |  1774 | `	 sxi32 rc;` |
|        - |  1775 | `	 /* Store the output in an internal BLOB */` |
|       56 |  1776 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       56 |  1777 | `	 return rc;` |
|      ! 0 |  1778 |  |
|        - |  1779 | `/*` |
|        - |  1780 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1781 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1782 | ` */` |
|    20916 |  1783 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        5 |  1784 |  |
|    20921 |  1785 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    20921 |  1786 | `	if( xCons != VmObConsumer ){` |
|     8305 |  1787 | `		pVm->nOutputLen += nLen;` |
|     8305 |  1788 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1031 |  1789 | `			pVm->bHeadersSent = 1;` |
|      513 |  1790 | `		}` |
|     4150 |  1791 | `	}` |
|    20921 |  1792 |  |
|        - |  1793 | `#define VM_STACK_GUARD 16` |
|        - |  1794 | `/*` |
|        - |  1795 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1796 | ` * our compiled PHP program.` |
|        - |  1797 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1798 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1799 | ` */` |
|    46782 |  1800 | `static ph7_value * VmNewOperandStack(` |
|        - |  1801 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1802 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1803 | `	)` |
|        5 |  1804 |  |
|        - |  1805 | `	ph7_value *pStack;` |
|        - |  1806 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1807 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1808 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1809 | `  ** on the maximum stack depth required.` |
|        - |  1810 | `  **` |
|        - |  1811 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1812 | `  */` |
|    46787 |  1813 | `	nInstr += VM_STACK_GUARD;` |
|    46787 |  1814 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    46787 |  1815 | `	if( pStack == 0 ){` |
|      ! 0 |  1816 | `		return 0;` |
|        - |  1817 | `	}` |
|        - |  1818 | `	/* Initialize the operand stack */` |
|  3087445 |  1819 | `	while( nInstr > 0 ){` |
|  3040663 |  1820 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  3040663 |  1821 | `		--nInstr;` |
|        5 |  1822 | `	}` |
|        - |  1823 | `	/* Ready for bytecode execution */` |
|    46787 |  1824 | `	return pStack;` |
|    23396 |  1825 |  |
|        - |  1826 | `/* Forward declaration */` |
|        - |  1827 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1828 | `/*` |
|        - |  1829 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1830 | ` * This routine gets called by the PH7 engine after` |
|        - |  1831 | ` * successful compilation of the target PHP program.` |
|        - |  1832 | ` */` |
|     2834 |  1833 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1834 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1835 | `	)` |
|        5 |  1836 |  |
|        - |  1837 | `	SyHashEntry *pEntry;` |
|        - |  1838 | `	sxi32 rc;` |
|     2839 |  1839 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1840 | `		/* Initialize your VM first */` |
|      ! 0 |  1841 | `		return SXERR_CORRUPT;` |
|        - |  1842 | `	}` |
|        - |  1843 | `	/* Mark the VM ready for byte-code execution */` |
|     2839 |  1844 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1845 | `	/* Release the code generator now we have compiled our program */` |
|     2839 |  1846 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1847 | `	/* Emit the DONE instruction */` |
|     2839 |  1848 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2839 |  1849 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1850 | `		return SXERR_MEM;` |
|        - |  1851 | `	}` |
|        - |  1852 | `	/* Script return value */` |
|     2839 |  1853 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1854 | `	/* Pending return value from a catch/finally block (see VmThrowException) */` |
|     2839 |  1855 | `	PH7_MemObjInit(&(*pVm),&pVm->sCatchReturn);` |
|     2839 |  1856 | `	pVm->bReturnRequested = 0;` |
|        - |  1857 | `	/* Allocate a new operand stack */` |
|     2839 |  1858 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2839 |  1859 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1860 | `		return SXERR_MEM;` |
|        - |  1861 | `	}` |
|        - |  1862 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1863 | `	 * private data. */` |
|     2839 |  1864 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2839 |  1865 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1866 | `	/* Allocate the reference table */` |
|     2839 |  1867 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2839 |  1868 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2839 |  1869 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1870 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1871 | `		return SXERR_MEM;` |
|        - |  1872 | `	}` |
|        - |  1873 | `	/* Zero the reference table */` |
|     2839 |  1874 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1875 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2839 |  1876 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2839 |  1877 | `	if( rc != SXRET_OK ){` |
|        - |  1878 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1879 | `		return rc;` |
|        - |  1880 | `	}` |
|        - |  1881 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|        - |  1882 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|        - |  1883 | `	 * every object/variable created during execution) is per-exec state that` |
|        - |  1884 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|        - |  1885 | `	 * below it is compile-time/init state that survives a reset. */` |
|     2839 |  1886 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|        - |  1887 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2839 |  1888 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2839 |  1889 | `	if( rc != SXRET_OK ){` |
|        - |  1890 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1891 | `		return rc;` |
|        - |  1892 | `	}` |
|        - |  1893 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2839 |  1894 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1895 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2839 |  1896 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1897 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2839 |  1898 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1899 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1900 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2839 |  1901 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2839 |  1902 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1903 | `#endif` |
|        - |  1904 | `	/* Initialize and install static and constants class attributes.` |
|        - |  1905 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|        - |  1906 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|        - |  1907 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|        - |  1908 | `	 * that function in sync when changing what is reserved here. */` |
|     2839 |  1909 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   110877 |  1910 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   108043 |  1911 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   108043 |  1912 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1913 | `			return rc;` |
|        - |  1914 | `		}` |
|        5 |  1915 | `	}` |
|        - |  1916 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2839 |  1917 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1918 | `	/* VM is ready for bytecode execution */` |
|     2839 |  1919 | `	return SXRET_OK;` |
|     1422 |  1920 |  |
|        - |  1921 | `/*` |
|        - |  1922 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|        - |  1923 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|        - |  1924 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|        - |  1925 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|        - |  1926 | ` * a dangling node pointer in some other object's reference record.` |
|        - |  1927 | ` */` |
|        6 |  1928 | `static void VmResetRefTable(ph7_vm *pVm)` |
|      ! 0 |  1929 |  |
|        - |  1930 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|        - |  1931 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|        - |  1932 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|      204 |  1933 | `	while( pVm->pRefList ){` |
|      198 |  1934 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|      ! 0 |  1935 | `	}` |
|        6 |  1936 |  |
|        - |  1937 | `/*` |
|        - |  1938 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|        - |  1939 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|        - |  1940 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|        - |  1941 | ` */` |
|       48 |  1942 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|      ! 0 |  1943 |  |
|       48 |  1944 | `	PH7_MemObjRelease(pObj);` |
|       48 |  1945 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|       48 |  1946 |  |
|        - |  1947 | `/*` |
|        - |  1948 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|        - |  1949 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|        - |  1950 | ` * of statics).` |
|        - |  1951 | ` */` |
|      380 |  1952 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|      ! 0 |  1953 |  |
|      380 |  1954 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|        - |  1955 | `	sxu32 k;` |
|      384 |  1956 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|        4 |  1957 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|        2 |  1958 | `	}` |
|      380 |  1959 |  |
|        - |  1960 | `/*` |
|        - |  1961 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|        - |  1962 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|        - |  1963 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|        - |  1964 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|        - |  1965 | ` *    captured environment values, its name buffer and its structure (the` |
|        - |  1966 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|        - |  1967 | ` *    freed). Its template-shared static sentinels are reset too.` |
|        - |  1968 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|        - |  1969 | ` *    has its static sentinels reset.` |
|        - |  1970 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|        - |  1971 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|        - |  1972 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|        - |  1973 | ` */` |
|        6 |  1974 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|      ! 0 |  1975 |  |
|        - |  1976 | `	SyHashEntry *pEntry;` |
|        6 |  1977 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|      386 |  1978 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|      380 |  1979 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      380 |  1980 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|        - |  1981 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|        - |  1982 | `			 * release its captured-by-value environment, then free the entry,` |
|        - |  1983 | `			 * name buffer and structure. */` |
|        4 |  1984 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        4 |  1985 | `			const char *zName = SyStringData(&pFunc->sName);` |
|        - |  1986 | `			sxu32 k;` |
|        4 |  1987 | `			VmResetFuncStatics(pFunc);` |
|        8 |  1988 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|        4 |  1989 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|        2 |  1990 | `			}` |
|        4 |  1991 | `			SySetRelease(&pFunc->aClosureEnv);` |
|        - |  1992 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|        4 |  1993 | `			SyHashDeleteEntry2(pEntry);` |
|        4 |  1994 | `			if( zName ){` |
|        4 |  1995 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|        2 |  1996 | `			}` |
|        4 |  1997 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|        4 |  1998 | `			continue;` |
|        - |  1999 | `		}` |
|        - |  2000 | `		/* Named function: reset statics for every overload sharing this name. */` |
|      752 |  2001 | `		while( pFunc ){` |
|      376 |  2002 | `			VmResetFuncStatics(pFunc);` |
|      376 |  2003 | `			pFunc = pFunc->pNextName;` |
|      ! 0 |  2004 | `		}` |
|      ! 0 |  2005 | `	}` |
|        6 |  2006 | `	pVm->closure_cnt = 0;` |
|        6 |  2007 |  |
|        - |  2008 | `/*` |
|        - |  2009 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|        - |  2010 | ` * are already gone (each object's destructor removed its own during the object` |
|        - |  2011 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|        - |  2012 | ` * the class re-mount registers fresh ones.` |
|        - |  2013 | ` */` |
|        6 |  2014 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|      ! 0 |  2015 |  |
|        - |  2016 | `	SyHashEntry *pEntry;` |
|        - |  2017 | `	/* Common case: no class static typed properties — table already empty. */` |
|        6 |  2018 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        2 |  2019 | `		return;` |
|        - |  2020 | `	}` |
|        - |  2021 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|        - |  2022 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|        4 |  2023 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|       10 |  2024 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|        4 |  2025 | `		if( pEntry->pUserData ){` |
|        4 |  2026 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|        2 |  2027 | `		}` |
|      ! 0 |  2028 | `	}` |
|        4 |  2029 | `	SyHashRelease(&pVm->hTypedSlot);` |
|        4 |  2030 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|        3 |  2031 |  |
|        - |  2032 | `/*` |
|        - |  2033 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|        - |  2034 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|        - |  2035 | ` *` |
|        - |  2036 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|        - |  2037 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|        - |  2038 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|        - |  2039 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|        - |  2040 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|        - |  2041 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|        - |  2042 | ` *` |
|        - |  2043 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|        - |  2044 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|        - |  2045 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|        - |  2046 | ` * exception/error-handler state, the reference table and every object/array` |
|        - |  2047 | ` * reserved during the run.` |
|        - |  2048 | ` *` |
|        - |  2049 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|        - |  2050 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|        - |  2051 | ` * global-scope destructors never fired.` |
|        - |  2052 | ` */` |
|        6 |  2053 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  2054 |  |
|        - |  2055 | `	sxu32 nWater,n;` |
|        6 |  2056 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  2057 | `		return SXERR_CORRUPT;` |
|        - |  2058 | `	}` |
|        6 |  2059 | `	nWater = pVm->nSuperBaseline;` |
|        - |  2060 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|        - |  2061 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        6 |  2062 | `	pVm->pGlobal = 0;` |
|        - |  2063 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|        - |  2064 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|        - |  2065 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|        - |  2066 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        6 |  2067 | `	pVm->bInReset = 1;` |
|        - |  2068 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        6 |  2069 | `	VmResetRefTable(&(*pVm));` |
|        - |  2070 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|        - |  2071 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|        - |  2072 | `	 * function/class registrations and intentionally persist across reuse (a` |
|        - |  2073 | `	 * re-run define() overwrites the value in place). */` |
|        6 |  2074 | `	VmResetFunctionState(&(*pVm));` |
|        - |  2075 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|        - |  2076 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|      218 |  2077 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|      212 |  2078 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      212 |  2079 | `		if( pObj ){` |
|      212 |  2080 | `			PH7_MemObjRelease(pObj);` |
|      106 |  2081 | `		}` |
|      106 |  2082 | `	}` |
|        - |  2083 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|        - |  2084 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        6 |  2085 | `	VmResetTypedSlots(&(*pVm));` |
|        - |  2086 | `	/* (5) Unwind any active frames back to none. */` |
|       12 |  2087 | `	while( pVm->pFrame ){` |
|        6 |  2088 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 |  2089 | `	}` |
|        - |  2090 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        6 |  2091 | `	pVm->bInReset = 0;` |
|        - |  2092 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|        - |  2093 | `	 * slots (their indices no longer exist). */` |
|        6 |  2094 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        6 |  2095 | `	SySetReset(&pVm->aFreeObj);` |
|        - |  2096 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        6 |  2097 | `	SyHashRelease(&pVm->hSuper);` |
|        6 |  2098 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        - |  2099 | `	/* (8) Drain remaining per-exec containers. */` |
|        6 |  2100 | `	SySetReset(&pVm->aSelf);` |
|        - |  2101 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|        - |  2102 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|        - |  2103 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        6 |  2104 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|      ! 0 |  2105 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|      ! 0 |  2106 | `		if( pCB ){` |
|        - |  2107 | `			int iArg;` |
|      ! 0 |  2108 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2109 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|      ! 0 |  2110 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|      ! 0 |  2111 | `			}` |
|      ! 0 |  2112 | `		}` |
|      ! 0 |  2113 | `	}` |
|        6 |  2114 | `	SySetReset(&pVm->aShutdown);` |
|        6 |  2115 | `	SySetReset(&pVm->aException);` |
|        6 |  2116 | `	pVm->pPendingException = 0;` |
|        6 |  2117 | `	pVm->nExceptDepth = 0;` |
|        - |  2118 | `	/* spl_autoload_register() callbacks are per request */` |
|        6 |  2119 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|      ! 0 |  2120 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|      ! 0 |  2121 | `		if( pCB ){` |
|      ! 0 |  2122 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|      ! 0 |  2123 | `		}` |
|      ! 0 |  2124 | `	}` |
|        6 |  2125 | `	SySetReset(&pVm->aAutoload);` |
|        - |  2126 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|        - |  2127 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        6 |  2128 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|      ! 0 |  2129 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|      ! 0 |  2130 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      ! 0 |  2131 | `	}` |
|        - |  2132 | `	/* Output buffers */` |
|        6 |  2133 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|      ! 0 |  2134 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|      ! 0 |  2135 | `		if( pOb ){` |
|      ! 0 |  2136 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|      ! 0 |  2137 | `			SyBlobRelease(&pOb->sOB);` |
|      ! 0 |  2138 | `		}` |
|      ! 0 |  2139 | `	}` |
|        6 |  2140 | `	SySetReset(&pVm->aOB);` |
|        6 |  2141 | `	pVm->nObDepth = 0;` |
|        - |  2142 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|        - |  2143 | `	{` |
|        6 |  2144 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        6 |  2145 | `		if( rc == SXRET_OK ){` |
|        6 |  2146 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|        3 |  2147 | `		}` |
|        6 |  2148 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  2149 | `			return rc;` |
|        - |  2150 | `		}` |
|        - |  2151 | `	}` |
|        - |  2152 | `	/* (10) Re-mount the static/const attribute slots of every class. */` |
|        - |  2153 | `	{` |
|        - |  2154 | `		SyHashEntry *pEntry;` |
|        6 |  2155 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      238 |  2156 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      232 |  2157 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      232 |  2158 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2159 | `				return rc;` |
|        - |  2160 | `			}` |
|      ! 0 |  2161 | `		}` |
|        - |  2162 | `	}` |
|        - |  2163 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        6 |  2164 | `	SyBlobReset(&pVm->sConsumer);` |
|        6 |  2165 | `	pVm->nOutputLen = 0;` |
|        6 |  2166 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        6 |  2167 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        6 |  2168 | `	pVm->iResponseStatus = 200;` |
|        6 |  2169 | `	pVm->bHeadersSent = 0;` |
|        6 |  2170 | `	pVm->bHttpContext = 0;` |
|        6 |  2171 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[0]);` |
|        6 |  2172 | `	VmReinitMemObj(&(*pVm),&pVm->aExceptionCB[1]);` |
|        6 |  2173 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[0]);` |
|        6 |  2174 | `	VmReinitMemObj(&(*pVm),&pVm->aErrCB[1]);` |
|        6 |  2175 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|        6 |  2176 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  2177 | `#ifdef PH7_ENABLE_PCRE` |
|        6 |  2178 | `	pVm->iPcreLastError = 0;` |
|        - |  2179 | `#endif` |
|        6 |  2180 | `	pVm->iCmpCallbackExc = 0;` |
|        6 |  2181 | `	pVm->bReturnRequested = 0;` |
|        6 |  2182 | `	VmReinitMemObj(&(*pVm),&pVm->sCatchReturn);` |
|        6 |  2183 | `	pVm->bHaltRequested = 0;` |
|        6 |  2184 | `	pVm->iExitStatus = 0;` |
|        6 |  2185 | `	pVm->iSpreadExtra = 0;` |
|        6 |  2186 | `	pVm->nRecursionDepth = 0;` |
|        6 |  2187 | `	pVm->pActiveCtx = 0;` |
|        6 |  2188 | `	pVm->pCoalesceObj = 0;` |
|        6 |  2189 | `	pVm->bCoalesceArmed = 0;` |
|        6 |  2190 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|        - |  2191 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        6 |  2192 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  2193 | `	/* Set the ready flag */` |
|        6 |  2194 | `	pVm->nMagic = PH7_VM_RUN;` |
|        6 |  2195 | `	return SXRET_OK;` |
|        3 |  2196 |  |
|        - |  2197 | `/*` |
|        - |  2198 | ` * Release a Virtual Machine.` |
|        - |  2199 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  2200 | ` */` |
|     2834 |  2201 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        5 |  2202 |  |
|        - |  2203 | `	/* Set the stale magic number */` |
|     2839 |  2204 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  2205 | `	/* Release the private memory subsystem */` |
|     2839 |  2206 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2839 |  2207 | `	return SXRET_OK;` |
|        5 |  2208 |  |
|        - |  2209 | `/*` |
|        - |  2210 | ` * Initialize a foreign function call context.` |
|        - |  2211 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  2212 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  2213 | ` * functions.` |
|        - |  2214 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  2215 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  2216 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  2217 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  2218 | ` */` |
|   705398 |  2219 | `static sxi32 VmInitCallContext(` |
|        - |  2220 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  2221 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  2222 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  2223 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  2224 | `	sxi32 iFlags          /* Control flags */` |
|        - |  2225 | `	)` |
|        5 |  2226 |  |
|   705403 |  2227 | `	pOut->pFunc = pFunc;` |
|   705403 |  2228 | `	pOut->pVm   = pVm;` |
|   705403 |  2229 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   705403 |  2230 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  2231 | `	/* Assume a null return value */` |
|   705403 |  2232 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   705403 |  2233 | `	pOut->pRet = pRet;` |
|   705403 |  2234 | `	pOut->iFlags = iFlags;` |
|   705403 |  2235 | `	return SXRET_OK;` |
|        5 |  2236 |  |
|        - |  2237 | `/*` |
|        - |  2238 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  2239 | ` * left behind.` |
|        - |  2240 | ` */` |
|   705398 |  2241 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        5 |  2242 |  |
|        - |  2243 | `	sxu32 n;` |
|   705403 |  2244 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8799 |  2245 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    25761 |  2246 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16967 |  2247 | `			if( apObj[n] == 0 ){` |
|        - |  2248 | `				/* Already released */` |
|      387 |  2249 | `				continue;` |
|        - |  2250 | `			}` |
|    16585 |  2251 | `			PH7_MemObjRelease(apObj[n]);` |
|    16585 |  2252 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     8295 |  2253 | `		}` |
|     8799 |  2254 | `		SySetRelease(&pCtx->sVar);` |
|     4397 |  2255 | `	}` |
|   705403 |  2256 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  2257 | `		ph7_aux_data *aAux;` |
|        - |  2258 | `		void *pChunk;` |
|        - |  2259 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  2260 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  2261 | `		 */` |
|        9 |  2262 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  2263 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  2264 | `			pChunk = aAux[n].pAuxData;` |
|        - |  2265 | `			/* Release the chunk */` |
|       25 |  2266 | `			if( pChunk ){` |
|       25 |  2267 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  2268 | `			}` |
|       13 |  2269 | `		}` |
|        9 |  2270 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  2271 | `	}` |
|   705403 |  2272 |  |
|        - |  2273 | `/*` |
|        - |  2274 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  2275 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  2276 | ` */` |
|      382 |  2277 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  2278 | `	ph7_context *pCtx, /* Call context */` |
|        - |  2279 | `	ph7_value *pValue  /* Release this value */` |
|        - |  2280 | `	)` |
|        5 |  2281 |  |
|      387 |  2282 | `	if( pValue == 0 ){` |
|        - |  2283 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  2284 | `		return;` |
|        - |  2285 | `	}` |
|      387 |  2286 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      387 |  2287 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  2288 | `		sxu32 n;` |
|     1285 |  2289 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1285 |  2290 | `			if( apObj[n] == pValue ){` |
|      387 |  2291 | `				PH7_MemObjRelease(pValue);` |
|      387 |  2292 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  2293 | `				/* Mark as released */` |
|      387 |  2294 | `				apObj[n] = 0;` |
|      387 |  2295 | `				break;` |
|        - |  2296 | `			}` |
|      454 |  2297 | `		}` |
|      191 |  2298 | `	}` |
|      196 |  2299 |  |
|        - |  2300 | `/*` |
|        - |  2301 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  2302 | ` */` |
|  3984044 |  2303 | `static void VmPopOperand(` |
|        - |  2304 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  2305 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  2306 | `	)` |
|        5 |  2307 |  |
|  3984049 |  2308 | `	ph7_value *pTos = *ppTos;` |
|  8490671 |  2309 | `	while( nPop > 0 ){` |
|  4506627 |  2310 | `		PH7_MemObjRelease(pTos);` |
|  4506627 |  2311 | `		pTos--;` |
|  4506627 |  2312 | `		nPop--;` |
|        5 |  2313 | `	}` |
|        - |  2314 | `	/* Top of the stack */` |
|  3984049 |  2315 | `	*ppTos = pTos;` |
|  3984049 |  2316 |  |
|        - |  2317 | `/*` |
|        - |  2318 | ` * Reserve a memory object.` |
|        - |  2319 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  2320 | ` */` |
|  3219160 |  2321 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        5 |  2322 |  |
|  3219165 |  2323 | `	ph7_value *pObj = 0;` |
|        - |  2324 | `	VmSlot *pSlot;` |
|        - |  2325 | `	sxu32 nIdx;` |
|        - |  2326 | `	/* Check for a free slot */` |
|  3219165 |  2327 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3219165 |  2328 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3219165 |  2329 | `	if( pSlot ){` |
|  1066963 |  2330 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1066963 |  2331 | `		nIdx = pSlot->nIdx;` |
|   533479 |  2332 | `	}` |
|  3219165 |  2333 | `	if( pObj == 0 ){` |
|        - |  2334 | `		/* Reserve a new memory object */` |
|  2152207 |  2335 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2152207 |  2336 | `		if( pObj == 0 ){` |
|      ! 0 |  2337 | `			return 0;` |
|        - |  2338 | `		}` |
|  1076101 |  2339 | `	}` |
|        - |  2340 | `	/* Set a null default value */` |
|  3219165 |  2341 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3219165 |  2342 | `	pObj->nIdx = nIdx;` |
|  3219165 |  2343 | `	return pObj;` |
|  1609585 |  2344 |  |
|        - |  2345 | `/*` |
|        - |  2346 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  2347 | ` */` |
|    35554 |  2348 | `static sxi32 VmHashmapRefInsert(` |
|        - |  2349 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  2350 | `	const char *zKey,  /* Entry key */` |
|        - |  2351 | `	sxu32 nByte,       /* Key length */` |
|        - |  2352 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  2353 | `	)` |
|        5 |  2354 |  |
|        - |  2355 | `	ph7_value sKey;` |
|        - |  2356 | `	sxi32 rc;` |
|    35559 |  2357 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    35559 |  2358 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  2359 | `	/* Perform the insertion */` |
|    35559 |  2360 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    35559 |  2361 | `	PH7_MemObjRelease(&sKey);` |
|    35559 |  2362 | `	return rc;` |
|        5 |  2363 |  |
|        - |  2364 | `/*` |
|        - |  2365 | ` * Extract a variable value from the top active VM frame.` |
|        - |  2366 | ` * Return a pointer to the variable value on success.` |
|        - |  2367 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  2368 | ` */` |
|  3697186 |  2369 | `static ph7_value * VmExtractMemObj(` |
|        - |  2370 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  2371 | `	const SyString *pName, /* Variable name */` |
|        - |  2372 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  2373 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  2374 | `	)` |
|        5 |  2375 |  |
|  3697191 |  2376 | `	int bNullify = FALSE;` |
|        - |  2377 | `	SyHashEntry *pEntry;` |
|        - |  2378 | `	VmFrame *pFrame;` |
|        - |  2379 | `	ph7_value *pObj;` |
|        - |  2380 | `	sxu32 nIdx;` |
|        - |  2381 | `	sxi32 rc;` |
|        - |  2382 | `	/* Point to the top active frame */` |
|  3697191 |  2383 | `	pFrame = pVm->pFrame;` |
|  3697191 |  2384 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  2385 | `	/* Perform the lookup */` |
|  3697191 |  2386 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2387 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2388 | `		pName = &sAnnon;` |
|        - |  2389 | `		/* Always nullify the object */` |
|      ! 0 |  2390 | `		bNullify = TRUE;` |
|      ! 0 |  2391 | `		bDup = FALSE;` |
|      ! 0 |  2392 | `	}` |
|        - |  2393 | `	/* Check the superglobals table first */` |
|  3697191 |  2394 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3697191 |  2395 | `	if( pEntry == 0 ){` |
|        - |  2396 | `		/* Query the top active frame */` |
|  3697145 |  2397 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3697145 |  2398 | `		if( pEntry == 0 ){` |
|   114815 |  2399 | `			char *zName = (char *)pName->zString;` |
|        - |  2400 | `			VmSlot sLocal;` |
|   114815 |  2401 | `			if( !bCreate ){` |
|        - |  2402 | `				/* Do not create the variable,return NULL instead */` |
|      987 |  2403 | `				return 0;` |
|        - |  2404 | `			}` |
|        - |  2405 | `			/* No such variable,automatically create a new one and install` |
|        - |  2406 | `			 * it in the current frame.` |
|        - |  2407 | `			 */` |
|   113833 |  2408 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   113833 |  2409 | `			if( pObj == 0 ){` |
|      ! 0 |  2410 | `				return 0;` |
|        - |  2411 | `			}` |
|   113833 |  2412 | `			nIdx = pObj->nIdx;` |
|   113833 |  2413 | `			if( bDup ){` |
|        - |  2414 | `				/* Duplicate name */` |
|      232 |  2415 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      232 |  2416 | `				if( zName == 0 ){` |
|      ! 0 |  2417 | `					return 0;` |
|        - |  2418 | `				}` |
|      114 |  2419 | `			}` |
|        - |  2420 | `			/* Link to the top active VM frame */` |
|   113833 |  2421 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   113833 |  2422 | `			if( rc != SXRET_OK ){` |
|        - |  2423 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2424 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2425 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2426 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2427 | `				return 0;` |
|        - |  2428 | `			}` |
|   113833 |  2429 | `			if( pFrame->pParent != 0 ){` |
|        - |  2430 | `				/* Local variable */` |
|   106711 |  2431 | `				sLocal.nIdx = nIdx;` |
|   106711 |  2432 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    53358 |  2433 | `			}else{` |
|        - |  2434 | `				/* Register in the $GLOBALS array */` |
|     7127 |  2435 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2436 | `			}` |
|        - |  2437 | `			/* Install in the reference table */` |
|   113833 |  2438 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2439 | `			/* Save object index */` |
|   113833 |  2440 | `			pObj->nIdx = nIdx;` |
|    56919 |  2441 | `		}else{` |
|        - |  2442 | `			/* Extract variable contents */` |
|  3582335 |  2443 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3582335 |  2444 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3582335 |  2445 | `			if( bNullify && pObj ){` |
|      ! 0 |  2446 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2447 | `			}` |
|        - |  2448 | `		}` |
|  1848194 |  2449 | `	}else{` |
|        - |  2450 | `		/* Superglobal */` |
|       51 |  2451 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       51 |  2452 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2453 | `	}` |
|  3696209 |  2454 | `	return pObj;` |
|  1848708 |  2455 |  |
|        - |  2456 | `/*` |
|        - |  2457 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2458 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2459 | ` */` |
|     3264 |  2460 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2461 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2462 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2463 | `	sxu32 nByte        /* zName length */` |
|        - |  2464 | `	)` |
|        5 |  2465 |  |
|        - |  2466 | `	SyHashEntry *pEntry;` |
|        - |  2467 | `	ph7_value *pValue;` |
|        - |  2468 | `	sxu32 nIdx;` |
|        - |  2469 | `	/* Query the superglobal table */` |
|     3269 |  2470 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3269 |  2471 | `	if( pEntry == 0 ){` |
|        - |  2472 | `		/* No such entry */` |
|      ! 0 |  2473 | `		return 0;` |
|        - |  2474 | `	}` |
|        - |  2475 | `	/* Extract the superglobal index in the global object pool */` |
|     3269 |  2476 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2477 | `	/* Extract the variable value  */` |
|     3269 |  2478 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3269 |  2479 | `	return pValue;` |
|     1637 |  2480 |  |
|        - |  2481 | `/*` |
|        - |  2482 | ` * Perform a raw hashmap insertion.` |
|        - |  2483 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2484 | ` */` |
|     3306 |  2485 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2486 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2487 | `	const char *zKey,   /* Entry key */` |
|        - |  2488 | `	int nKeylen,        /* zKey length*/` |
|        - |  2489 | `	const char *zData,  /* Entry data */` |
|        - |  2490 | `	int nLen            /* zData length */` |
|        - |  2491 | `	)` |
|        5 |  2492 |  |
|        - |  2493 | `	ph7_value sKey,sValue;` |
|        - |  2494 | `	sxi32 rc;` |
|     3311 |  2495 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3311 |  2496 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3311 |  2497 | `	if( zKey ){` |
|     3289 |  2498 | `		if( nKeylen < 0 ){` |
|     3207 |  2499 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1601 |  2500 | `		}` |
|     3289 |  2501 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1642 |  2502 | `	}` |
|     3311 |  2503 | `	if( zData ){` |
|     3311 |  2504 | `		if( nLen < 0 ){` |
|        - |  2505 | `			/* Compute length automatically */` |
|      198 |  2506 | `			nLen = (int)SyStrlen(zData);` |
|       99 |  2507 | `		}` |
|     3311 |  2508 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1653 |  2509 | `	}` |
|        - |  2510 | `	/* Perform the insertion */` |
|     3311 |  2511 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3311 |  2512 | `	PH7_MemObjRelease(&sKey);` |
|     3311 |  2513 | `	PH7_MemObjRelease(&sValue);` |
|     3311 |  2514 | `	return rc;` |
|        5 |  2515 |  |
|        - |  2516 | `/*` |
|        - |  2517 | ` * Configure a working virtual machine instance.` |
|        - |  2518 | ` *` |
|        - |  2519 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2520 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2521 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2522 | ` * The second argument to this function is an integer configuration option` |
|        - |  2523 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2524 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2525 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2526 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2527 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2528 | ` */` |
|    45872 |  2529 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2530 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2531 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2532 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2533 | `	)` |
|        5 |  2534 |  |
|    45877 |  2535 | `	sxi32 rc = SXRET_OK;` |
|    45877 |  2536 | `	switch(nOp){` |
|     1409 |  2537 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2823 |  2538 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2823 |  2539 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2540 | `		/* VM output consumer callback */` |
|        - |  2541 | `#ifdef UNTRUST` |
|        - |  2542 | `		if( xConsumer == 0 ){` |
|        - |  2543 | `			rc = SXERR_CORRUPT;` |
|        - |  2544 | `			break;` |
|        - |  2545 | `		}` |
|        - |  2546 | `#endif` |
|        - |  2547 | `		/* Install the output consumer */` |
|     2823 |  2548 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2823 |  2549 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2823 |  2550 | `		break;` |
|        - |  2551 | `							   }` |
|     1417 |  2552 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2553 | `		/* Import path */` |
|        - |  2554 | `		  const char *zPath;` |
|        - |  2555 | `		  SyString sPath;` |
|     2839 |  2556 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2557 | `#if defined(UNTRUST)` |
|        - |  2558 | `		  if( zPath == 0 ){` |
|        - |  2559 | `			  rc = SXERR_EMPTY;` |
|        - |  2560 | `			  break;` |
|        - |  2561 | `		  }` |
|        - |  2562 | `#endif` |
|     2839 |  2563 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2564 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2565 | `#ifdef __WINNT__` |
|        5 |  2566 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2567 | `#endif` |
|     5673 |  2568 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2569 | `		  /* Remove leading and trailing white spaces */` |
|     2839 |  2570 | `		  SyStringFullTrim(&sPath);` |
|     2839 |  2571 | `		  if( sPath.nByte > 0 ){` |
|        - |  2572 | `			  /* Store the path in the corresponding conatiner */` |
|     2839 |  2573 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1417 |  2574 | `		  }` |
|     2839 |  2575 | `		  break;` |
|        - |  2576 | `									 }` |
|     1420 |  2577 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2578 | `		/* Run-Time Error report */` |
|     2845 |  2579 | `		pVm->bErrReport = 1;` |
|     2845 |  2580 | `		break;` |
|      ! 0 |  2581 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2582 | `		/* Recursion depth */` |
|      ! 0 |  2583 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2584 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2585 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2586 | `		}` |
|      ! 0 |  2587 | `		break;` |
|        - |  2588 | `									   }` |
|      ! 0 |  2589 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2590 | `		/* VM output length in bytes */` |
|      ! 0 |  2591 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2592 | `#ifdef UNTRUST` |
|        - |  2593 | `		if( pOut == 0 ){` |
|        - |  2594 | `			rc = SXERR_CORRUPT;` |
|        - |  2595 | `			break;` |
|        - |  2596 | `		}` |
|        - |  2597 | `#endif` |
|      ! 0 |  2598 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2599 | `		break;` |
|        - |  2600 | `							   }` |
|        - |  2601 |  |
|    14200 |  2602 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2603 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2604 | `		/* Create a new superglobal/global variable */` |
|    28405 |  2605 | `		const char *zName = va_arg(ap,const char *);` |
|    28405 |  2606 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2607 | `		SyHashEntry *pEntry;` |
|        - |  2608 | `		ph7_value *pObj;` |
|        - |  2609 | `		sxu32 nByte;` |
|        - |  2610 | `		sxu32 nIdx;` |
|        - |  2611 | `#ifdef UNTRUST` |
|        - |  2612 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2613 | `			rc = SXERR_CORRUPT;` |
|        - |  2614 | `			break;` |
|        - |  2615 | `		}` |
|        - |  2616 | `#endif` |
|    28405 |  2617 | `		nByte = SyStrlen(zName);` |
|    28405 |  2618 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2619 | `			/* Check if the superglobal is already installed */` |
|    28405 |  2620 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14205 |  2621 | `		}else{` |
|        - |  2622 | `			/* Query the top active VM frame */` |
|      ! 0 |  2623 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2624 | `		}` |
|    28405 |  2625 | `		if( pEntry ){` |
|        - |  2626 | `			/* Variable already installed */` |
|      ! 0 |  2627 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2628 | `			/* Extract contents */` |
|      ! 0 |  2629 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2630 | `			if( pObj ){` |
|        - |  2631 | `				/* Overwrite old contents */` |
|      ! 0 |  2632 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2633 | `			}` |
|      ! 0 |  2634 | `		}else{` |
|        - |  2635 | `			/* Install a new variable */` |
|    28405 |  2636 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28405 |  2637 | `			if( pObj == 0 ){` |
|      ! 0 |  2638 | `				rc = SXERR_MEM;` |
|      ! 0 |  2639 | `				break;` |
|        - |  2640 | `			}` |
|    28405 |  2641 | `			nIdx = pObj->nIdx;` |
|        - |  2642 | `			/* Copy value */` |
|    28405 |  2643 | `			PH7_MemObjStore(pValue,pObj);` |
|    28405 |  2644 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2645 | `				/* Install the superglobal */` |
|    28405 |  2646 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14205 |  2647 | `			}else{` |
|        - |  2648 | `				/* Install in the current frame */` |
|      ! 0 |  2649 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2650 | `			}` |
|    28405 |  2651 | `			if( rc == SXRET_OK ){` |
|        - |  2652 | `				SyHashEntry *pRef;` |
|    28405 |  2653 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28405 |  2654 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14205 |  2655 | `				}else{` |
|      ! 0 |  2656 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2657 | `				}` |
|        - |  2658 | `				/* Install in the reference table */` |
|    28405 |  2659 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28405 |  2660 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2661 | `					/* Register in the $GLOBALS array */` |
|    28405 |  2662 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14200 |  2663 | `				}` |
|    14200 |  2664 | `			}` |
|        - |  2665 | `		}` |
|    28405 |  2666 | `		break;` |
|        - |  2667 | `									}` |
|     1601 |  2668 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2669 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2670 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2671 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2672 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2673 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2674 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3207 |  2675 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3207 |  2676 | `		const char *zValue = va_arg(ap,const char *);` |
|     3207 |  2677 | `		int nLen = va_arg(ap,int);` |
|        - |  2678 | `		ph7_hashmap *pMap;` |
|        - |  2679 | `		ph7_value *pValue;` |
|     3207 |  2680 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2681 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2682 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3206 |  2683 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2684 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2685 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3205 |  2686 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2687 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2688 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3205 |  2689 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2690 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2691 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3205 |  2692 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2693 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2694 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3205 |  2695 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2696 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2697 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2698 | `		}else{` |
|        - |  2699 | `			/* Extract the $_SERVER superglobal */` |
|     3205 |  2700 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2701 | `		}` |
|     3207 |  2702 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2703 | `			/* No such entry */` |
|      ! 0 |  2704 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2705 | `			break;` |
|        - |  2706 | `		}` |
|        - |  2707 | `		/* Point to the hashmap */` |
|     3207 |  2708 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2709 | `		/* Perform the insertion */` |
|     3207 |  2710 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3207 |  2711 | `		break;` |
|        - |  2712 | `								   }` |
|       11 |  2713 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2714 | `		/* Script arguments */` |
|       27 |  2715 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2716 | `		ph7_hashmap *pMap;` |
|        - |  2717 | `		ph7_value *pValue;` |
|        - |  2718 | `		sxu32 n;` |
|       27 |  2719 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2720 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2721 | `			break;` |
|        - |  2722 | `		}` |
|        - |  2723 | `		/* Extract the $argv array */` |
|       27 |  2724 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       27 |  2725 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2726 | `			/* No such entry */` |
|      ! 0 |  2727 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2728 | `			break;` |
|        - |  2729 | `		}` |
|        - |  2730 | `		/* Point to the hashmap */` |
|       27 |  2731 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2732 | `		/* Perform the insertion */` |
|       27 |  2733 | `		n = (sxu32)SyStrlen(zValue);` |
|       27 |  2734 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       27 |  2735 | `		if( rc == SXRET_OK ){` |
|       27 |  2736 | `			if( pMap->nEntry > 1 ){` |
|        - |  2737 | `				/* Append space separator first */` |
|       21 |  2738 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2739 | `			}` |
|       27 |  2740 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2741 | `		}` |
|       27 |  2742 | `		break;` |
|        - |  2743 | `								  }` |
|      ! 0 |  2744 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2745 | `		/* error_log() consumer */` |
|      ! 0 |  2746 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2747 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2748 | `		break;` |
|        - |  2749 | `										}` |
|      ! 0 |  2750 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2751 | `		/* Script return value */` |
|      ! 0 |  2752 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2753 | `#ifdef UNTRUST` |
|        - |  2754 | `		if( ppValue == 0 ){` |
|        - |  2755 | `			rc = SXERR_CORRUPT;` |
|        - |  2756 | `			break;` |
|        - |  2757 | `		}` |
|        - |  2758 | `#endif` |
|      ! 0 |  2759 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2760 | `		break;` |
|        - |  2761 | `								   }` |
|     2834 |  2762 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2763 | `		/* Register an IO stream device */` |
|     5673 |  2764 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2765 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8502 |  2766 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5673 |  2767 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2768 | `				/* Invalid stream */` |
|      ! 0 |  2769 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2770 | `				break;` |
|        - |  2771 | `		}` |
|     5673 |  2772 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2773 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2839 |  2774 | `			pVm->pDefStream = pStream;` |
|     1417 |  2775 | `		}` |
|        - |  2776 | `		/* Insert in the appropriate container */` |
|     5673 |  2777 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5673 |  2778 | `		break;` |
|        - |  2779 | `								  }` |
|       11 |  2780 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2781 | `		/* Point to the VM internal output consumer buffer */` |
|       22 |  2782 | `		const void **ppOut = va_arg(ap,const void **);` |
|       22 |  2783 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2784 | `#ifdef UNTRUST` |
|        - |  2785 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2786 | `			rc = SXERR_CORRUPT;` |
|        - |  2787 | `			break;` |
|        - |  2788 | `		}` |
|        - |  2789 | `#endif` |
|       22 |  2790 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       22 |  2791 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       22 |  2792 | `		break;` |
|        - |  2793 | `									   }` |
|       11 |  2794 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2795 | `		/* Raw HTTP request*/` |
|       22 |  2796 | `		const char *zRequest = va_arg(ap,const char *);` |
|       22 |  2797 | `		int nByte = va_arg(ap,int);` |
|       22 |  2798 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2799 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2800 | `			break;` |
|        - |  2801 | `		}` |
|       22 |  2802 | `		if( nByte < 0 ){` |
|        - |  2803 | `			/* Compute length automatically */` |
|      ! 0 |  2804 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2805 | `		}` |
|        - |  2806 | `		/* Process the request */` |
|       22 |  2807 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2808 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       22 |  2809 | `		if( rc == SXRET_OK ){` |
|       22 |  2810 | `			pVm->bHttpContext = 1;` |
|       11 |  2811 | `		}` |
|       22 |  2812 | `		break;` |
|        - |  2813 | `									}` |
|       11 |  2814 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2815 | `		/* Extract HTTP response status code */` |
|       22 |  2816 | `		int *pStatus = va_arg(ap, int *);` |
|       22 |  2817 | `		if( pStatus ){` |
|       22 |  2818 | `			*pStatus = pVm->iResponseStatus;` |
|       11 |  2819 | `		}` |
|       22 |  2820 | `		break;` |
|        - |  2821 | `										}` |
|       11 |  2822 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2823 | `		/* Iterate response headers via callback */` |
|        - |  2824 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       22 |  2825 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       22 |  2826 | `		void *pUserData = va_arg(ap, void *);` |
|       22 |  2827 | `		if( xCallback ){` |
|       22 |  2828 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       22 |  2829 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       34 |  2830 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2831 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2832 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2833 | `							   pUserData);` |
|       12 |  2834 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2835 | `					break;` |
|        - |  2836 | `				}` |
|        6 |  2837 | `			}` |
|       11 |  2838 | `		}` |
|       22 |  2839 | `		break;` |
|        - |  2840 | `										 }` |
|      ! 0 |  2841 | `	default:` |
|        - |  2842 | `		/* Unknown configuration option */` |
|      ! 0 |  2843 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2844 | `		break;` |
|        - |  2845 | `	}` |
|    45877 |  2846 | `	return rc;` |
|        5 |  2847 |  |
|        - |  2848 | `/* Forward declaration */` |
|        - |  2849 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2850 | `/*` |
|        - |  2851 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2852 | ` * format.` |
|        - |  2853 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2854 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2855 | ` * (STDOUT).` |
|        - |  2856 | ` */` |
|        2 |  2857 | `static sxi32 VmByteCodeDump(` |
|        - |  2858 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2859 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2860 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2861 | `	)` |
|        1 |  2862 |  |
|        - |  2863 | `	static const char zDump[] = {` |
|        - |  2864 | `		"====================================================\n"` |
|        - |  2865 | `		"PH7 VM Dump\n"` |
|        - |  2866 | `		"====================================================\n"` |
|        - |  2867 | `	};` |
|        - |  2868 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2869 | `	sxi32 rc = SXRET_OK;` |
|        - |  2870 | `	sxu32 n;` |
|        - |  2871 | `	/* Point to the PH7 instructions */` |
|        3 |  2872 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2873 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2874 | `	n = 0;` |
|        3 |  2875 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2876 | `	/* Dump instructions */` |
|        7 |  2877 | `	for(;;){` |
|       15 |  2878 | `		if( pInstr >= pEnd ){` |
|        - |  2879 | `			/* No more instructions */` |
|        3 |  2880 | `			break;` |
|        - |  2881 | `		}` |
|        - |  2882 | `		/* Format and call the consumer callback */` |
|       19 |  2883 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2884 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2885 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2886 | `		if( rc != SXRET_OK ){` |
|        - |  2887 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2888 | `			return rc;` |
|        - |  2889 | `		}` |
|       13 |  2890 | `		++n;` |
|       13 |  2891 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2892 | `	}` |
|        3 |  2893 | `	return rc;` |
|        2 |  2894 |  |
|        - |  2895 | `/* Forward declaration */` |
|        - |  2896 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2897 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2898 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2899 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2900 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2901 | `/*` |
|        - |  2902 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2903 | ` * consumer callback.` |
|        - |  2904 | ` */` |
|      604 |  2905 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        4 |  2906 |  |
|      608 |  2907 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      608 |  2908 | `	sxi32 rc = SXRET_OK;` |
|        - |  2909 | `	/* Append a new line */` |
|        - |  2910 | `#ifdef __WINNT__` |
|        4 |  2911 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2912 | `#else` |
|      604 |  2913 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2914 | `#endif` |
|        - |  2915 | `	/* Invoke the output consumer callback */` |
|      608 |  2916 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      608 |  2917 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      608 |  2918 | `	return rc;` |
|        4 |  2919 |  |
|        - |  2920 | `/*` |
|        - |  2921 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2922 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2923 | ` * information.` |
|        - |  2924 | ` */` |
|      152 |  2925 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |  2926 |  |
|      157 |  2927 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2928 | `		ph7_value apArg[4];` |
|        - |  2929 | `		ph7_value *apArgPtr[4];` |
|        - |  2930 | `		ph7_value sResult;` |
|        - |  2931 | `		SyString sErr;` |
|        - |  2932 | `		/* Prepare arguments */` |
|       76 |  2933 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2934 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2935 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2936 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2937 | `		if( pFile ){` |
|       76 |  2938 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2939 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2940 | `		}else{` |
|      ! 0 |  2941 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2942 | `		}` |
|       76 |  2943 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2944 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2945 | `		/* Set up pointer array */` |
|       76 |  2946 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2947 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2948 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2949 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2950 | `		/* Call the handler */` |
|       76 |  2951 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2952 | `		/* Check return value */` |
|       76 |  2953 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2954 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2955 | `		}` |
|        - |  2956 | `		/* Release */` |
|       76 |  2957 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2958 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2959 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2960 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2961 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2962 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2963 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2964 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2965 | `	}` |
|        - |  2966 | `	/* No handler, always call error handler */` |
|       82 |  2967 | `	return TRUE;` |
|       81 |  2968 |  |
|      110 |  2969 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2970 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2971 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2972 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2973 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2974 | `	)` |
|        5 |  2975 |  |
|      115 |  2976 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2977 | `	SyString *pFile;` |
|        - |  2978 | `	char *zErr;` |
|      115 |  2979 | `	sxi32 rc = SXRET_OK;` |
|      115 |  2980 | `	if( !pVm->bErrReport ){` |
|        - |  2981 | `		/* Don't bother reporting errors */` |
|        3 |  2982 | `		return SXRET_OK;` |
|        - |  2983 | `	}` |
|        - |  2984 | `	/* Reset the working buffer */` |
|      113 |  2985 | `	SyBlobReset(pWorker);` |
|        - |  2986 | `	/* Peek the processed file if available */` |
|      113 |  2987 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      113 |  2988 | `	if( pFile ){` |
|        - |  2989 | `		/* Append file name */` |
|      113 |  2990 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      113 |  2991 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2992 | `	}` |
|        - |  2993 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2994 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2995 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2996 | `	 * E_DEPRECATED). */` |
|      113 |  2997 | `	zErr = "Error:  ";` |
|      113 |  2998 | `	switch(iErr){` |
|       19 |  2999 | `	case PH7_CTX_WARNING:` |
|       42 |  3000 | `		zErr = "Warning:  ";` |
|       42 |  3001 | `		break;` |
|        6 |  3002 | `	case PH7_CTX_NOTICE:` |
|       15 |  3003 | `		zErr = "Notice:  ";` |
|       12 |  3004 | `		break;` |
|       29 |  3005 | `	default:` |
|        - |  3006 | `		/* keep iErr unchanged */` |
|       58 |  3007 | `		break;` |
|        - |  3008 | `	}` |
|      113 |  3009 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      113 |  3010 | `	if( pFuncName ){` |
|        - |  3011 | `		/* Append function name first */` |
|       24 |  3012 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       24 |  3013 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  3014 | `	}` |
|      113 |  3015 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  3016 | `	/* Check for user error handler.  compute length of C string */` |
|      113 |  3017 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       52 |  3018 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  3019 | `	}` |
|      113 |  3020 | `	return rc;` |
|       60 |  3021 |  |
|        - |  3022 | `/*` |
|        - |  3023 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  3024 | ` *` |
|        - |  3025 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  3026 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  3027 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  3028 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  3029 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  3030 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  3031 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  3032 | ` * calling it from a VM op.` |
|        - |  3033 | ` */` |
|      ! 0 |  3034 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  3035 |  |
|      ! 0 |  3036 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  3037 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  3038 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  3039 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  3040 | `	return PH7_ABORT;` |
|      ! 0 |  3041 |  |
|        - |  3042 | `/*` |
|        - |  3043 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  3044 | ` */` |
|      ! 0 |  3045 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  3046 |  |
|      ! 0 |  3047 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  3048 |  |
|        - |  3049 | `/*` |
|        - |  3050 | ` * Single source of truth for the call-recursion cap policy. Each recursion` |
|        - |  3051 | ` * entry point (OP_CALL, eval/include, fibers/generators) tests this before` |
|        - |  3052 | ` * descending another native C frame; the control flow on a hit differs per` |
|        - |  3053 | ` * site, but the rule itself lives here.` |
|        - |  3054 | ` */` |
|    32418 |  3055 | `static int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  3056 |  |
|    32423 |  3057 | `	return pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  3058 |  |
|        - |  3059 | `/*` |
|        - |  3060 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  3061 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  3062 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  3063 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  3064 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  3065 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  3066 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  3067 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  3068 | ` * register_shutdown_function() callbacks. Used by every recursion path —` |
|        - |  3069 | ` * OP_CALL, eval()/include/require (VmEvalChunk) and fibers/generators` |
|        - |  3070 | ` * (VmStartCtx/VmResumeCtx).` |
|        - |  3071 | ` *` |
|        - |  3072 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  3073 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  3074 | ` * re-enter and loop.` |
|        - |  3075 | ` */` |
|        6 |  3076 | `static sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        2 |  3077 |  |
|        8 |  3078 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  3079 | `		return PH7_ABORT;` |
|        - |  3080 | `	}` |
|        8 |  3081 | `	pVm->iExitStatus = 255;` |
|        8 |  3082 | `	pVm->bHaltRequested = 1;` |
|        8 |  3083 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        8 |  3084 | `	return PH7_ABORT;` |
|        5 |  3085 |  |
|        - |  3086 | `/*` |
|        - |  3087 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3088 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3089 | ` * information.` |
|        - |  3090 | ` */` |
|       44 |  3091 | `static sxi32 VmThrowErrorAp(` |
|        - |  3092 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3093 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  3094 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  3095 | `	const char *zFormat, /* Format message */` |
|        - |  3096 | `	va_list ap           /* Variable list of arguments */` |
|        - |  3097 | `	)` |
|        5 |  3098 |  |
|       49 |  3099 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  3100 | `	SyBlob sMsg;` |
|        - |  3101 | `	SyString *pFile;` |
|        - |  3102 | `	char *zErr;` |
|       49 |  3103 | `	sxi32 rc = SXRET_OK;` |
|       49 |  3104 | `	if( !pVm->bErrReport ){` |
|        - |  3105 | `		/* Don't bother reporting errors */` |
|      ! 0 |  3106 | `		return SXRET_OK;` |
|        - |  3107 | `	}` |
|        - |  3108 | `	/* Reset the working buffer */` |
|       49 |  3109 | `	SyBlobReset(pWorker);` |
|        - |  3110 | `	/* Peek the processed file if available */` |
|       49 |  3111 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       49 |  3112 | `	if( pFile ){` |
|        - |  3113 | `		/* Append file name */` |
|       49 |  3114 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       49 |  3115 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       22 |  3116 | `	}` |
|        - |  3117 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  3118 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  3119 | `	 * the correct errno value. */` |
|       49 |  3120 | `	zErr = "Error:  ";` |
|       49 |  3121 | `	switch(iErr){` |
|        4 |  3122 | `	case PH7_CTX_WARNING:` |
|       11 |  3123 | `		zErr = "Warning:  ";` |
|       11 |  3124 | `		break;` |
|        3 |  3125 | `	case PH7_CTX_NOTICE:` |
|        8 |  3126 | `		zErr = "Notice:  ";` |
|        6 |  3127 | `		break;` |
|       15 |  3128 | `	default:` |
|        - |  3129 | `		/* do not change iErr */` |
|       30 |  3130 | `		break;` |
|        - |  3131 | `	}` |
|       49 |  3132 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       49 |  3133 | `	if( pFuncName ){` |
|        - |  3134 | `		/* Append function name first */` |
|       28 |  3135 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       28 |  3136 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  3137 | `	}` |
|        - |  3138 | `	/* Format the raw message */` |
|       49 |  3139 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       49 |  3140 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  3141 | `	/* Check if a user error handler is installed */` |
|       49 |  3142 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  3143 | `		/* No handler or handler returned TRUE, normal processing */` |
|       34 |  3144 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       34 |  3145 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       15 |  3146 | `	}` |
|       49 |  3147 | `	SyBlobRelease(&sMsg);` |
|       49 |  3148 | `	return rc;` |
|       27 |  3149 |  |
|        - |  3150 | `/*` |
|        - |  3151 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  3152 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  3153 | ` * possible.` |
|        - |  3154 | ` */` |
|       42 |  3155 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        5 |  3156 |  |
|        - |  3157 | `	ph7_class *pClass;` |
|       47 |  3158 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  3159 | `	ph7_class_instance *pThis;` |
|        - |  3160 | `	ph7_class_method *pCons;` |
|        - |  3161 | `	ph7_value sArg;` |
|        - |  3162 | `	ph7_value *apArg[1];` |
|        - |  3163 | `	SyBlob sMsg;` |
|        - |  3164 | `	SyString sMsgStr;` |
|        - |  3165 | `	VmFrame *pFrame;` |
|        - |  3166 | `	sxi32 rc;` |
|       47 |  3167 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       47 |  3168 | `	if( pClass == 0 ){` |
|      ! 0 |  3169 | `		return PH7_ABORT;` |
|        - |  3170 | `	}` |
|       47 |  3171 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       47 |  3172 | `	if( pThis == 0 ){` |
|      ! 0 |  3173 | `		return PH7_ABORT;` |
|        - |  3174 | `	}` |
|       47 |  3175 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3176 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  3177 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  3178 | `	{` |
|       47 |  3179 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       47 |  3180 | `		if( pOwner ){` |
|       47 |  3181 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       21 |  3182 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       26 |  3183 | `		}else{` |
|      ! 0 |  3184 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  3185 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  3186 | `		}` |
|        - |  3187 | `	}` |
|       47 |  3188 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       47 |  3189 | `	if( pCons ){` |
|       47 |  3190 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       47 |  3191 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       47 |  3192 | `		apArg[0] = &sArg;` |
|       47 |  3193 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       47 |  3194 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3195 | `	}` |
|       47 |  3196 | `	SyBlobRelease(&sMsg);` |
|       47 |  3197 | `	pFrame = pVm->pFrame;` |
|       47 |  3198 | `	if( pFrame ){` |
|       47 |  3199 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       47 |  3200 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3201 | `	}` |
|       47 |  3202 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       47 |  3203 | `	PH7_ClassInstanceUnref(pThis);` |
|       47 |  3204 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3205 | `		return PH7_ABORT;` |
|        - |  3206 | `	}` |
|       47 |  3207 | `	return PH7_EXCEPTION;` |
|       26 |  3208 |  |
|        - |  3209 |  |
|        - |  3210 | `/*` |
|        - |  3211 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  3212 | ` */` |
|        4 |  3213 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 |  3214 |  |
|        - |  3215 | `	ph7_class *pErrClass;` |
|        - |  3216 | `	ph7_class_instance *pThis;` |
|        - |  3217 | `	ph7_class_method *pCons;` |
|        - |  3218 | `	ph7_value sArg;` |
|        - |  3219 | `	ph7_value *apArg[1];` |
|        - |  3220 | `	SyBlob sMsg;` |
|        - |  3221 | `	SyString sMsgStr;` |
|        - |  3222 | `	VmFrame *pFrame;` |
|        - |  3223 | `	sxi32 rc;` |
|        6 |  3224 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        6 |  3225 | `	if( pErrClass == 0 ){` |
|      ! 0 |  3226 | `		return PH7_ABORT;` |
|        - |  3227 | `	}` |
|        6 |  3228 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        6 |  3229 | `	if( pThis == 0 ){` |
|      ! 0 |  3230 | `		return PH7_ABORT;` |
|        - |  3231 | `	}` |
|        6 |  3232 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3233 | `	{` |
|        6 |  3234 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        6 |  3235 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        6 |  3236 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  3237 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  3238 | `	}` |
|        6 |  3239 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        6 |  3240 | `	if( pCons ){` |
|        6 |  3241 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        6 |  3242 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        6 |  3243 | `		apArg[0] = &sArg;` |
|        6 |  3244 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        6 |  3245 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3246 | `	}` |
|        6 |  3247 | `	SyBlobRelease(&sMsg);` |
|        6 |  3248 | `	pFrame = pVm->pFrame;` |
|        6 |  3249 | `	if( pFrame ){` |
|        6 |  3250 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        6 |  3251 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3252 | `	}` |
|        6 |  3253 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        6 |  3254 | `	PH7_ClassInstanceUnref(pThis);` |
|        6 |  3255 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3256 | `		return PH7_ABORT;` |
|        - |  3257 | `	}` |
|        6 |  3258 | `	return PH7_EXCEPTION;` |
|        4 |  3259 |  |
|        - |  3260 |  |
|        - |  3261 | `/*` |
|        - |  3262 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  3263 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  3264 | ` * For class types, instanceof is verified.` |
|        - |  3265 | ` *` |
|        - |  3266 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  3267 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  3268 | ` */` |
|        - |  3269 | `/*` |
|        - |  3270 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  3271 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  3272 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  3273 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  3274 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  3275 | ` */` |
|       22 |  3276 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        3 |  3277 |  |
|        - |  3278 | `	const char *z, *zEnd, *zTail;` |
|        - |  3279 | `	sxu32 n;` |
|        - |  3280 | `	sxu8 bReal;` |
|        - |  3281 | `	sxi32 rc;` |
|       25 |  3282 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3283 | `		return 0;` |
|        - |  3284 | `	}` |
|       25 |  3285 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       25 |  3286 | `	n = SyBlobLength(&pValue->sBlob);` |
|       25 |  3287 | `	zEnd = z + n;` |
|       25 |  3288 | `	if( n == 0 ){` |
|      ! 0 |  3289 | `		return 0;` |
|        - |  3290 | `	}` |
|       25 |  3291 | `	zTail = 0;` |
|       25 |  3292 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       25 |  3293 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        8 |  3294 | `		return 0;` |
|        - |  3295 | `	}` |
|        - |  3296 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       19 |  3297 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  3298 | `		zTail++;` |
|      ! 0 |  3299 | `	}` |
|       19 |  3300 | `	return zTail == zEnd ? 1 : 0;` |
|       14 |  3301 |  |
|        - |  3302 |  |
|        - |  3303 | `/*` |
|        - |  3304 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  3305 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  3306 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  3307 | ` *   0 if it's not strictly numeric.` |
|        - |  3308 | ` */` |
|       16 |  3309 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  3310 |  |
|        - |  3311 | `	const char *z, *zEnd, *zTail;` |
|        - |  3312 | `	sxu32 n;` |
|       18 |  3313 | `	sxu8 bReal = 0;` |
|        - |  3314 | `	sxi32 rc;` |
|       18 |  3315 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3316 | `		return 0;` |
|        - |  3317 | `	}` |
|       18 |  3318 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  3319 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  3320 | `	zEnd = z + n;` |
|       18 |  3321 | `	if( n == 0 ) return 0;` |
|       18 |  3322 | `	zTail = 0;` |
|       18 |  3323 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  3324 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  3325 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  3326 | `	if( zTail != zEnd ) return 0;` |
|       15 |  3327 | `	return bReal ? 2 : 1;` |
|       10 |  3328 |  |
|        - |  3329 |  |
|        - |  3330 | `/*` |
|        - |  3331 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - |  3332 | `` * PH7 parses `true`/`false`/`iterable`/`mixed` as class-name atoms (they are not`` |
|        - |  3333 | ` * scalar keywords), so without this every enforcement site — return, parameter,` |
|        - |  3334 | ` * property, union alternative — would have to string-match the name itself.` |
|        - |  3335 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - |  3336 | ` * to extend when another literal/pseudo type is added.` |
|        - |  3337 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - |  3338 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - |  3339 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - |  3340 | ` */` |
|      160 |  3341 | `static int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        4 |  3342 |  |
|      164 |  3343 | `	const char *z = pClass->zString;` |
|      164 |  3344 | `	sxu32 n = pClass->nByte;` |
|      164 |  3345 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|       51 |  3346 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - |  3347 | `	}` |
|      114 |  3348 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       15 |  3349 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - |  3350 | `	}` |
|      100 |  3351 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|        3 |  3352 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - |  3353 | `	}` |
|       98 |  3354 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - |  3355 | `		/* iterable === array \| Traversable */` |
|       17 |  3356 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  3357 | `			return 1;` |
|        - |  3358 | `		}` |
|       11 |  3359 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|        5 |  3360 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        5 |  3361 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|        5 |  3362 | `				return 1;` |
|        - |  3363 | `			}` |
|      ! 0 |  3364 | `		}` |
|        7 |  3365 | `		return 0;` |
|        - |  3366 | `	}` |
|       82 |  3367 | `	return -1;` |
|       84 |  3368 |  |
|        - |  3369 | `/*` |
|        - |  3370 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  3371 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  3372 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  3373 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  3374 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  3375 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  3376 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  3377 | ` * throw.` |
|        - |  3378 | ` *` |
|        - |  3379 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  3380 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  3381 | ` */` |
|      102 |  3382 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        5 |  3383 |  |
|        - |  3384 | `	sxu32 i;` |
|        - |  3385 | `	ph7_type_alt *aAlts;` |
|        - |  3386 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  3387 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      107 |  3388 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       16 |  3389 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  3390 | `	}` |
|       95 |  3391 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - |  3392 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - |  3393 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - |  3394 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - |  3395 | ``	 * `iterable\|Foo`). */`` |
|      271 |  3396 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      178 |  3397 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      108 |  3398 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|        3 |  3399 | `			return SXRET_OK;` |
|        - |  3400 | `		}` |
|       93 |  3401 | `	}` |
|       93 |  3402 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       93 |  3403 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      269 |  3404 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      181 |  3405 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      155 |  3406 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      155 |  3407 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      155 |  3408 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       79 |  3409 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       51 |  3410 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  3411 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       93 |  3412 | `	}` |
|        - |  3413 | `	/* Object handling */` |
|       93 |  3414 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       19 |  3415 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       19 |  3416 | `		if( bHasClassAlt ){` |
|       14 |  3417 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  3418 | `			ph7_class *pSelfNow = 0;` |
|       14 |  3419 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  3420 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  3421 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  3422 | `			}` |
|       26 |  3423 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  3424 | `				ph7_class *pExpected;` |
|        - |  3425 | `				SyString *pCN;` |
|       22 |  3426 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  3427 | `				pCN = &aAlts[i].sClass;` |
|       22 |  3428 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  3429 | `					pExpected = pSelfNow;` |
|       22 |  3430 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  3431 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3432 | `				}else{` |
|       22 |  3433 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  3434 | `				}` |
|       22 |  3435 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  3436 | `					return SXRET_OK;` |
|        - |  3437 | `				}` |
|        8 |  3438 | `			}` |
|        2 |  3439 | `		}` |
|       10 |  3440 | `		return SXERR_INVALID;` |
|        - |  3441 | `	}` |
|        - |  3442 | `	/* Array handling */` |
|       76 |  3443 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        9 |  3444 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  3445 | `	}` |
|        - |  3446 | `	/* Scalar handling — exact match first */` |
|       68 |  3447 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       28 |  3448 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  3449 | `	}` |
|       42 |  3450 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  3451 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  3452 | `	}` |
|       38 |  3453 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  3454 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  3455 | `	}` |
|       18 |  3456 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3457 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  3458 | `	}` |
|       18 |  3459 | `	if( bStrict ){` |
|        - |  3460 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  3461 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  3462 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  3463 | `			return SXRET_OK;` |
|        - |  3464 | `		}` |
|      ! 0 |  3465 | `		return SXERR_INVALID;` |
|        - |  3466 | `	}` |
|        - |  3467 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  3468 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  3469 | `	 * to match PHP's union RFC. */` |
|        - |  3470 | `	{` |
|       18 |  3471 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  3472 | `		if( bHasInt ){` |
|        - |  3473 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  3474 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  3475 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  3476 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3477 | `				return SXRET_OK;` |
|        - |  3478 | `			}` |
|       18 |  3479 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3480 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  3481 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  3482 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  3483 | `					return SXRET_OK;` |
|        - |  3484 | `				}` |
|      ! 0 |  3485 | `			}` |
|       18 |  3486 | `			if( kind == 1 ){` |
|        9 |  3487 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  3488 | `				return SXRET_OK;` |
|        - |  3489 | `			}` |
|        4 |  3490 | `		}` |
|       10 |  3491 | `		if( bHasFloat ){` |
|       10 |  3492 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  3493 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  3494 | `				return SXRET_OK;` |
|        - |  3495 | `			}` |
|       10 |  3496 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  3497 | `				PH7_MemObjToReal(pValue);` |
|        7 |  3498 | `				return SXRET_OK;` |
|        - |  3499 | `			}` |
|        1 |  3500 | `		}` |
|        3 |  3501 | `		if( bHasString ){` |
|      ! 0 |  3502 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3503 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3504 | `				return SXRET_OK;` |
|        - |  3505 | `			}` |
|      ! 0 |  3506 | `		}` |
|        3 |  3507 | `		if( bHasBool ){` |
|      ! 0 |  3508 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3509 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3510 | `				return SXRET_OK;` |
|        - |  3511 | `			}` |
|      ! 0 |  3512 | `		}` |
|        - |  3513 | `	}` |
|        3 |  3514 | `	return SXERR_INVALID;` |
|       56 |  3515 |  |
|        - |  3516 |  |
|        - |  3517 | `/*` |
|        - |  3518 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3519 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3520 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3521 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3522 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3523 | ` */` |
|       38 |  3524 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        3 |  3525 |  |
|        - |  3526 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - |  3527 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - |  3528 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - |  3529 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - |  3530 | `	 * silently swallow any argument. */` |
|       41 |  3531 | `	if( nType == MEMOBJ_NULL ){` |
|        3 |  3532 | `		return SXERR_INVALID;` |
|        - |  3533 | `	}` |
|       39 |  3534 | `	if( bStrict ){` |
|        - |  3535 | `		/* Only int -> float widening is allowed implicitly. */` |
|       13 |  3536 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3537 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3538 | `			return SXRET_OK;` |
|        - |  3539 | `		}` |
|       11 |  3540 | `		return SXERR_INVALID;` |
|        - |  3541 | `	}` |
|        - |  3542 | `	{` |
|       28 |  3543 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3544 | `		if( xCast ) xCast(pVal);` |
|        - |  3545 | `	}` |
|       28 |  3546 | `	return SXRET_OK;` |
|       22 |  3547 |  |
|        - |  3548 |  |
|        - |  3549 | `/*` |
|        - |  3550 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3551 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3552 | ` *` |
|        - |  3553 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3554 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3555 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3556 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3557 | ` */` |
|       12 |  3558 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        4 |  3559 |  |
|       16 |  3560 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|       16 |  3561 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|       16 |  3562 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       16 |  3563 | `		if( pDeclared->zString && nCopy > 0 ){` |
|       16 |  3564 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        6 |  3565 | `		}` |
|       16 |  3566 | `		zBuf[nCopy] = 0;` |
|       16 |  3567 | `		return zBuf;` |
|        - |  3568 | `	}` |
|      ! 0 |  3569 | `	switch( nType ){` |
|      ! 0 |  3570 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3571 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3572 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3573 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3574 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3575 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3576 | `		default:             return "scalar";` |
|        - |  3577 | `	}` |
|       10 |  3578 |  |
|        - |  3579 |  |
|        - |  3580 | `/*` |
|        - |  3581 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3582 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3583 | ` */` |
|       18 |  3584 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        3 |  3585 |  |
|       21 |  3586 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       30 |  3587 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3588 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       21 |  3589 | `	return zBuf;` |
|        3 |  3590 |  |
|        - |  3591 |  |
|     6782 |  3592 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 |  3593 |  |
|        - |  3594 | `	SyHashEntry *pSlot;` |
|        - |  3595 | `	VmClassAttr *pVmAttr;` |
|        - |  3596 | `	ph7_class_attr *pAttr;` |
|     6787 |  3597 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|     6787 |  3598 | `	if( pSlot == 0 ){` |
|     6561 |  3599 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3600 | `	}` |
|      231 |  3601 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      231 |  3602 | `	pAttr = pVmAttr->pAttr;` |
|      231 |  3603 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3604 | `		return SXRET_OK;` |
|        - |  3605 | `	}` |
|        - |  3606 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3607 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3608 | `	 * matching PHP's documented behavior. */` |
|      231 |  3609 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       25 |  3610 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3611 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3612 |  |
|       18 |  3613 | `		if( rc == SXRET_OK ){` |
|        9 |  3614 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3615 | `			return SXRET_OK;` |
|        - |  3616 | `		}` |
|        9 |  3617 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3618 | `			char zBuf[128];` |
|        4 |  3619 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3620 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3621 | `		}` |
|        6 |  3622 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3623 | `	}` |
|        - |  3624 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - |  3625 | `	 * includes null). */` |
|      217 |  3626 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       15 |  3627 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       12 |  3628 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 |  3629 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       14 |  3630 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       14 |  3631 | `			return SXRET_OK;` |
|        - |  3632 | `		}` |
|        3 |  3633 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3634 | `	}` |
|        - |  3635 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - |  3636 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - |  3637 | `	 * type error. */` |
|      203 |  3638 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 |  3639 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3640 | `	}` |
|        - |  3641 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3642 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3643 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      203 |  3644 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3645 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3646 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3647 | `			return SXRET_OK;` |
|        - |  3648 | `		}` |
|        7 |  3649 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3650 | `	}` |
|        - |  3651 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - |  3652 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - |  3653 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - |  3654 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - |  3655 | `	 * wrongly accept any object / reject arrays. */` |
|      193 |  3656 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       39 |  3657 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       39 |  3658 | `		if( rcPseudo == 1 ){` |
|       11 |  3659 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       11 |  3660 | `			return SXRET_OK;` |
|        - |  3661 | `		}` |
|       29 |  3662 | `		if( rcPseudo == 0 ){` |
|        3 |  3663 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3664 | `		}` |
|        - |  3665 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       12 |  3666 | `	}` |
|      181 |  3667 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3668 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3669 | `		 * currently active on the self-stack. */` |
|       27 |  3670 | `		ph7_class *pExpected = 0;` |
|       27 |  3671 | `		SyString *pClassName = &pAttr->sClass;` |
|       27 |  3672 | `		ph7_class *pSelfNow = 0;` |
|       27 |  3673 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3674 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3675 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3676 | `		}` |
|       27 |  3677 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3678 | `			pExpected = pSelfNow;` |
|       25 |  3679 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3680 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3681 | `		}else{` |
|       23 |  3682 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3683 | `		}` |
|       27 |  3684 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3685 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3686 | `		}` |
|       27 |  3687 | `		if( pExpected ){` |
|       23 |  3688 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       23 |  3689 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3690 | `				char zBuf[128];` |
|        8 |  3691 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3692 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3693 | `			}` |
|        8 |  3694 | `		}` |
|       23 |  3695 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       23 |  3696 | `		return SXRET_OK;` |
|        - |  3697 | `	}` |
|        - |  3698 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3699 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      157 |  3700 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3701 | `		char zBuf[128];` |
|       11 |  3702 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3703 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3704 | `	}` |
|      151 |  3705 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       31 |  3706 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       31 |  3707 | `		if( xCast ){` |
|        - |  3708 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       31 |  3709 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3710 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3711 | `			}` |
|       29 |  3712 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        6 |  3713 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3714 | `			}` |
|        - |  3715 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3716 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3717 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       29 |  3718 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       19 |  3719 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       22 |  3720 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|       13 |  3721 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3722 | `			}` |
|       12 |  3723 | `			xCast(pValue);` |
|        5 |  3724 | `		}` |
|        5 |  3725 | `	}` |
|      134 |  3726 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      134 |  3727 | `	return SXRET_OK;` |
|     3396 |  3728 |  |
|        - |  3729 |  |
|        - |  3730 | `/*` |
|        - |  3731 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3732 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3733 | ` * information.` |
|        - |  3734 | ` * ------------------------------------` |
|        - |  3735 | ` * Simple boring wrapper function.` |
|        - |  3736 | ` * ------------------------------------` |
|        - |  3737 | ` */` |
|       20 |  3738 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        3 |  3739 |  |
|        - |  3740 | `	va_list ap;` |
|        - |  3741 | `	sxi32 rc;` |
|       23 |  3742 | `	va_start(ap,zFormat);` |
|       23 |  3743 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       23 |  3744 | `	va_end(ap);` |
|       23 |  3745 | `	return rc;` |
|        3 |  3746 |  |
|        - |  3747 | `/*` |
|        - |  3748 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3749 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3750 | ` */` |
|       42 |  3751 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 |  3752 |  |
|        - |  3753 | `	ph7_class *pClass;` |
|        - |  3754 | `	ph7_class_instance *pThis;` |
|        - |  3755 | `	ph7_class_method *pCons;` |
|        - |  3756 | `	ph7_value sArg;` |
|        - |  3757 | `	ph7_value *apArg[1];` |
|        - |  3758 | `	SyBlob sMsg;` |
|        - |  3759 | `	SyString sMsgStr;` |
|        - |  3760 | `	VmFrame *pFrame;` |
|        - |  3761 | `	sxi32 rc;` |
|       47 |  3762 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       47 |  3763 | `	if( pClass == 0 ){` |
|      ! 0 |  3764 | `		return PH7_ABORT;` |
|        - |  3765 | `	}` |
|       47 |  3766 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       47 |  3767 | `	if( pThis == 0 ){` |
|      ! 0 |  3768 | `		return PH7_ABORT;` |
|        - |  3769 | `	}` |
|       47 |  3770 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       47 |  3771 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       21 |  3772 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       47 |  3773 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       47 |  3774 | `	if( pCons ){` |
|       47 |  3775 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       47 |  3776 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       47 |  3777 | `		apArg[0] = &sArg;` |
|       47 |  3778 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       47 |  3779 | `		PH7_MemObjRelease(&sArg);` |
|       21 |  3780 | `	}` |
|       47 |  3781 | `	SyBlobRelease(&sMsg);` |
|       47 |  3782 | `	pFrame = pVm->pFrame;` |
|       47 |  3783 | `	if( pFrame ){` |
|       47 |  3784 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       47 |  3785 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       21 |  3786 | `	}` |
|       47 |  3787 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       47 |  3788 | `	PH7_ClassInstanceUnref(pThis);` |
|       47 |  3789 | `	if( rc == SXERR_ABORT ){` |
|        6 |  3790 | `		return PH7_ABORT;` |
|        - |  3791 | `	}` |
|       43 |  3792 | `	return PH7_EXCEPTION;` |
|       26 |  3793 |  |
|        - |  3794 | `/*` |
|        - |  3795 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3796 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3797 | ` */` |
|       12 |  3798 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        4 |  3799 |  |
|        - |  3800 | `	ph7_class *pClass;` |
|        - |  3801 | `	ph7_class_instance *pThis;` |
|        - |  3802 | `	ph7_class_method *pCons;` |
|        - |  3803 | `	ph7_value sArg;` |
|        - |  3804 | `	ph7_value *apArg[1];` |
|        - |  3805 | `	SyBlob sMsg;` |
|        - |  3806 | `	SyString sMsgStr;` |
|        - |  3807 | `	VmFrame *pFrame;` |
|        - |  3808 | `	sxi32 rc;` |
|       16 |  3809 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       16 |  3810 | `	if( pClass == 0 ){` |
|      ! 0 |  3811 | `		return PH7_ABORT;` |
|        - |  3812 | `	}` |
|       16 |  3813 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       16 |  3814 | `	if( pThis == 0 ){` |
|      ! 0 |  3815 | `		return PH7_ABORT;` |
|        - |  3816 | `	}` |
|       16 |  3817 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       16 |  3818 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        6 |  3819 | `		pFuncName,zExpected,zGiven);` |
|       16 |  3820 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       16 |  3821 | `	if( pCons ){` |
|       16 |  3822 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       16 |  3823 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       16 |  3824 | `		apArg[0] = &sArg;` |
|       16 |  3825 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       16 |  3826 | `		PH7_MemObjRelease(&sArg);` |
|        6 |  3827 | `	}` |
|       16 |  3828 | `	SyBlobRelease(&sMsg);` |
|       16 |  3829 | `	pFrame = pVm->pFrame;` |
|       16 |  3830 | `	if( pFrame ){` |
|       16 |  3831 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       16 |  3832 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 |  3833 | `	}` |
|       16 |  3834 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       16 |  3835 | `	PH7_ClassInstanceUnref(pThis);` |
|       16 |  3836 | `	if( rc == SXERR_ABORT ){` |
|        9 |  3837 | `		return PH7_ABORT;` |
|        - |  3838 | `	}` |
|        7 |  3839 | `	return PH7_EXCEPTION;` |
|       10 |  3840 |  |
|        - |  3841 | `/*` |
|        - |  3842 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3843 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3844 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3845 | ` */` |
|       28 |  3846 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        3 |  3847 |  |
|       31 |  3848 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       10 |  3849 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3850 | `	}` |
|       23 |  3851 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        9 |  3852 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        9 |  3853 | `		if( pThis && pThis->pClass ){` |
|        9 |  3854 | `			SyString *pName = &pThis->pClass->sName;` |
|        9 |  3855 | `			sxu32 n = pName->nByte;` |
|        9 |  3856 | `			if( n >= nBuf ){` |
|      ! 0 |  3857 | `				n = nBuf - 1;` |
|      ! 0 |  3858 | `			}` |
|        9 |  3859 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        9 |  3860 | `			zBuf[n] = 0;` |
|        9 |  3861 | `			return zBuf;` |
|        - |  3862 | `		}` |
|      ! 0 |  3863 | `		return "object";` |
|        - |  3864 | `	}` |
|       16 |  3865 | `	return ph7_type_name(pVal);` |
|       17 |  3866 |  |
|        - |  3867 | `/*` |
|        - |  3868 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3869 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3870 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3871 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3872 | ` */` |
|       18 |  3873 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        3 |  3874 |  |
|        - |  3875 | `	ph7_class *pClass;` |
|        - |  3876 | `	ph7_class_instance *pThis;` |
|        - |  3877 | `	ph7_class_method *pCons;` |
|        - |  3878 | `	ph7_value sArg;` |
|        - |  3879 | `	ph7_value *apArg[1];` |
|        - |  3880 | `	SyBlob sMsg;` |
|        - |  3881 | `	SyString sMsgStr;` |
|        - |  3882 | `	VmFrame *pFrame;` |
|        - |  3883 | `	sxi32 rc;` |
|       21 |  3884 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3885 | `	char zNameBuf[64];` |
|       21 |  3886 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       21 |  3887 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       21 |  3888 | `	if( pClass == 0 ){` |
|      ! 0 |  3889 | `		return PH7_ABORT;` |
|        - |  3890 | `	}` |
|       21 |  3891 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       21 |  3892 | `	if( pThis == 0 ){` |
|      ! 0 |  3893 | `		return PH7_ABORT;` |
|        - |  3894 | `	}` |
|       21 |  3895 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       21 |  3896 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       21 |  3897 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       21 |  3898 | `	if( pCons ){` |
|       21 |  3899 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       21 |  3900 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       21 |  3901 | `		apArg[0] = &sArg;` |
|       21 |  3902 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       21 |  3903 | `		PH7_MemObjRelease(&sArg);` |
|        9 |  3904 | `	}` |
|       21 |  3905 | `	SyBlobRelease(&sMsg);` |
|       21 |  3906 | `	pFrame = pVm->pFrame;` |
|       21 |  3907 | `	if( pFrame ){` |
|       21 |  3908 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       21 |  3909 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        9 |  3910 | `	}` |
|       21 |  3911 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       21 |  3912 | `	PH7_ClassInstanceUnref(pThis);` |
|       21 |  3913 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3914 | `		return PH7_ABORT;` |
|        - |  3915 | `	}` |
|       21 |  3916 | `	return PH7_EXCEPTION;` |
|       12 |  3917 |  |
|        - |  3918 | `/*` |
|        - |  3919 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3920 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3921 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3922 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3923 | ` */` |
|        - |  3924 | `/*` |
|        - |  3925 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3926 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3927 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3928 | ` */` |
|       34 |  3929 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        5 |  3930 |  |
|        - |  3931 | `	sxu32 nCopy;` |
|       39 |  3932 | `	if( nBuf == 0 ) return "";` |
|       39 |  3933 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3934 | `		zBuf[0] = 0;` |
|      ! 0 |  3935 | `		return zBuf;` |
|        - |  3936 | `	}` |
|       39 |  3937 | `	nCopy = SyStringLength(pStr);` |
|       39 |  3938 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       39 |  3939 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       39 |  3940 | `	zBuf[nCopy] = 0;` |
|       39 |  3941 | `	return zBuf;` |
|       22 |  3942 |  |
|        - |  3943 |  |
|      474 |  3944 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 |  3945 |  |
|      479 |  3946 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3947 | `	const char *zGiven;` |
|        - |  3948 | `	char zBuf[128];` |
|        - |  3949 | `	char zTypeBuf[128];` |
|        - |  3950 | `	/* Untyped function: no enforcement. */` |
|      479 |  3951 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3952 | `		return SXRET_OK;` |
|        - |  3953 | `	}` |
|        - |  3954 | `	/* void return type: the function must not produce a value. */` |
|      479 |  3955 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      156 |  3956 | `		if( pValue == 0 ){` |
|      154 |  3957 | `			return SXRET_OK;` |
|        - |  3958 | `		}` |
|        - |  3959 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3960 | `		 * still counts as "returned a value" here. */` |
|        3 |  3961 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3962 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3963 | `	}` |
|        - |  3964 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3965 | ``	 * returns null. For a typed non-nullable return (including `mixed`, which`` |
|        - |  3966 | `	 * requires an explicit returned value), that's a TypeError. */` |
|      327 |  3967 | `	if( pValue == 0 ){` |
|      ! 0 |  3968 | `		const char *zExpected = "value";` |
|      ! 0 |  3969 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3970 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3971 | `		}` |
|      ! 0 |  3972 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3973 | `	}` |
|        - |  3974 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - |  3975 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - |  3976 | `	 * matching how every other typed return reports a missing value.) */` |
|      327 |  3977 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 |  3978 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 |  3979 | `			return SXRET_OK;` |
|        - |  3980 | `		}` |
|        4 |  3981 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"null",` |
|        1 |  3982 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3983 | `	}` |
|        - |  3984 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - |  3985 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - |  3986 | `	 * Check by value before the real-class instanceof branch below. */` |
|      323 |  3987 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|       64 |  3988 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|       64 |  3989 | `		if( rcPseudo == 1 ){` |
|       53 |  3990 | `			return SXRET_OK;` |
|        - |  3991 | `		}` |
|       12 |  3992 | `		if( rcPseudo == 0 ){` |
|        9 |  3993 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        4 |  3994 | `				VmSyStringToCStr(&pFunc->sReturnClass,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  3995 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3996 | `		}` |
|        - |  3997 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|        3 |  3998 | `	}` |
|        - |  3999 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  4000 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  4001 | `	 * bNullable=0 here. */` |
|      267 |  4002 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  4003 | `		sxi32 rcU;` |
|      ! 0 |  4004 | `		int bNullable = 0;` |
|      ! 0 |  4005 | `		const char *zExpected = "union";` |
|        - |  4006 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  4007 | `		{` |
|        - |  4008 | `			sxu32 i;` |
|      ! 0 |  4009 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  4010 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  4011 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  4012 | `			}` |
|        - |  4013 | `		}` |
|      ! 0 |  4014 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  4015 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  4016 | `			return SXRET_OK;` |
|        - |  4017 | `		}` |
|      ! 0 |  4018 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4019 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4020 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  4021 | `			zGiven = "null";` |
|      ! 0 |  4022 | `		}else{` |
|      ! 0 |  4023 | `			zGiven = ph7_type_name(pValue);` |
|        - |  4024 | `		}` |
|      ! 0 |  4025 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  4026 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  4027 | `		}` |
|      ! 0 |  4028 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4029 | `	}` |
|        - |  4030 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  4031 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  4032 | `	 * it into the TypeError message. */` |
|      267 |  4033 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        8 |  4034 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  4035 | `		const char *zExpected;` |
|        - |  4036 | `		ph7_class *pExpected;` |
|        8 |  4037 | `		ph7_class *pSelfNow = 0;` |
|        8 |  4038 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        8 |  4039 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        8 |  4040 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        3 |  4041 | `		}` |
|        8 |  4042 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  4043 | `			pExpected = pSelfNow;` |
|        6 |  4044 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  4045 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  4046 | `		}else{` |
|        5 |  4047 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  4048 | `		}` |
|        8 |  4049 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        8 |  4050 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  4051 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  4052 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4053 | `		}` |
|        8 |  4054 | `		if( pExpected ){` |
|        6 |  4055 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  4056 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  4057 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4058 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  4059 | `			}` |
|        2 |  4060 | `		}` |
|        8 |  4061 | `		return SXRET_OK;` |
|        - |  4062 | `	}` |
|        - |  4063 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  4064 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  4065 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  4066 | `	 * via the type-text leading '?'. */` |
|      261 |  4067 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  4068 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  4069 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  4070 | `			return SXRET_OK;` |
|        - |  4071 | `		}` |
|      ! 0 |  4072 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4073 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4074 | `			"null");` |
|        - |  4075 | `	}` |
|        - |  4076 | `	/* Exact match? Done. */` |
|      255 |  4077 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      249 |  4078 | `		return SXRET_OK;` |
|        - |  4079 | `	}` |
|        - |  4080 | `	/* Object->scalar is never compatible. */` |
|        9 |  4081 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4082 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  4083 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4084 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4085 | `			zGiven);` |
|        - |  4086 | `	}` |
|        - |  4087 | `	/* Array <-> scalar is never compatible. */` |
|        9 |  4088 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  4089 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  4090 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  4091 | `			ph7_type_name(pValue));` |
|        - |  4092 | `	}` |
|        - |  4093 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  4094 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  4095 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  4096 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  4097 | `	if( !bStrict` |
|        5 |  4098 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  4099 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        7 |  4100 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  4101 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4102 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  4103 | `			"string");` |
|        - |  4104 | `	}` |
|        6 |  4105 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  4106 | `		return SXRET_OK;` |
|        - |  4107 | `	}` |
|        4 |  4108 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  4109 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  4110 | `		ph7_type_name(pValue));` |
|      242 |  4111 |  |
|        - |  4112 | `/*` |
|        - |  4113 | ` * Report a fatal named-argument error.` |
|        - |  4114 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  4115 | ` */` |
|        6 |  4116 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        2 |  4117 |  |
|        8 |  4118 | `	const char *zFunc = 0;` |
|        8 |  4119 | `	int nFunc = 0;` |
|        8 |  4120 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        8 |  4121 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        2 |  4122 |  |
|        - |  4123 | `/*` |
|        - |  4124 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  4125 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  4126 | ` * information.` |
|        - |  4127 | ` * ------------------------------------` |
|        - |  4128 | ` * Simple boring wrapper function.` |
|        - |  4129 | ` * ------------------------------------` |
|        - |  4130 | ` */` |
|       24 |  4131 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        4 |  4132 |  |
|        - |  4133 | `	sxi32 rc;` |
|       28 |  4134 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       28 |  4135 | `	return rc;` |
|        4 |  4136 |  |
|        - |  4137 | `/*` |
|        - |  4138 | ` * Resolve function context from the current frame.` |
|        - |  4139 | ` */` |
|     1018 |  4140 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 |  4141 |  |
|        - |  4142 | `	VmFrame *pFrame;` |
|        - |  4143 | `	ph7_vm_func *pFunc;` |
|     1022 |  4144 | `	*pzFuncName = 0;` |
|     1022 |  4145 | `	*pnFuncLen = 0;` |
|     1022 |  4146 | `	pFrame = pVm->pFrame;` |
|     1022 |  4147 | `	if( pFrame == 0 ){` |
|      ! 0 |  4148 | `		return;` |
|        - |  4149 | `	}` |
|     1022 |  4150 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1022 |  4151 | `	if( pFrame->pParent == 0 ){` |
|      998 |  4152 | `		return;` |
|        - |  4153 | `	}` |
|       28 |  4154 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       28 |  4155 | `	if( pFunc == 0 ){` |
|      ! 0 |  4156 | `		return;` |
|        - |  4157 | `	}` |
|       28 |  4158 | `	*pzFuncName = pFunc->sName.zString;` |
|       28 |  4159 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      513 |  4160 |  |
|        - |  4161 | `/*` |
|        - |  4162 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  4163 | ` */` |
|      524 |  4164 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        4 |  4165 |  |
|        - |  4166 | `	SyBlob sOut;` |
|        - |  4167 | `	SyString *pFile;` |
|      528 |  4168 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  4169 | `		return PH7_OK;` |
|        - |  4170 | `	}` |
|      528 |  4171 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  4172 | `		zClass = "Exception";` |
|      ! 0 |  4173 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  4174 | `	}` |
|      528 |  4175 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      506 |  4176 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  4177 | `	}` |
|      528 |  4178 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      528 |  4179 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      528 |  4180 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      528 |  4181 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      528 |  4182 | `	if( zMsg && nMsg > 0 ){` |
|      528 |  4183 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      528 |  4184 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  4185 | `	}` |
|      528 |  4186 | `	if( pFile ){` |
|      528 |  4187 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      528 |  4188 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      528 |  4189 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  4190 | `	}` |
|      528 |  4191 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      528 |  4192 | `	if( pFile ){` |
|      528 |  4193 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      528 |  4194 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      528 |  4195 | `		if( zFuncName && nFuncLen > 0 ){` |
|       28 |  4196 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       16 |  4197 | `		}else{` |
|      504 |  4198 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        4 |  4199 | `		}` |
|      262 |  4200 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  4201 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  4202 | `	}else{` |
|      ! 0 |  4203 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  4204 | `	}` |
|      528 |  4205 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      528 |  4206 | `	if( pFile ){` |
|      528 |  4207 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      528 |  4208 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      528 |  4209 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      528 |  4210 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  4211 | `	}` |
|      528 |  4212 | `	VmCallErrorHandler(pVm,&sOut);` |
|      528 |  4213 | `	SyBlobRelease(&sOut);` |
|      528 |  4214 | `	return PH7_ABORT;` |
|      266 |  4215 |  |
|        - |  4216 | `/*` |
|        - |  4217 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  4218 | ` *` |
|        - |  4219 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  4220 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  4221 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  4222 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  4223 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  4224 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  4225 | ` */` |
|      938 |  4226 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 |  4227 |  |
|      943 |  4228 | `	if( pVm->bCoalesceArmed ){` |
|        8 |  4229 | `		if( pVm->pCoalesceObj ){` |
|        8 |  4230 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  4231 | `		}` |
|        8 |  4232 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        8 |  4233 | `		pVm->pCoalesceObj = 0;` |
|        8 |  4234 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  4235 | `	}` |
|      943 |  4236 |  |
|        - |  4237 | `/*` |
|        - |  4238 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  4239 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  4240 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  4241 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  4242 | ` *` |
|        - |  4243 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  4244 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  4245 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  4246 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  4247 | ` */` |
|        4 |  4248 | `static sxi32 VmThrowFromVm(` |
|        - |  4249 | `	ph7_vm *pVm,` |
|        - |  4250 | `	const char *zClass,` |
|        - |  4251 | `	const char *zMsg,` |
|        - |  4252 | `	sxu32 nMsg` |
|        2 |  4253 | `){` |
|        - |  4254 | `	ph7_class *pClass;` |
|        - |  4255 | `	ph7_class_instance *pThis;` |
|        - |  4256 | `	ph7_class_method *pCons;` |
|        - |  4257 | `	VmFrame *pFrame;` |
|        - |  4258 | `	sxi32 rc;` |
|        6 |  4259 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        6 |  4260 | `	if( pClass == 0 ){` |
|      ! 0 |  4261 | `		return SXERR_ABORT;` |
|        - |  4262 | `	}` |
|        6 |  4263 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        6 |  4264 | `	if( pThis == 0 ){` |
|      ! 0 |  4265 | `		return SXERR_ABORT;` |
|        - |  4266 | `	}` |
|        6 |  4267 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        6 |  4268 | `	if( pCons ){` |
|        - |  4269 | `		ph7_value sArg;` |
|        - |  4270 | `		ph7_value *apArg[1];` |
|        - |  4271 | `		SyString sMsgStr;` |
|        6 |  4272 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        6 |  4273 | `		PH7_MemObjInit(pVm,&sArg);` |
|        6 |  4274 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        6 |  4275 | `		apArg[0] = &sArg;` |
|        6 |  4276 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        6 |  4277 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  4278 | `	}` |
|        6 |  4279 | `	pFrame = pVm->pFrame;` |
|        6 |  4280 | `	if( pFrame ){` |
|        6 |  4281 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        6 |  4282 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  4283 | `	}` |
|        6 |  4284 | `	rc = VmThrowException(pVm,pThis);` |
|        6 |  4285 | `	PH7_ClassInstanceUnref(pThis);` |
|        6 |  4286 | `	return rc;` |
|        4 |  4287 |  |
|        - |  4288 | `/*` |
|        - |  4289 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  4290 | ` */` |
|      574 |  4291 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 |  4292 |  |
|        - |  4293 | `	ph7_vm *pVm;` |
|        - |  4294 | `	ph7_class *pClass;` |
|        - |  4295 | `	ph7_class_instance *pThis;` |
|        - |  4296 | `	ph7_class_method *pCons;` |
|        - |  4297 | `	ph7_value sArg;` |
|        - |  4298 | `	ph7_value *apArg[1];` |
|        - |  4299 | `	SyBlob sMsg;` |
|        - |  4300 | `	SyString sMsgStr;` |
|        - |  4301 | `	VmFrame *pFrame;` |
|        - |  4302 | `	va_list ap;` |
|        - |  4303 | `	sxi32 rc;` |
|        - |  4304 |  |
|      579 |  4305 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4306 | `		return PH7_ABORT;` |
|        - |  4307 | `	}` |
|      579 |  4308 | `	pVm = pCtx->pVm;` |
|      579 |  4309 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4310 | `		zClass = "Error";` |
|      ! 0 |  4311 | `	}` |
|      579 |  4312 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      579 |  4313 | `	if( pClass == 0 ){` |
|      ! 0 |  4314 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4315 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  4316 | `			zClass` |
|        - |  4317 | `			);` |
|        - |  4318 | `	}` |
|      579 |  4319 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      579 |  4320 | `	if( pThis == 0 ){` |
|      ! 0 |  4321 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  4322 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  4323 | `			);` |
|        - |  4324 | `	}` |
|        - |  4325 |  |
|      579 |  4326 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      579 |  4327 | `	va_start(ap,zFormat);` |
|      579 |  4328 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      579 |  4329 | `	va_end(ap);` |
|        - |  4330 |  |
|      579 |  4331 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      579 |  4332 | `	if( pCons ){` |
|      579 |  4333 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      579 |  4334 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      579 |  4335 | `		apArg[0] = &sArg;` |
|      579 |  4336 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      579 |  4337 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  4338 | `	}` |
|      579 |  4339 | `	SyBlobRelease(&sMsg);` |
|        - |  4340 |  |
|      579 |  4341 | `	pFrame = pVm->pFrame;` |
|      579 |  4342 | `	if( pFrame ){` |
|      579 |  4343 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      579 |  4344 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  4345 | `	}` |
|      579 |  4346 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      579 |  4347 | `	PH7_ClassInstanceUnref(pThis);` |
|      579 |  4348 | `	if( rc == SXERR_ABORT ){` |
|      494 |  4349 | `		return PH7_ABORT;` |
|        - |  4350 | `	}` |
|       88 |  4351 | `	return PH7_EXCEPTION;` |
|      292 |  4352 |  |
|        - |  4353 | `/*` |
|        - |  4354 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  4355 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  4356 | ` */` |
|      ! 0 |  4357 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  4358 |  |
|        - |  4359 | `	ph7_vm *pVm;` |
|        - |  4360 | `	SyBlob sMsg;` |
|      ! 0 |  4361 | `	const char *zFuncName = 0;` |
|      ! 0 |  4362 | `	int nFuncLen = 0;` |
|        - |  4363 | `	va_list ap;` |
|        - |  4364 | `	sxi32 rc;` |
|        - |  4365 |  |
|      ! 0 |  4366 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  4367 | `		return PH7_OK;` |
|        - |  4368 | `	}` |
|      ! 0 |  4369 | `	pVm = pCtx->pVm;` |
|      ! 0 |  4370 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  4371 | `		zClass = "Error";` |
|      ! 0 |  4372 | `	}` |
|        - |  4373 |  |
|      ! 0 |  4374 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  4375 |  |
|      ! 0 |  4376 | `	va_start(ap,zFormat);` |
|      ! 0 |  4377 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  4378 | `	va_end(ap);` |
|        - |  4379 |  |
|      ! 0 |  4380 | `	if( pCtx->pFunc ){` |
|      ! 0 |  4381 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  4382 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  4383 | `	}` |
|      ! 0 |  4384 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  4385 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  4386 | `	}` |
|      ! 0 |  4387 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  4388 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  4389 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  4390 | `	return rc;` |
|      ! 0 |  4391 |  |
|        - |  4392 | `/*` |
|        - |  4393 | ` * Save the execution state of a fiber/generator context.` |
|        - |  4394 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  4395 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  4396 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  4397 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  4398 | ` * when VmByteCodeExec returns.` |
|        - |  4399 | ` */` |
|      200 |  4400 | `static sxi32 VmSuspendCtx(` |
|        - |  4401 | `	ph7_vm *pVm,` |
|        - |  4402 | `	ph7_exec_ctx *pCtx,` |
|        - |  4403 | `	sxi32 pc,` |
|        - |  4404 | `	sxi32 nTos` |
|        - |  4405 | `	)` |
|        5 |  4406 |  |
|      100 |  4407 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      205 |  4408 | `	pCtx->pc = pc;` |
|      205 |  4409 | `	pCtx->nTos = nTos;` |
|      205 |  4410 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      205 |  4411 | `	return PH7_SUSPEND;` |
|        5 |  4412 |  |
|        - |  4413 | `/*` |
|        - |  4414 | ` * Resolve named-argument mapping.` |
|        - |  4415 | ` *` |
|        - |  4416 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  4417 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  4418 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  4419 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  4420 | ` * every formal parameter that received a value.` |
|        - |  4421 | ` *` |
|        - |  4422 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  4423 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  4424 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  4425 | ` */` |
|       98 |  4426 | `static sxi32 VmResolveNamedArgs(` |
|        - |  4427 | `	ph7_vm *pVm,` |
|        - |  4428 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  4429 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  4430 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  4431 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  4432 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  4433 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  4434 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  4435 |  |
|        3 |  4436 |  |
|      101 |  4437 | `	sxi32 posIdx = 0;` |
|        - |  4438 | `	sxu32 i;` |
|        - |  4439 | `	char zErrMsg[256];` |
|      101 |  4440 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      297 |  4441 | `	for( i = 0; i < nActual; i++ ){` |
|      199 |  4442 | `		aSlot[i] = -2;` |
|      101 |  4443 | `	}` |
|      291 |  4444 | `	for( i = 0; i < nActual; i++ ){` |
|      287 |  4445 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  4446 | `			/* Named argument — find formal by name */` |
|      185 |  4447 | `			int found = 0;` |
|        - |  4448 | `			sxu32 k;` |
|      305 |  4449 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  4450 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      282 |  4451 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  4452 | `						pMap->aNames[i].zString,` |
|      402 |  4453 | `						pMap->aNames[i].nByte) == 0 ){` |
|      173 |  4454 | `					if( aUsed[k] ){` |
|        8 |  4455 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4456 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  4457 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        6 |  4458 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        6 |  4459 | `						return PH7_ABORT;` |
|        - |  4460 | `					}` |
|      168 |  4461 | `					aSlot[i] = (sxi32)k;` |
|      168 |  4462 | `					aUsed[k] = 1;` |
|      168 |  4463 | `					found = 1;` |
|      168 |  4464 | `					break;` |
|        - |  4465 | `				}` |
|       62 |  4466 | `			}` |
|      181 |  4467 | `			if( !found ){` |
|       14 |  4468 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  4469 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  4470 | `				}else{` |
|        4 |  4471 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4472 | `						"Unknown named parameter $%.*s",` |
|        2 |  4473 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  4474 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  4475 | `					return PH7_ABORT;` |
|        - |  4476 | `				}` |
|        5 |  4477 | `			}` |
|       90 |  4478 | `		}else{` |
|        - |  4479 | `			/* Positional argument */` |
|       16 |  4480 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  4481 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  4482 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  4483 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  4484 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  4485 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  4486 | `					return PH7_ABORT;` |
|        - |  4487 | `				}` |
|       16 |  4488 | `				aSlot[i] = posIdx;` |
|       16 |  4489 | `				aUsed[posIdx] = 1;` |
|        7 |  4490 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  4491 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  4492 | `			}` |
|       16 |  4493 | `			posIdx++;` |
|        - |  4494 | `		}` |
|       98 |  4495 | `	}` |
|       93 |  4496 | `	return SXRET_OK;` |
|       52 |  4497 |  |
|        - |  4498 | `/*` |
|        - |  4499 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|        - |  4500 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|        - |  4501 | ` */` |
|       42 |  4502 | `static int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|        4 |  4503 |  |
|       46 |  4504 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       33 |  4505 | `		return 0;` |
|        - |  4506 | `	}` |
|       15 |  4507 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       25 |  4508 |  |
|        - |  4509 | `/*` |
|        - |  4510 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|        - |  4511 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|        - |  4512 | ` * preserved (later wins), integer keys are renumbered.` |
|        - |  4513 | ` */` |
|       10 |  4514 | `static sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4515 |  |
|       11 |  4516 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|        5 |  4517 | `	(void)pVm;` |
|       11 |  4518 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|       11 |  4519 | `	return SXRET_OK;` |
|        1 |  4520 |  |
|        - |  4521 | `/*` |
|        - |  4522 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|        - |  4523 | ` * collect values positionally (keys ignored) into a temp array.` |
|        - |  4524 | ` */` |
|        6 |  4525 | `static sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        1 |  4526 |  |
|        3 |  4527 | `	(void)pVm; (void)pKey;` |
|        7 |  4528 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|        7 |  4529 | `	return SXRET_OK;` |
|        1 |  4530 |  |
|        - |  4531 | `/*` |
|        - |  4532 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  4533 | ` *` |
|        - |  4534 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  4535 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  4536 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  4537 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  4538 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  4539 | ` * then the program execution is halted.` |
|        - |  4540 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  4541 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  4542 | ` * or to reset the VM to it's initial state.` |
|        - |  4543 | ` */` |
|    46934 |  4544 | `static sxi32 VmByteCodeExec(` |
|        - |  4545 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  4546 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  4547 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  4548 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  4549 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  4550 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  4551 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  4552 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  4553 | `	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  4554 | `	int bReturnPropagates /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value to pVm->sCatchReturn for the enclosing try handler to return. */` |
|        - |  4555 | `	)` |
|        5 |  4556 |  |
|        - |  4557 | `	VmInstr *pInstr;` |
|        - |  4558 | `	ph7_value *pTos;` |
|        - |  4559 | `	SySet aArg;` |
|        - |  4560 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  4561 | `	VmFrame *pEntryFrame;  /* Active frame at entry (for return-unwind frame teardown) */` |
|        - |  4562 | `	sxi32 pc;` |
|        - |  4563 | `	sxi32 rc;` |
|        - |  4564 | `	/* Argument container */` |
|    46939 |  4565 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    46939 |  4566 | `	if( nTos < 0 ){` |
|    43287 |  4567 | `		pTos = &pStack[-1];` |
|    21646 |  4568 | `	}else{` |
|     3657 |  4569 | `		pTos = &pStack[nTos];` |
|        - |  4570 | `	}` |
|    46939 |  4571 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    46939 |  4572 | `	pEntryFrame = pVm->pFrame;` |
|    46939 |  4573 | `	pc = nPc;` |
|        - |  4574 | `/*` |
|        - |  4575 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  4576 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  4577 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  4578 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  4579 | ` */` |
|        - |  4580 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  4581 | `	{ \` |
|        - |  4582 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  4583 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  4584 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  4585 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  4586 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  4587 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  4588 | `				break; \` |
|        - |  4589 | `			} \` |
|        - |  4590 | `			goto Exception; \` |
|        - |  4591 | `		} \` |
|        - |  4592 | `	}` |
|        - |  4593 | `	/* Execute as much as we can */` |
|  5956490 |  4594 | `	for(;;){` |
|        - |  4595 | `		/* Fetch the instruction to execute */` |
| 11912281 |  4596 | `		pInstr = &aInstr[pc];` |
| 11912281 |  4597 | `		rc = SXRET_OK;` |
|        - |  4598 | `/*` |
|        - |  4599 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4600 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4601 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4602 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4603 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4604 | ` */` |
| 11912281 |  4605 | `		switch(pInstr->iOp){` |
|        - |  4606 | `/*` |
|        - |  4607 | ` * DONE: P1 * *` |
|        - |  4608 | ` *` |
|        - |  4609 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4610 | ` * and return immediately.` |
|        - |  4611 | ` */` |
|    22976 |  4612 | `case PH7_OP_DONE:` |
|    45957 |  4613 | `	if( pInstr->iP2 && bReturnPropagates ){` |
|        - |  4614 | ``		/* Explicit `return` inside a catch/finally mini-program. Defer the value`` |
|        - |  4615 | `		 * to pVm->sCatchReturn; the enclosing try's OP_THROW / OP_POP_EXCEPTION` |
|        - |  4616 | `		 * handler materializes it into the function's result and returns. Drain` |
|        - |  4617 | `		 * any finally opened within this body first (nested try/finally inside` |
|        - |  4618 | `		 * the catch), which may itself override sCatchReturn. */` |
|       36 |  4619 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|       34 |  4620 | `			PH7_MemObjStore(pTos,&pVm->sCatchReturn);` |
|       34 |  4621 | `			VmPopOperand(&pTos,1);` |
|       18 |  4622 | `		}else{` |
|        3 |  4623 | ``			PH7_MemObjRelease(&pVm->sCatchReturn); /* bare `return;` -> null */`` |
|        - |  4624 | `		}` |
|       36 |  4625 | `		pVm->bReturnRequested = 1;` |
|       36 |  4626 | `		rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|       36 |  4627 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4628 | `			goto Abort;` |
|        - |  4629 | `		}` |
|       36 |  4630 | `		goto Done;` |
|        - |  4631 | `	}` |
|        - |  4632 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4633 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4634 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4635 | `	 * callback trampolines, and the main script. */` |
|    45918 |  4636 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0` |
|      485 |  4637 | `	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){` |
|        - |  4638 | `		/* The VM_FRAME_THROW guard skips enforcement when the function is` |
|        - |  4639 | `		 * unwinding because an exception was thrown (the compiler routes an` |
|        - |  4640 | `		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a` |
|        - |  4641 | `		 * value the function never actually returned, so enforcing here would` |
|        - |  4642 | `		 * raise a spurious "Return value must be of type X" over the real` |
|        - |  4643 | `		 * exception. */` |
|      479 |  4644 | `		ph7_value *pRetVal = 0;` |
|      479 |  4645 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      329 |  4646 | `			pRetVal = pTos;` |
|      162 |  4647 | `		}` |
|      479 |  4648 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      479 |  4649 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      473 |  4650 | `		if( rc == PH7_EXCEPTION ){` |
|        7 |  4651 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|        7 |  4652 | `				PH7_MemObjRelease(pTos);` |
|        7 |  4653 | `				pTos--;` |
|        3 |  4654 | `			}` |
|        7 |  4655 | `			goto Exception;` |
|        - |  4656 | `		}` |
|        - |  4657 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4658 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4659 | `		 * defensively we clear the pointer after a successful check). */` |
|      467 |  4660 | `		pEnforceRetFunc = 0;` |
|      231 |  4661 | `	}` |
|    45911 |  4662 | `	if( pInstr->iP1 ){` |
|        - |  4663 | `#ifdef UNTRUST` |
|        - |  4664 | `		if( pTos < pStack ){` |
|        - |  4665 | `			goto Abort;` |
|        - |  4666 | `		}` |
|        - |  4667 | `#endif` |
|    28193 |  4668 | `		if( pLastRef ){` |
|    16897 |  4669 | `			*pLastRef = pTos->nIdx;` |
|     8446 |  4670 | `		}` |
|    28193 |  4671 | `		if( pResult ){` |
|        - |  4672 | `			/* Execution result */` |
|    26523 |  4673 | `			PH7_MemObjStore(pTos,pResult);` |
|    13259 |  4674 | `		}` |
|    28193 |  4675 | `		VmPopOperand(&pTos,1);` |
|    31817 |  4676 | `	}else if( pLastRef ){` |
|        - |  4677 | `		/* Nothing referenced */` |
|     2095 |  4678 | `		*pLastRef = SXU32_HIGH;` |
|     1045 |  4679 | `	}` |
|        - |  4680 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4681 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4682 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4683 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4684 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4685 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4686 | `	 * block can override it (the finally writes pVm->sCatchReturn, materialized` |
|        - |  4687 | `	 * below).` |
|        - |  4688 | `	 */` |
|    45911 |  4689 | `	rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|    45911 |  4690 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4691 | `		goto Abort;` |
|        - |  4692 | `	}` |
|    45911 |  4693 | `	if( pVm->bReturnRequested && !bReturnPropagates ){` |
|        - |  4694 | `		/* A drained finally issued a 'return' that overrides this one. */` |
|        8 |  4695 | `		VmMaterializeCatchReturn(&(*pVm),pResult,pEntryFrame);` |
|        3 |  4696 | `	}` |
|    45911 |  4697 | `	goto Done;` |
|        - |  4698 | `/*` |
|        - |  4699 | ` * HALT: P1 * *` |
|        - |  4700 | ` *` |
|        - |  4701 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4702 | ` * and abort immediately.` |
|        - |  4703 | ` */` |
|        7 |  4704 | `case PH7_OP_HALT:` |
|       18 |  4705 | `	if( pInstr->iP1 ){` |
|        - |  4706 | `#ifdef UNTRUST` |
|        - |  4707 | `		if( pTos < pStack ){` |
|        - |  4708 | `			goto Abort;` |
|        - |  4709 | `		}` |
|        - |  4710 | `#endif` |
|       18 |  4711 | `		if( pLastRef ){` |
|        3 |  4712 | `			*pLastRef = pTos->nIdx;` |
|        1 |  4713 | `		}` |
|       18 |  4714 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|       13 |  4715 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4716 | `				/* Output the exit message */` |
|       18 |  4717 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        5 |  4718 | `					pVm->sVmConsumer.pUserData);` |
|       13 |  4719 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        8 |  4720 | `			}` |
|       11 |  4721 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4722 | `			/* Record exit status */` |
|        6 |  4723 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4724 | `		}` |
|       18 |  4725 | `		VmPopOperand(&pTos,1);` |
|        7 |  4726 | `	}else if( pLastRef ){` |
|        - |  4727 | `		/* Nothing referenced */` |
|      ! 0 |  4728 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4729 | `	}` |
|        - |  4730 | `	/* Request a VM-wide halt so the abort cascades out of any enclosing` |
|        - |  4731 | `	 * include/require/eval execution unit; shutdown callbacks then run` |
|        - |  4732 | `	 * at the top level (PHP semantics) instead of hard-exiting here.` |
|        - |  4733 | `	 */` |
|       18 |  4734 | `	pVm->bHaltRequested = 1;` |
|       18 |  4735 | `	goto Abort;` |
|        - |  4736 | `/*` |
|        - |  4737 | ` * JMP: * P2 *` |
|        - |  4738 | ` *` |
|        - |  4739 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4740 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4741 | ` */` |
|   253694 |  4742 | `case PH7_OP_JMP:` |
|   507437 |  4743 | `	pc = pInstr->iP2 - 1;` |
|   507437 |  4744 | `	break;` |
|        - |  4745 | `/*` |
|        - |  4746 | ` * JZ: P1 P2 *` |
|        - |  4747 | ` *` |
|        - |  4748 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4749 | ` * entry in the stack if P1 is zero.` |
|        - |  4750 | ` */` |
|   602574 |  4751 | `case PH7_OP_JZ:` |
|        - |  4752 | `#ifdef UNTRUST` |
|        - |  4753 | `	if( pTos < pStack ){` |
|        - |  4754 | `		goto Abort;` |
|        - |  4755 | `	}` |
|        - |  4756 | `#endif` |
|        - |  4757 | `	/* Get a boolean value */` |
|  1205241 |  4758 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      175 |  4759 | `		PH7_MemObjToBool(pTos);` |
|       86 |  4760 | `	}` |
|  1205241 |  4761 | `	if( !pTos->x.iVal ){` |
|        - |  4762 | `		/* Take the jump */` |
|   620539 |  4763 | `		pc = pInstr->iP2 - 1;` |
|   310267 |  4764 | `	}` |
|  1205241 |  4765 | `	if( !pInstr->iP1 ){` |
|   954121 |  4766 | `		VmPopOperand(&pTos,1);` |
|   477080 |  4767 | `	}` |
|  1205241 |  4768 | `	break;` |
|        - |  4769 | `/*` |
|        - |  4770 | ` * JNZ: P1 P2 *` |
|        - |  4771 | ` *` |
|        - |  4772 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4773 | ` * entry in the stack if P1 is zero.` |
|        - |  4774 | ` */` |
|    61484 |  4775 | `case PH7_OP_JNZ:` |
|        - |  4776 | `#ifdef UNTRUST` |
|        - |  4777 | `	if( pTos < pStack ){` |
|        - |  4778 | `		goto Abort;` |
|        - |  4779 | `	}` |
|        - |  4780 | `#endif` |
|        - |  4781 | `	/* Get a boolean value */` |
|   122973 |  4782 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4783 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4784 | `	}` |
|   122973 |  4785 | `	if( pTos->x.iVal ){` |
|        - |  4786 | `		/* Take the jump */` |
|     5675 |  4787 | `		pc = pInstr->iP2 - 1;` |
|     2835 |  4788 | `	}` |
|   122973 |  4789 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4790 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4791 | `	}` |
|   122973 |  4792 | `	break;` |
|        - |  4793 | `/*` |
|        - |  4794 | ` * NOOP: * * *` |
|        - |  4795 | ` *` |
|        - |  4796 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4797 | ` * destination.` |
|        - |  4798 | ` */` |
|      ! 0 |  4799 | `case PH7_OP_NOOP:` |
|      ! 0 |  4800 | `	break;` |
|        - |  4801 | `/*` |
|        - |  4802 | ` * POP: P1 * *` |
|        - |  4803 | ` *` |
|        - |  4804 | ` * Pop P1 elements from the operand stack.` |
|        - |  4805 | ` */` |
|   466908 |  4806 | `case PH7_OP_POP: {` |
|   933865 |  4807 | `	sxi32 n = pInstr->iP1;` |
|   933865 |  4808 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4809 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       53 |  4810 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4811 | `	}` |
|   933865 |  4812 | `	VmPopOperand(&pTos,n);` |
|   933865 |  4813 | `	break;` |
|        - |  4814 | `				 }` |
|        - |  4815 | `/*` |
|        - |  4816 | ` * DUP: * * *` |
|        - |  4817 | ` *` |
|        - |  4818 | ` * Duplicate the top of the stack.` |
|        - |  4819 | ` */` |
|       41 |  4820 | `case PH7_OP_DUP:` |
|        - |  4821 | `#ifdef UNTRUST` |
|        - |  4822 | `	if( pTos < pStack ){` |
|        - |  4823 | `		goto Abort;` |
|        - |  4824 | `	}` |
|        - |  4825 | `#endif` |
|       84 |  4826 | `	pTos++;` |
|       84 |  4827 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4828 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4829 | `	break;` |
|        - |  4830 | `/*` |
|        - |  4831 | ` * NSSWITCH: * * P3` |
|        - |  4832 | ` *` |
|        - |  4833 | ` * Switch the active namespace at runtime.` |
|        - |  4834 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4835 | ` */` |
|     7925 |  4836 | `case PH7_OP_NSSWITCH:` |
|    15855 |  4837 | `	SyBlobReset(&pVm->sNamespace);` |
|    15855 |  4838 | `	if( pInstr->p3 ){` |
|      103 |  4839 | `		const char *zNs = (const char *)pInstr->p3;` |
|      103 |  4840 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       49 |  4841 | `	}` |
|        - |  4842 | `	/* Clear namespace-scoped use-const imports */` |
|    15855 |  4843 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15855 |  4844 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15855 |  4845 | `	break;` |
|        - |  4846 | `/* OP_USECONST P1 * P3` |
|        - |  4847 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4848 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4849 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4850 | ` */` |
|        7 |  4851 | `case PH7_OP_USECONST: {` |
|       16 |  4852 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4853 | `	if( azPair ){` |
|       16 |  4854 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4855 | `	}` |
|       16 |  4856 | `	break;` |
|        - |  4857 | `				}` |
|        - |  4858 | `/*` |
|        - |  4859 | ` * CVT_INT: * * *` |
|        - |  4860 | ` *` |
|        - |  4861 | ` * Force the top of the stack to be an integer.` |
|        - |  4862 | ` */` |
|       80 |  4863 | `case PH7_OP_CVT_INT:` |
|        - |  4864 | `#ifdef UNTRUST` |
|        - |  4865 | `	if( pTos < pStack ){` |
|        - |  4866 | `		goto Abort;` |
|        - |  4867 | `	}` |
|        - |  4868 | `#endif` |
|      165 |  4869 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      115 |  4870 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4871 | `	}` |
|        - |  4872 | `	/* Invalidate any prior representation */` |
|      165 |  4873 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      165 |  4874 | `	break;` |
|        - |  4875 | `/*` |
|        - |  4876 | ` * CVT_REAL: * * *` |
|        - |  4877 | ` *` |
|        - |  4878 | ` * Force the top of the stack to be a real.` |
|        - |  4879 | ` */` |
|        7 |  4880 | `case PH7_OP_CVT_REAL:` |
|        - |  4881 | `#ifdef UNTRUST` |
|        - |  4882 | `	if( pTos < pStack ){` |
|        - |  4883 | `		goto Abort;` |
|        - |  4884 | `	}` |
|        - |  4885 | `#endif` |
|       15 |  4886 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 |  4887 | `		PH7_MemObjToReal(pTos);` |
|        5 |  4888 | `	}` |
|        - |  4889 | `	/* Invalidate any prior representation */` |
|       15 |  4890 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       15 |  4891 | `	break;` |
|        - |  4892 | `/*` |
|        - |  4893 | ` * CVT_STR: * * *` |
|        - |  4894 | ` *` |
|        - |  4895 | ` * Force the top of the stack to be a string.` |
|        - |  4896 | ` */` |
|      163 |  4897 | `case PH7_OP_CVT_STR:` |
|        - |  4898 | `#ifdef UNTRUST` |
|        - |  4899 | `	if( pTos < pStack ){` |
|        - |  4900 | `		goto Abort;` |
|        - |  4901 | `	}` |
|        - |  4902 | `#endif` |
|      330 |  4903 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      310 |  4904 | `		PH7_MemObjToString(pTos);` |
|      153 |  4905 | `	}` |
|      330 |  4906 | `	break;` |
|        - |  4907 | `/*` |
|        - |  4908 | ` * CVT_BOOL: * * *` |
|        - |  4909 | ` *` |
|        - |  4910 | ` * Force the top of the stack to be a boolean.` |
|        - |  4911 | ` */` |
|        5 |  4912 | `case PH7_OP_CVT_BOOL:` |
|        - |  4913 | `#ifdef UNTRUST` |
|        - |  4914 | `	if( pTos < pStack ){` |
|        - |  4915 | `		goto Abort;` |
|        - |  4916 | `	}` |
|        - |  4917 | `#endif` |
|       11 |  4918 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4919 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4920 | `	}` |
|       11 |  4921 | `	break;` |
|        - |  4922 | `/*` |
|        - |  4923 | ` * CVT_NULL: * * *` |
|        - |  4924 | ` *` |
|        - |  4925 | ` * Nullify the top of the stack.` |
|        - |  4926 | ` */` |
|        3 |  4927 | `case PH7_OP_CVT_NULL:` |
|        - |  4928 | `#ifdef UNTRUST` |
|        - |  4929 | `	if( pTos < pStack ){` |
|        - |  4930 | `		goto Abort;` |
|        - |  4931 | `	}` |
|        - |  4932 | `#endif` |
|        7 |  4933 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4934 | `	break;` |
|        - |  4935 | `/*` |
|        - |  4936 | ` * CVT_NUMC: * * *` |
|        - |  4937 | ` *` |
|        - |  4938 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4939 | ` */` |
|      ! 0 |  4940 | `case PH7_OP_CVT_NUMC:` |
|        - |  4941 | `#ifdef UNTRUST` |
|        - |  4942 | `	if( pTos < pStack ){` |
|        - |  4943 | `		goto Abort;` |
|        - |  4944 | `	}` |
|        - |  4945 | `#endif` |
|        - |  4946 | `	/* Force a numeric cast */` |
|      ! 0 |  4947 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4948 | `	break;` |
|        - |  4949 | `/*` |
|        - |  4950 | ` * CVT_ARRAY: * * *` |
|        - |  4951 | ` *` |
|        - |  4952 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4953 | ` */` |
|       10 |  4954 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4955 | `#ifdef UNTRUST` |
|        - |  4956 | `	if( pTos < pStack ){` |
|        - |  4957 | `		goto Abort;` |
|        - |  4958 | `	}` |
|        - |  4959 | `#endif` |
|        - |  4960 | `	/* Force a hashmap cast */` |
|       21 |  4961 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4962 | `	if( rc != SXRET_OK ){` |
|        - |  4963 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4964 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4965 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4966 | `	}` |
|       21 |  4967 | `	break;` |
|        - |  4968 | `/*` |
|        - |  4969 | ` * CVT_OBJ: * * *` |
|        - |  4970 | ` *` |
|        - |  4971 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4972 | ` */` |
|        8 |  4973 | `case PH7_OP_CVT_OBJ:` |
|        - |  4974 | `#ifdef UNTRUST` |
|        - |  4975 | `	if( pTos < pStack ){` |
|        - |  4976 | `		goto Abort;` |
|        - |  4977 | `	}` |
|        - |  4978 | `#endif` |
|       17 |  4979 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4980 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4981 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4982 | `	}` |
|       17 |  4983 | `	break;` |
|        - |  4984 | `/*` |
|        - |  4985 | ` * ERR_CTRL * * *` |
|        - |  4986 | ` *` |
|        - |  4987 | ` * Error control operator.` |
|        - |  4988 | ` */` |
|    16287 |  4989 | `case PH7_OP_ERR_CTRL:` |
|        - |  4990 | `	/*` |
|        - |  4991 | `	 * TICKET 1433-038:` |
|        - |  4992 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4993 | `	 * use the public API,to control error output.` |
|        - |  4994 | `	 */` |
|    32574 |  4995 | `	break;` |
|        - |  4996 | `/*` |
|        - |  4997 | ` * IS_A * * *` |
|        - |  4998 | ` *` |
|        - |  4999 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  5000 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  5001 | ` * holding a class name or an object).` |
|        - |  5002 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  5003 | ` */` |
|       77 |  5004 | `case PH7_OP_IS_A:{` |
|      159 |  5005 | `	ph7_value *pNos = &pTos[-1];` |
|      159 |  5006 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  5007 | `#ifdef UNTRUST` |
|        - |  5008 | `	if( pNos < pStack ){` |
|        - |  5009 | `		goto Abort;` |
|        - |  5010 | `	}` |
|        - |  5011 | `#endif` |
|      159 |  5012 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      157 |  5013 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      157 |  5014 | `		ph7_class *pClass = 0;` |
|        - |  5015 | `		/* Extract the target class */` |
|      157 |  5016 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5017 | `			/* Instance already loaded */` |
|      ! 0 |  5018 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      157 |  5019 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      157 |  5020 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      157 |  5021 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  5022 | `			/* Handle self/static/parent keywords */` |
|      157 |  5023 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        6 |  5024 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      155 |  5025 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  5026 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      154 |  5027 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        6 |  5028 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        6 |  5029 | `				if( pSelf && pSelf->pBase ){` |
|        6 |  5030 | `					pClass = pSelf->pBase;` |
|        2 |  5031 | `				}` |
|        4 |  5032 | `			}else{` |
|      147 |  5033 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5034 | `			}` |
|       76 |  5035 | `		}` |
|      157 |  5036 | `		if( pClass ){` |
|        - |  5037 | `			/* Perform the query */` |
|      157 |  5038 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       76 |  5039 | `		}` |
|       76 |  5040 | `	}` |
|        - |  5041 | `	/* Push result */` |
|      159 |  5042 | `	VmPopOperand(&pTos,1);` |
|      159 |  5043 | `	PH7_MemObjRelease(pTos);` |
|      159 |  5044 | `	pTos->x.iVal = iRes;` |
|      159 |  5045 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      159 |  5046 | `	break;` |
|        - |  5047 | `				 }` |
|        - |  5048 |  |
|        - |  5049 | `/*` |
|        - |  5050 | ` * LOADC P1 P2 *` |
|        - |  5051 | ` *` |
|        - |  5052 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  5053 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  5054 | ` */` |
|  1028366 |  5055 | `case PH7_OP_LOADC: {` |
|        - |  5056 | `	ph7_value *pObj;` |
|        - |  5057 | `	/* Reserve a room */` |
|  2056781 |  5058 | `	pTos++;` |
|  3075187 |  5059 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  2056781 |  5060 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  5061 | `			SyHashEntry *pEntry;` |
|        - |  5062 | `			/* Check use const imports first — imports take precedence */` |
|        - |  5063 | `			{` |
|        - |  5064 | `				SyHashEntry *pConstImport;` |
|    30023 |  5065 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    20012 |  5066 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    20017 |  5067 | `				if( pConstImport ){` |
|       11 |  5068 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  5069 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  5070 | `					if( pEntry ){` |
|       11 |  5071 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  5072 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  5073 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  5074 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  5075 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  5076 | `						break;` |
|        - |  5077 | `					}` |
|        - |  5078 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  5079 | `				}` |
|        - |  5080 | `			}` |
|        - |  5081 | `			/* Candidate for expansion via user defined callbacks */` |
|    20007 |  5082 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    20007 |  5083 | `			if( pEntry ){` |
|    20001 |  5084 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  5085 | `				/* Set a NULL default value */` |
|    20001 |  5086 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    20001 |  5087 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  5088 | `				/* Invoke the callback and deal with the expanded value */` |
|    20001 |  5089 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  5090 | `				/* Mark as constant */` |
|    20001 |  5091 | `				pTos->nIdx = SXU32_HIGH;` |
|    20001 |  5092 | `				break;` |
|        - |  5093 | `			}` |
|        - |  5094 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  5095 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  5096 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  5097 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  5098 | `			{` |
|        9 |  5099 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        9 |  5100 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  5101 | `				sxu32 j;` |
|        9 |  5102 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       25 |  5103 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  5104 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  5105 | `				}` |
|        9 |  5106 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  5107 | `					/* Try current_namespace\name */` |
|      ! 0 |  5108 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  5109 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  5110 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  5111 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  5112 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  5113 | `					if( pEntry ){` |
|      ! 0 |  5114 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  5115 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5116 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  5117 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  5118 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5119 | `						break;` |
|        - |  5120 | `					}` |
|        - |  5121 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  5122 | `				}` |
|        9 |  5123 | `				if( isQualified ){` |
|        - |  5124 | `					/* Qualified name: must be a real constant. */` |
|        3 |  5125 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  5126 | `					SyBlob sErr;` |
|        3 |  5127 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  5128 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  5129 | `					if( pErrFile ){` |
|        3 |  5130 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  5131 | `					}` |
|        3 |  5132 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  5133 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  5134 | `					SyBlobRelease(&sErr);` |
|        3 |  5135 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  5136 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  5137 | `					goto LoadC_Done;` |
|        - |  5138 | `				}` |
|        - |  5139 | `			}` |
|        2 |  5140 | `		}` |
|  2036773 |  5141 | `		PH7_MemObjLoad(pObj,pTos);` |
|  1018411 |  5142 | `	}else{` |
|        - |  5143 | `		/* Set a NULL value */` |
|      ! 0 |  5144 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5145 | `	}` |
|  1018363 |  5146 | `LoadC_Done:` |
|        - |  5147 | `	/* Mark as constant */` |
|  2036775 |  5148 | `	pTos->nIdx = SXU32_HIGH;` |
|  2036775 |  5149 | `	break;` |
|        - |  5150 | `				  }` |
|        - |  5151 | `/*` |
|        - |  5152 | ` * LOAD: P1 * P3` |
|        - |  5153 | ` *` |
|        - |  5154 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  5155 | ` * from the P3 operand.` |
|        - |  5156 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  5157 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  5158 | ` */` |
|  1586945 |  5159 | `case PH7_OP_LOAD:{` |
|        - |  5160 | `	ph7_value *pObj;` |
|        - |  5161 | `	SyString sName;` |
|  3174115 |  5162 | `	if( pInstr->p3 == 0 ){` |
|        - |  5163 | `		/* Take the variable name from the top of the stack */` |
|        - |  5164 | `#ifdef UNTRUST` |
|        - |  5165 | `		if( pTos < pStack ){` |
|        - |  5166 | `			goto Abort;` |
|        - |  5167 | `		}` |
|        - |  5168 | `#endif` |
|        - |  5169 | `		/* Force a string cast */` |
|       19 |  5170 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5171 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5172 | `		}` |
|       19 |  5173 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  5174 | `	}else{` |
|  3174097 |  5175 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5176 | `		/* Reserve a room for the target object */` |
|  3174097 |  5177 | `		pTos++;` |
|        - |  5178 | `	}` |
|        - |  5179 | `	/* Extract the requested memory object */` |
|  3174115 |  5180 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3174115 |  5181 | `	if( pObj == 0 ){` |
|      859 |  5182 | `		if( pInstr->iP1 ){` |
|        - |  5183 | `			/* Variable not found,load NULL */` |
|      859 |  5184 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5185 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5186 | `			}else{` |
|      859 |  5187 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5188 | `			}` |
|      859 |  5189 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1587377 |  5190 | `			break;` |
|      ! 0 |  5191 | `		}else{` |
|        - |  5192 | `			/* Fatal error */` |
|      ! 0 |  5193 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5194 | `			goto Abort;` |
|        - |  5195 | `		}` |
|        - |  5196 | `	}` |
|        - |  5197 | `	/* Load variable contents */` |
|  3173261 |  5198 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3173261 |  5199 | `	pTos->nIdx = pObj->nIdx;` |
|  3173261 |  5200 | `	break;` |
|        - |  5201 | `				   }` |
|        - |  5202 | `/*` |
|        - |  5203 | ` * LOAD_MAP P1 * *` |
|        - |  5204 | ` *` |
|        - |  5205 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  5206 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  5207 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  5208 | ` */` |
|    23098 |  5209 | `case PH7_OP_LOAD_MAP: {` |
|        - |  5210 | `	ph7_hashmap *pMap;` |
|        - |  5211 | `	/* Allocate a new hashmap instance */` |
|    46201 |  5212 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    46201 |  5213 | `	if( pMap == 0 ){` |
|      ! 0 |  5214 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5215 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  5216 | `		goto Abort;` |
|        - |  5217 | `	}` |
|    46201 |  5218 | `	if( pInstr->iP1 > 0 ){` |
|     2813 |  5219 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2813 |  5220 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  5221 | `		/* Perform the insertion */` |
|     8579 |  5222 | `		while( pEntry < pTos ){` |
|     5789 |  5223 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  5224 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  5225 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  5226 | `				 * renumbered. Same routine that backs array_merge. */` |
|       77 |  5227 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       53 |  5228 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       53 |  5229 | `					if( rcMerge != SXRET_OK ){` |
|        - |  5230 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  5231 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  5232 | `						 * map dangling. */` |
|      ! 0 |  5233 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  5234 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  5235 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  5236 | `						break;` |
|        1 |  5237 | `					}` |
|       51 |  5238 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|        - |  5239 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|        - |  5240 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|        5 |  5241 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|        5 |  5242 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  5243 | `						rcSpread = rcW;` |
|      ! 0 |  5244 | `						break;` |
|        - |  5245 | `					}` |
|        3 |  5246 | `				}else{` |
|        - |  5247 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       21 |  5248 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       21 |  5249 | `					break;` |
|        1 |  5250 | `				}` |
|     5743 |  5251 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  5252 | `				/* Insertion by reference */` |
|      151 |  5253 | `				PH7_HashmapInsertByRef(pMap,` |
|      100 |  5254 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|      100 |  5255 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  5256 | `					);` |
|       51 |  5257 | `			}else{` |
|        - |  5258 | `				/* Standard insertion */` |
|     8420 |  5259 | `				PH7_HashmapInsert(pMap,` |
|     5610 |  5260 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2805 |  5261 | `					&pEntry[1]` |
|        - |  5262 | `				);` |
|        - |  5263 | `			}` |
|        - |  5264 | `			/* Next pair on the stack */` |
|     5771 |  5265 | `			pEntry += 2;` |
|        5 |  5266 | `		}` |
|        - |  5267 | `		/* Pop P1 elements */` |
|     2813 |  5268 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2813 |  5269 | `		if( rcSpread != SXRET_OK ){` |
|        - |  5270 | `			/* Discard the partially-built map and propagate the exception. */` |
|       21 |  5271 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       21 |  5272 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  5273 | `				goto Abort;` |
|        - |  5274 | `			}` |
|        - |  5275 | `			{` |
|       21 |  5276 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       21 |  5277 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        6 |  5278 | `					pc = pFrm2->iExceptionJump - 1;` |
|        6 |  5279 | `					break;` |
|        - |  5280 | `				}` |
|        - |  5281 | `			}` |
|       15 |  5282 | `			goto Exception;` |
|        - |  5283 | `		}` |
|     1395 |  5284 | `	}` |
|        - |  5285 | `	/* Push the hashmap */` |
|    46183 |  5286 | `	pTos++;` |
|    46183 |  5287 | `	pTos->nIdx = SXU32_HIGH;` |
|    46183 |  5288 | `	pTos->x.pOther = pMap;` |
|    46183 |  5289 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    46183 |  5290 | `	break;` |
|        - |  5291 | `					  }` |
|        - |  5292 | `/*` |
|        - |  5293 | ` * LOAD_LIST: P1 * *` |
|        - |  5294 | ` *` |
|        - |  5295 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  5296 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  5297 | ` * Caveats:` |
|        - |  5298 | ` *  This implementation support only a single nesting level.` |
|        - |  5299 | ` */` |
|       48 |  5300 | `case PH7_OP_LOAD_LIST: {` |
|        - |  5301 | `	ph7_value *pEntry;` |
|       98 |  5302 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  5303 | `		/* Empty list,break immediately */` |
|      ! 0 |  5304 | `		break;` |
|        - |  5305 | `	}` |
|       98 |  5306 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  5307 | `#ifdef UNTRUST` |
|        - |  5308 | `	if( &pEntry[-1] < pStack ){` |
|        - |  5309 | `		goto Abort;` |
|        - |  5310 | `	}` |
|        - |  5311 | `#endif` |
|       98 |  5312 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  5313 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  5314 | `		ph7_hashmap_node *pNode;` |
|        - |  5315 | `		ph7_value sKey,*pObj;` |
|        - |  5316 | `		/* Start Copying */` |
|       91 |  5317 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  5318 | `		while( pEntry <= pTos ){` |
|      193 |  5319 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  5320 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  5321 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  5322 | `					if( rc == SXRET_OK ){` |
|        - |  5323 | `						/* Store node value */` |
|      165 |  5324 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  5325 | `					}else{` |
|        - |  5326 | `						/* Undefined array key */` |
|        - |  5327 | `						char zMsg[128];` |
|      ! 0 |  5328 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  5329 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5330 | `						PH7_MemObjRelease(pObj);` |
|        - |  5331 | `					}` |
|       82 |  5332 | `				}` |
|       82 |  5333 | `			}` |
|      193 |  5334 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  5335 | `			pEntry++;` |
|        1 |  5336 | `		}` |
|       46 |  5337 | `	}else{` |
|        - |  5338 | `		/* Source is not an array */` |
|        - |  5339 | `		ph7_value *pObj;` |
|       18 |  5340 | `		while( pEntry <= pTos ){` |
|       12 |  5341 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  5342 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  5343 | `					PH7_MemObjRelease(pObj);` |
|        5 |  5344 | `				}` |
|        5 |  5345 | `			}` |
|       12 |  5346 | `			pEntry++;` |
|        2 |  5347 | `		}` |
|        8 |  5348 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  5349 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  5350 | `			const char *zType = "unknown";` |
|        3 |  5351 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  5352 | `			char zMsg[256];` |
|        3 |  5353 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  5354 | `				zType = "string";` |
|        1 |  5355 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5356 | `				zType = "int";` |
|      ! 0 |  5357 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5358 | `				zType = "float";` |
|      ! 0 |  5359 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  5360 | `				zType = "object";` |
|      ! 0 |  5361 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  5362 | `				zType = "resource";` |
|      ! 0 |  5363 | `			}` |
|        3 |  5364 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  5365 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  5366 | `		}` |
|        - |  5367 | `	}` |
|       98 |  5368 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  5369 | `	break;` |
|        - |  5370 | `					   }` |
|        - |  5371 | `/*` |
|        - |  5372 | ` * LOAD_IDX: P1 P2 *` |
|        - |  5373 | ` *` |
|        - |  5374 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  5375 | ` * from the stack.` |
|        - |  5376 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  5377 | ` * instead.` |
|        - |  5378 | ` */` |
|   251712 |  5379 | `case PH7_OP_LOAD_IDX: {` |
|   503473 |  5380 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   503473 |  5381 | `	ph7_hashmap *pMap = 0;` |
|        - |  5382 | `	ph7_value *pIdx;` |
|   503473 |  5383 | `	pIdx = 0;` |
|   503473 |  5384 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  5385 | `		if( !pInstr->iP2){` |
|        - |  5386 | `			/* No available index,load NULL */` |
|      ! 0 |  5387 | `			if( pTos >= pStack ){` |
|      ! 0 |  5388 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5389 | `			}else{` |
|        - |  5390 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  5391 | `				pTos++;` |
|      ! 0 |  5392 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  5393 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5394 | `			}` |
|        - |  5395 | `			/* Emit a notice */` |
|      ! 0 |  5396 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  5397 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  5398 | `			break;` |
|        - |  5399 | `		}` |
|      ! 0 |  5400 | `	}else{` |
|   503473 |  5401 | `		pIdx = pTos;` |
|   503473 |  5402 | `		pTos--;` |
|        - |  5403 | `	}` |
|   503473 |  5404 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  5405 | `		/* String access */` |
|   388239 |  5406 | `		if( pIdx ){` |
|        - |  5407 | `			sxu32 nOfft;` |
|   388239 |  5408 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  5409 | `				/* Force an int cast */` |
|      ! 0 |  5410 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5411 | `			}` |
|   388239 |  5412 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   388239 |  5413 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  5414 | `				/* Invalid offset,load null */` |
|      ! 0 |  5415 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5416 | `			}else{` |
|   388239 |  5417 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   388239 |  5418 | `				int c = zData[nOfft];` |
|   388239 |  5419 | `				PH7_MemObjRelease(pTos);` |
|   388239 |  5420 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   388239 |  5421 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  5422 | `			}` |
|   194144 |  5423 | `		}else{` |
|        - |  5424 | `			/* No available index,load NULL */` |
|      ! 0 |  5425 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  5426 | `		}` |
|   388239 |  5427 | `		break;` |
|        - |  5428 | `	}` |
|   115239 |  5429 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5430 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  5431 | `		 * iP2 codes:` |
|        - |  5432 | `		 *   0 = read       → offsetGet` |
|        - |  5433 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  5434 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  5435 | `		 *   4 = isset()    → offsetExists` |
|        - |  5436 | `		 *   5 = unset()    → offsetUnset` |
|        - |  5437 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      129 |  5438 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      129 |  5439 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      129 |  5440 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5441 | `			ph7_class_method *pMeth;` |
|        - |  5442 | `			ph7_value sResult;` |
|        - |  5443 | `			ph7_value *apArg[1];` |
|      127 |  5444 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  5445 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  5446 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  5447 | `					"Cannot use [] for reading");` |
|      ! 0 |  5448 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5449 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5450 | `				break;` |
|        - |  5451 | `			}` |
|      127 |  5452 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      127 |  5453 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  5454 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       54 |  5455 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5456 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       54 |  5457 | `				apArg[0] = pIdx;` |
|       54 |  5458 | `				if( pMeth ){` |
|       54 |  5459 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       29 |  5460 | `				}` |
|      102 |  5461 | `			}else if( pInstr->iP2 == 5 ){` |
|       11 |  5462 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5463 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|       11 |  5464 | `				apArg[0] = pIdx;` |
|       11 |  5465 | `				if( pMeth ){` |
|       11 |  5466 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  5467 | `				}` |
|        7 |  5468 | `			}else{` |
|       69 |  5469 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5470 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       69 |  5471 | `				apArg[0] = pIdx;` |
|       69 |  5472 | `				if( pMeth ){` |
|       69 |  5473 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  5474 | `				}` |
|        - |  5475 | `			}` |
|      127 |  5476 | `			if( pInstr->iP2 == 4 ){` |
|        - |  5477 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  5478 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  5479 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       36 |  5480 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       36 |  5481 | `				PH7_MemObjRelease(pTos);` |
|       36 |  5482 | `				pTos->nIdx = SXU32_HIGH;` |
|       36 |  5483 | `				if( bExists ){` |
|       19 |  5484 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       19 |  5485 | `					pTos->x.iVal = 1;` |
|       11 |  5486 | `				}else{` |
|       20 |  5487 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        4 |  5488 | `				}` |
|      111 |  5489 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  5490 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  5491 | `				 * vm_builtin_unset is a harmless no-op. */` |
|       11 |  5492 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5493 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5494 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       91 |  5495 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  5496 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  5497 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  5498 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  5499 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  5500 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  5501 | `				PH7_MemObjRelease(pTos);` |
|       11 |  5502 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  5503 | `				if( !bExists ){` |
|        3 |  5504 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  5505 | `				}else{` |
|        9 |  5506 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5507 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  5508 | `					ph7_value sValue;` |
|        9 |  5509 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  5510 | `					apArg[0] = pIdx;` |
|        9 |  5511 | `					if( pGet ){` |
|        9 |  5512 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  5513 | `					}` |
|        9 |  5514 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  5515 | `					PH7_MemObjRelease(&sValue);` |
|        - |  5516 | `				}` |
|       11 |  5517 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  5518 | `				break; /* skip the duplicate sResult release below */` |
|       77 |  5519 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  5520 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  5521 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  5522 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  5523 | `				 *     and push NULL.` |
|        - |  5524 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|       10 |  5525 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       10 |  5526 | `				int bShouldArm = !bExists;` |
|        - |  5527 | `				ph7_value sValue;` |
|       10 |  5528 | `				PH7_MemObjRelease(&sResult);` |
|        - |  5529 | `				/* Reset any prior arming defensively */` |
|       10 |  5530 | `				VmCoalesceDisarm(pVm);` |
|       10 |  5531 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|       10 |  5532 | `				if( bExists ){` |
|        5 |  5533 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5534 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  5535 | `					apArg[0] = pIdx;` |
|        5 |  5536 | `					if( pGet ){` |
|        5 |  5537 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  5538 | `					}` |
|        5 |  5539 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  5540 | `						bShouldArm = 1;` |
|        1 |  5541 | `					}` |
|        2 |  5542 | `				}` |
|       10 |  5543 | `				PH7_MemObjRelease(pTos);` |
|       10 |  5544 | `				pTos->nIdx = SXU32_HIGH;` |
|       10 |  5545 | `				if( bShouldArm ){` |
|        - |  5546 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  5547 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  5548 | `					 * intervening expression evaluation. */` |
|        8 |  5549 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        8 |  5550 | `					if( pIdx ){` |
|        8 |  5551 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  5552 | `					}` |
|        8 |  5553 | `					pVm->pCoalesceObj = pInst;` |
|        8 |  5554 | `					pInst->iRef++;` |
|        8 |  5555 | `					pVm->bCoalesceArmed = 1;` |
|        5 |  5556 | `				}else{` |
|        3 |  5557 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  5558 | `				}` |
|       10 |  5559 | `				PH7_MemObjRelease(&sValue);` |
|       10 |  5560 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       10 |  5561 | `				break;` |
|      ! 0 |  5562 | `			}else{` |
|        - |  5563 | `				/* offsetGet: replace pTos with the returned value. */` |
|       69 |  5564 | `				PH7_MemObjRelease(pTos);` |
|       69 |  5565 | `				PH7_MemObjStore(&sResult,pTos);` |
|       69 |  5566 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  5567 | `			}` |
|      109 |  5568 | `			PH7_MemObjRelease(&sResult);` |
|      109 |  5569 | `			if( pIdx ){` |
|      109 |  5570 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  5571 | `			}` |
|      109 |  5572 | `			break;` |
|        - |  5573 | `		}` |
|        - |  5574 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  5575 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  5576 | `		if( pInst ){` |
|        - |  5577 | `			char zMsg[256];` |
|        3 |  5578 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  5579 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5580 | `				"Cannot use object of type %.*s as array",` |
|        2 |  5581 | `				(int)pName->nByte,pName->zString);` |
|        3 |  5582 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5583 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  5584 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5585 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5586 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5587 | `			break;` |
|        - |  5588 | `		}` |
|      ! 0 |  5589 | `	}` |
|   115115 |  5590 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  5591 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5592 | `			ph7_value *pObj;` |
|        3 |  5593 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5594 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  5595 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  5596 | `			}` |
|        1 |  5597 | `		}` |
|        1 |  5598 | `	}` |
|   115115 |  5599 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   115115 |  5600 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   115115 |  5601 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  5602 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  5603 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  5604 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  5605 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  5606 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  5607 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      898 |  5608 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      447 |  5609 | `		}` |
|        - |  5610 | `		/* Point to the hashmap */` |
|   115115 |  5611 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   115115 |  5612 | `		if( pIdx ){` |
|        - |  5613 | `			/* Load the desired entry */` |
|   115115 |  5614 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    57555 |  5615 | `		}` |
|   115115 |  5616 | `		if( pInstr->iP2 == 3 ){` |
|        - |  5617 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  5618 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  5619 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  5620 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  5621 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5622 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5623 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5624 | `			 * correct for the outermost write. */` |
|       19 |  5625 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5626 | `			if( !needWrite && pNode ){` |
|       13 |  5627 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5628 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5629 | `					needWrite = 1;` |
|        3 |  5630 | `				}` |
|        6 |  5631 | `			}` |
|       19 |  5632 | `			if( needWrite ){` |
|       13 |  5633 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5634 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5635 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5636 | `					 * into the new map's storage. */` |
|        7 |  5637 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5638 | `					if( pIdx ){` |
|        7 |  5639 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5640 | `					}` |
|        3 |  5641 | `				}` |
|        6 |  5642 | `			}` |
|        9 |  5643 | `		}` |
|   115115 |  5644 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5645 | `			/* Create a new empty entry */` |
|      273 |  5646 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5647 | `			if( rc == SXRET_OK ){` |
|        - |  5648 | `				/* Point to the last inserted entry */` |
|      273 |  5649 | `				pNode = pMap->pLast;` |
|      136 |  5650 | `			}` |
|      136 |  5651 | `		}` |
|    57555 |  5652 | `	}` |
|   115115 |  5653 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5654 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5655 | `		char zMsg[128];` |
|      ! 0 |  5656 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5657 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5658 | `		}` |
|      ! 0 |  5659 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5660 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5661 | `	}` |
|   115115 |  5662 | `	if( pIdx ){` |
|   115115 |  5663 | `		PH7_MemObjRelease(pIdx);` |
|    57555 |  5664 | `	}` |
|   115115 |  5665 | `	if( rc == SXRET_OK ){` |
|        - |  5666 | `		/* Load entry contents */` |
|    50953 |  5667 | `		if( pMap->iRef < 2 ){` |
|        - |  5668 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5669 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5670 | `			 */` |
|       28 |  5671 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5672 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5673 | `		}else{` |
|    50927 |  5674 | `			pTos->nIdx = pNode->nValIdx;` |
|    50927 |  5675 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    50927 |  5676 | `			PH7_HashmapUnref(pMap);` |
|        - |  5677 | `		}` |
|    25479 |  5678 | `	}else{` |
|        - |  5679 | `		/* No such entry,load NULL */` |
|    64167 |  5680 | `		PH7_MemObjRelease(pTos);` |
|    64167 |  5681 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5682 | `	}` |
|   115115 |  5683 | `	break;` |
|        - |  5684 | `					  }` |
|        - |  5685 | `/*` |
|        - |  5686 | ` * LOAD_CLOSURE * * P3` |
|        - |  5687 | ` *` |
|        - |  5688 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5689 | ` * name in the stack.` |
|        - |  5690 | ` */` |
|       64 |  5691 | `case PH7_OP_LOAD_CLOSURE:{` |
|      130 |  5692 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      130 |  5693 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5694 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5695 | `		ph7_vm_func *pClosure;` |
|        - |  5696 | `		char *zName;` |
|        - |  5697 | `		sxu32 mLen;` |
|        - |  5698 | `		sxu32 n;` |
|        - |  5699 | `		/* Create a new VM function */` |
|      130 |  5700 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5701 | `		/* Generate an unique closure name */` |
|      130 |  5702 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|      130 |  5703 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5704 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5705 | `			goto Abort;` |
|        - |  5706 | `		}` |
|      130 |  5707 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      130 |  5708 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5709 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5710 | `		}` |
|        - |  5711 | `		/* Zero the stucture */` |
|      130 |  5712 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5713 | `		/* Perform a structure assignment on read-only items */` |
|      130 |  5714 | `		pClosure->aArgs = pFunc->aArgs;` |
|      130 |  5715 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|      130 |  5716 | `		pClosure->aStatic = pFunc->aStatic;` |
|      130 |  5717 | `		pClosure->iFlags = pFunc->iFlags;` |
|      130 |  5718 | `		pClosure->pUserData = pFunc->pUserData;` |
|      130 |  5719 | `		pClosure->sSignature = pFunc->sSignature;` |
|      130 |  5720 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|      130 |  5721 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|      130 |  5722 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|      130 |  5723 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|      130 |  5724 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5725 | `		/* Register the closure */` |
|      130 |  5726 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5727 | `		/* Set up closure environment */` |
|      130 |  5728 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|      130 |  5729 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      324 |  5730 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5731 | `			ph7_value *pValue;` |
|      196 |  5732 | `			pEnv = &aEnv[n];` |
|      196 |  5733 | `			sEnv.sName  = pEnv->sName;` |
|      196 |  5734 | `			sEnv.iFlags = pEnv->iFlags;` |
|      196 |  5735 | `			sEnv.nIdx = SXU32_HIGH;` |
|      196 |  5736 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      196 |  5737 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5738 | `				/* Pass by reference */` |
|      ! 0 |  5739 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5740 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5741 | `					);` |
|      ! 0 |  5742 | `			}` |
|        - |  5743 | `			/* Standard pass by value */` |
|      196 |  5744 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      196 |  5745 | `			if( pValue ){` |
|        - |  5746 | `				/* Copy imported value */` |
|       72 |  5747 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5748 | `			}` |
|        - |  5749 | `			/* Insert the imported variable */` |
|      196 |  5750 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       99 |  5751 | `		}` |
|        - |  5752 | `		/* Finally,load the closure name on the stack */` |
|      130 |  5753 | `		pTos++;` |
|      130 |  5754 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       64 |  5755 | `	}` |
|      130 |  5756 | `	break;` |
|        - |  5757 | `						 }` |
|        - |  5758 | `/*` |
|        - |  5759 | ` * STORE * P2 P3` |
|        - |  5760 | ` *` |
|        - |  5761 | ` * Perform a store (Assignment) operation.` |
|        - |  5762 | ` */` |
|   148078 |  5763 | `case PH7_OP_STORE: {` |
|        - |  5764 | `	ph7_value *pObj;` |
|        - |  5765 | `	SyString sName;` |
|        - |  5766 | `#ifdef UNTRUST` |
|        - |  5767 | `	if( pTos < pStack ){` |
|        - |  5768 | `		goto Abort;` |
|        - |  5769 | `	}` |
|        - |  5770 | `#endif` |
|   296161 |  5771 | `	if( pInstr->iP2 ){` |
|        - |  5772 | `		sxu32 nIdx;` |
|        - |  5773 | `		sxi32 rcT;` |
|        - |  5774 | `		/* Member store operation */` |
|     5639 |  5775 | `		nIdx = pTos->nIdx;` |
|     5639 |  5776 | `		VmPopOperand(&pTos,1);` |
|     5639 |  5777 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5778 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5779 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5780 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5781 | `		}else{` |
|        - |  5782 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5783 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     5635 |  5784 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     5635 |  5785 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5786 | `				goto Abort;` |
|        - |  5787 | `			}` |
|     5635 |  5788 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5789 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5790 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5791 | `				 * propagate out of the VM loop. */` |
|       43 |  5792 | `				VmPopOperand(&pTos,1);` |
|        - |  5793 | `				{` |
|       43 |  5794 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       43 |  5795 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       43 |  5796 | `						pc = pFrm2->iExceptionJump - 1;` |
|   148102 |  5797 | `						break;` |
|        - |  5798 | `					}` |
|        - |  5799 | `				}` |
|      ! 0 |  5800 | `				goto Exception;` |
|        - |  5801 | `			}` |
|        - |  5802 | `			/* Point to the desired memory object */` |
|     5597 |  5803 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5597 |  5804 | `			if( pObj ){` |
|        - |  5805 | `				/* Perform the store operation */` |
|     5597 |  5806 | `				PH7_MemObjStore(pTos,pObj);` |
|     2796 |  5807 | `			}` |
|        - |  5808 | `		}` |
|     5601 |  5809 | `		break;` |
|   290527 |  5810 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5811 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5812 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5813 | `			/* Force a string cast */` |
|      ! 0 |  5814 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5815 | `		}` |
|        7 |  5816 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5817 | `		pTos--;` |
|        - |  5818 | `#ifdef UNTRUST` |
|        - |  5819 | `		if( pTos < pStack  ){` |
|        - |  5820 | `			goto Abort;` |
|        - |  5821 | `		}` |
|        - |  5822 | `#endif` |
|        4 |  5823 | `	}else{` |
|   290521 |  5824 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5825 | `	}` |
|        - |  5826 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   290527 |  5827 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   290527 |  5828 | `	if( pObj == 0 ){` |
|      ! 0 |  5829 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5830 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5831 | `		goto Abort;` |
|        - |  5832 | `	}` |
|   290527 |  5833 | `	if( !pInstr->p3 ){` |
|        7 |  5834 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5835 | `	}` |
|        - |  5836 | `	/* Perform the store operation */` |
|   290527 |  5837 | `	PH7_MemObjStore(pTos,pObj);` |
|   290527 |  5838 | `	break;` |
|        - |  5839 | `				   }` |
|        - |  5840 | `/*` |
|        - |  5841 | ` * STORE_IDX:   P1 * P3` |
|        - |  5842 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5843 | ` *` |
|        - |  5844 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5845 | ` */` |
|    98156 |  5846 | `case PH7_OP_STORE_IDX:` |
|        - |  5847 | `case PH7_OP_STORE_IDX_REF: {` |
|   196317 |  5848 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5849 | `	ph7_value *pKey;` |
|        - |  5850 | `	sxu32 nIdx;` |
|   196317 |  5851 | `	if( pInstr->iP1 ){` |
|        - |  5852 | `		/* Key is next on stack */` |
|    63727 |  5853 | `		pKey = pTos;` |
|    63727 |  5854 | `		pTos--;` |
|    31866 |  5855 | `	}else{` |
|   132595 |  5856 | `		pKey = 0;` |
|        - |  5857 | `	}` |
|   196317 |  5858 | `	nIdx = pTos->nIdx;` |
|        - |  5859 | `	{` |
|        - |  5860 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5861 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5862 | `		 * the backing variable slot at nIdx. */` |
|   196317 |  5863 | `		ph7_class_instance *pInst = 0;` |
|   196317 |  5864 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       35 |  5865 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   196301 |  5866 | `		}else if( nIdx != SXU32_HIGH ){` |
|   196285 |  5867 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   196285 |  5868 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5869 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5870 | `			}` |
|    98140 |  5871 | `		}` |
|   196317 |  5872 | `		if( pInst ){` |
|       35 |  5873 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       35 |  5874 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5875 | `				ph7_class_method *pMeth;` |
|        - |  5876 | `				ph7_value sNullKey;` |
|        - |  5877 | `				ph7_value *apArg[2];` |
|       33 |  5878 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5879 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5880 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5881 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5882 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5883 | `					break;` |
|        - |  5884 | `				}` |
|       33 |  5885 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5886 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5887 | `				/* Pop container; pTos now points to the value */` |
|       33 |  5888 | `				VmPopOperand(&pTos,1);` |
|       33 |  5889 | `				if( pKey == 0 ){` |
|        7 |  5890 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5891 | `					apArg[0] = &sNullKey;` |
|        4 |  5892 | `				}else{` |
|       27 |  5893 | `					apArg[0] = pKey;` |
|        - |  5894 | `				}` |
|       33 |  5895 | `				apArg[1] = pTos;` |
|       33 |  5896 | `				if( pMeth ){` |
|       33 |  5897 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5898 | `				}` |
|       33 |  5899 | `				if( pKey ){` |
|       27 |  5900 | `					PH7_MemObjRelease(pKey);` |
|       15 |  5901 | `				}else{` |
|        7 |  5902 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5903 | `				}` |
|        - |  5904 | `				/* Pop the value */` |
|       33 |  5905 | `				VmPopOperand(&pTos,1);` |
|       33 |  5906 | `				break;` |
|        - |  5907 | `			}` |
|        - |  5908 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5909 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5910 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5911 | `			 * a few lines below). Match PHP. */` |
|        - |  5912 | `			{` |
|        - |  5913 | `				char zMsg[256];` |
|        3 |  5914 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5915 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5916 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5917 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5918 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5919 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5920 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5921 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5922 | `				break;` |
|        - |  5923 | `			}` |
|        - |  5924 | `		}` |
|        - |  5925 | `	}` |
|   196285 |  5926 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5927 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5928 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5929 | `		 * checking true sharing count, then re-add after separation. */` |
|   196233 |  5930 | `		if( nIdx != SXU32_HIGH ){` |
|   196233 |  5931 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   294347 |  5932 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   196233 |  5933 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5934 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5935 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5936 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5937 | `				 * refcounts if the backing array was already separated. */` |
|   196233 |  5938 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   196233 |  5939 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   196233 |  5940 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   196233 |  5941 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   196233 |  5942 | `					pTos->x.pOther = pMap;` |
|    98119 |  5943 | `				}else{` |
|        - |  5944 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5945 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5946 | `					pMap = pCur;` |
|        - |  5947 | `				}` |
|    98119 |  5948 | `			}else{` |
|      ! 0 |  5949 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5950 | `			}` |
|    98119 |  5951 | `		}else{` |
|      ! 0 |  5952 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5953 | `		}` |
|   196233 |  5954 | `		if( pMap->iRef < 2 ){` |
|        - |  5955 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5956 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5957 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5958 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5959 | `			pMap->iRef = 2;` |
|      ! 0 |  5960 | `		}` |
|    98119 |  5961 | `	}else{` |
|        - |  5962 | `		ph7_value *pObj;` |
|       53 |  5963 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5964 | `		if( pObj == 0 ){` |
|      ! 0 |  5965 | `			if( pKey ){` |
|      ! 0 |  5966 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5967 | `			}` |
|      ! 0 |  5968 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5969 | `			break;` |
|        - |  5970 | `		}` |
|        - |  5971 | `		/* Phase#1: Load the array */` |
|       53 |  5972 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5973 | `			VmPopOperand(&pTos,1);` |
|       53 |  5974 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5975 | `				/* Force a string cast */` |
|      ! 0 |  5976 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5977 | `			}` |
|       53 |  5978 | `			if( pKey == 0 ){` |
|        - |  5979 | `				/* Append string */` |
|        3 |  5980 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5981 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5982 | `				}` |
|        2 |  5983 | `			}else{` |
|        - |  5984 | `				sxu32 nOfft;` |
|       51 |  5985 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5986 | `					/* Force an int cast */` |
|       51 |  5987 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5988 | `				}` |
|       51 |  5989 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5990 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5991 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5992 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5993 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5994 | `				}else{` |
|      ! 0 |  5995 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5996 | `						/* Perform an append operation */` |
|      ! 0 |  5997 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5998 | `					}` |
|        - |  5999 | `				}` |
|        - |  6000 | `			}` |
|       53 |  6001 | `			if( pKey ){` |
|       51 |  6002 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  6003 | `			}` |
|       53 |  6004 | `			break;` |
|      ! 0 |  6005 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  6006 | `			/* Force a hashmap cast  */` |
|      ! 0 |  6007 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  6008 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6009 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  6010 | `				goto Abort;` |
|        - |  6011 | `			}` |
|      ! 0 |  6012 | `		}` |
|        - |  6013 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  6014 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  6015 | `	}` |
|   196233 |  6016 | `	VmPopOperand(&pTos,1);` |
|        - |  6017 | `	/* Phase#2: Perform the insertion */` |
|   196233 |  6018 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  6019 | `		/* Insertion by reference */` |
|       15 |  6020 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  6021 | `	}else{` |
|   196219 |  6022 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  6023 | `	}` |
|   196233 |  6024 | `	if( pKey ){` |
|    63651 |  6025 | `		PH7_MemObjRelease(pKey);` |
|    31823 |  6026 | `	}` |
|   196233 |  6027 | `	break;` |
|        - |  6028 | `					   }` |
|        - |  6029 | `/*` |
|        - |  6030 | ` * INCR: P1 * *` |
|        - |  6031 | ` *` |
|        - |  6032 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  6033 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  6034 | ` * the stack and increment after that.` |
|        - |  6035 | ` */` |
|   167994 |  6036 | `case PH7_OP_INCR:` |
|        - |  6037 | `#ifdef UNTRUST` |
|        - |  6038 | `	if( pTos < pStack ){` |
|        - |  6039 | `		goto Abort;` |
|        - |  6040 | `	}` |
|        - |  6041 | `#endif` |
|   336037 |  6042 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   336037 |  6043 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6044 | `			ph7_value *pObj;` |
|   336037 |  6045 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   336037 |  6046 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6047 | `					/* Perl-style string increment.` |
|        - |  6048 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  6049 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  6050 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  6051 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  6052 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  6053 | `					}` |
|       49 |  6054 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  6055 | `					if( pInstr->iP1 ){` |
|        - |  6056 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  6057 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  6058 | `					}` |
|       25 |  6059 | `				}else{` |
|        - |  6060 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  6061 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  6062 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  6063 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  6064 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  6065 | `					 * so its old-value view survives the coercion. */` |
|   335989 |  6066 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  6067 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  6068 | `					}` |
|        - |  6069 | `					/* Force a numeric cast on the variable */` |
|   335989 |  6070 | `					PH7_MemObjToNumeric(pObj);` |
|   335989 |  6071 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6072 | `						pObj->rVal++;` |
|        - |  6073 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6074 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6075 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6076 | `						 * integer-valued real. */` |
|        9 |  6077 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6078 | `					}else{` |
|   335981 |  6079 | `						pObj->x.iVal++;` |
|        - |  6080 | `					}` |
|   335989 |  6081 | `					if( pInstr->iP1 ){` |
|        - |  6082 | `						/* Pre-increment: result is the new value. */` |
|       83 |  6083 | `						PH7_MemObjStore(pObj,pTos);` |
|       41 |  6084 | `					}` |
|        - |  6085 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  6086 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  6087 | `				}` |
|   168038 |  6088 | `			}` |
|   168043 |  6089 | `		}else{` |
|      ! 0 |  6090 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6091 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  6092 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  6093 | `				}else{` |
|        - |  6094 | `					/* Force a numeric cast */` |
|      ! 0 |  6095 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6096 | `					/* Pre-increment */` |
|      ! 0 |  6097 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6098 | `						pTos->rVal++;` |
|        - |  6099 | `						/* Try to get an integer representation */` |
|      ! 0 |  6100 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6101 | `					}else{` |
|      ! 0 |  6102 | `						pTos->x.iVal++;` |
|      ! 0 |  6103 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6104 | `					}` |
|        - |  6105 | `				}` |
|      ! 0 |  6106 | `			}` |
|        - |  6107 | `		}` |
|   168038 |  6108 | `	}` |
|   336037 |  6109 | `	break;` |
|        - |  6110 | `/*` |
|        - |  6111 | ` * DECR: P1 * *` |
|        - |  6112 | ` *` |
|        - |  6113 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  6114 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  6115 | ` * and decrement after that.` |
|        - |  6116 | ` */` |
|       14 |  6117 | `case PH7_OP_DECR:` |
|        - |  6118 | `#ifdef UNTRUST` |
|        - |  6119 | `	if( pTos < pStack ){` |
|        - |  6120 | `		goto Abort;` |
|        - |  6121 | `	}` |
|        - |  6122 | `#endif` |
|        - |  6123 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  6124 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  6125 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  6126 | `			ph7_value *pObj;` |
|       27 |  6127 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  6128 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  6129 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  6130 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  6131 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  6132 | `					if( pInstr->iP1 ){` |
|        - |  6133 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  6134 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  6135 | `					}` |
|        - |  6136 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  6137 | `				}else{` |
|        - |  6138 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  6139 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  6140 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  6141 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  6142 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  6143 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  6144 | `					}` |
|       21 |  6145 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  6146 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  6147 | `						pObj->rVal--;` |
|        - |  6148 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  6149 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  6150 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  6151 | `						 * integer-valued real. */` |
|        9 |  6152 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  6153 | `					}else{` |
|       13 |  6154 | `						pObj->x.iVal--;` |
|        - |  6155 | `					}` |
|       21 |  6156 | `					if( pInstr->iP1 ){` |
|        - |  6157 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  6158 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  6159 | `					}` |
|        - |  6160 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  6161 | `				}` |
|       13 |  6162 | `			}` |
|       14 |  6163 | `		}else{` |
|      ! 0 |  6164 | `			if( pInstr->iP1 ){` |
|      ! 0 |  6165 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  6166 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  6167 | `				}else{` |
|        - |  6168 | `					/* Force a numeric cast */` |
|      ! 0 |  6169 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  6170 | `					/* Pre-decrement */` |
|      ! 0 |  6171 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6172 | `						pTos->rVal--;` |
|        - |  6173 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  6174 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  6175 | `					}else{` |
|      ! 0 |  6176 | `						pTos->x.iVal--;` |
|      ! 0 |  6177 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  6178 | `					}` |
|        - |  6179 | `				}` |
|      ! 0 |  6180 | `			}` |
|        - |  6181 | `		}` |
|       13 |  6182 | `	}` |
|       29 |  6183 | `	break;` |
|        - |  6184 | `/*` |
|        - |  6185 | ` * UMINUS: * * *` |
|        - |  6186 | ` *` |
|        - |  6187 | ` * Perform a unary minus operation.` |
|        - |  6188 | ` */` |
|    30119 |  6189 | `case PH7_OP_UMINUS:` |
|        - |  6190 | `#ifdef UNTRUST` |
|        - |  6191 | `	if( pTos < pStack ){` |
|        - |  6192 | `		goto Abort;` |
|        - |  6193 | `	}` |
|        - |  6194 | `#endif` |
|        - |  6195 | `	/* Force a numeric (integer,real or both) cast */` |
|    60243 |  6196 | `	PH7_MemObjToNumeric(pTos);` |
|    60243 |  6197 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  6198 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  6199 | `	}` |
|    60243 |  6200 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    60213 |  6201 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    30104 |  6202 | `	}` |
|    60243 |  6203 | `	break;` |
|        - |  6204 | `/*` |
|        - |  6205 | ` * UPLUS: * * *` |
|        - |  6206 | ` *` |
|        - |  6207 | ` * Perform a unary plus operation.` |
|        - |  6208 | ` */` |
|       18 |  6209 | `case PH7_OP_UPLUS:` |
|        - |  6210 | `#ifdef UNTRUST` |
|        - |  6211 | `	if( pTos < pStack ){` |
|        - |  6212 | `		goto Abort;` |
|        - |  6213 | `	}` |
|        - |  6214 | `#endif` |
|        - |  6215 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  6216 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  6217 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  6218 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  6219 | `	}` |
|       37 |  6220 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  6221 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  6222 | `	}` |
|       37 |  6223 | `	break;` |
|        - |  6224 | `/*` |
|        - |  6225 | ` * OP_LNOT: * * *` |
|        - |  6226 | ` *` |
|        - |  6227 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  6228 | ` * with its complement.` |
|        - |  6229 | ` */` |
|    45116 |  6230 | `case PH7_OP_LNOT:` |
|        - |  6231 | `#ifdef UNTRUST` |
|        - |  6232 | `	if( pTos < pStack ){` |
|        - |  6233 | `		goto Abort;` |
|        - |  6234 | `	}` |
|        - |  6235 | `#endif` |
|        - |  6236 | `	/* Force a boolean cast */` |
|    90281 |  6237 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       27 |  6238 | `		PH7_MemObjToBool(pTos);` |
|       11 |  6239 | `	}` |
|    90281 |  6240 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    90281 |  6241 | `	break;` |
|        - |  6242 | `/*` |
|        - |  6243 | ` * OP_BITNOT: * * *` |
|        - |  6244 | ` *` |
|        - |  6245 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  6246 | ` * with its ones-complement.` |
|        - |  6247 | ` */` |
|       14 |  6248 | `case PH7_OP_BITNOT:` |
|        - |  6249 | `#ifdef UNTRUST` |
|        - |  6250 | `	if( pTos < pStack ){` |
|        - |  6251 | `		goto Abort;` |
|        - |  6252 | `	}` |
|        - |  6253 | `#endif` |
|        - |  6254 | `	/* Force an integer cast */` |
|       33 |  6255 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6256 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6257 | `	}` |
|       33 |  6258 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       33 |  6259 | `	break;` |
|        - |  6260 | `/* OP_MUL * * *` |
|        - |  6261 | ` * OP_MUL_STORE * * *` |
|        - |  6262 | ` *` |
|        - |  6263 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  6264 | ` * and push the result back onto the stack.` |
|        - |  6265 | ` */` |
|     1296 |  6266 | `case PH7_OP_MUL:` |
|        - |  6267 | `case PH7_OP_MUL_STORE: {` |
|     2596 |  6268 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6269 | `	/* Force the operand to be numeric */` |
|        - |  6270 | `#ifdef UNTRUST` |
|        - |  6271 | `	if( pNos < pStack ){` |
|        - |  6272 | `		goto Abort;` |
|        - |  6273 | `	}` |
|        - |  6274 | `#endif` |
|     2596 |  6275 | `	PH7_MemObjToNumeric(pTos);` |
|     2596 |  6276 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6277 | `	/* Perform the requested operation */` |
|     2596 |  6278 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6279 | `		/* Floating point arithemic */` |
|        - |  6280 | `		ph7_real a,b,r;` |
|       21 |  6281 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  6282 | `			PH7_MemObjToReal(pTos);` |
|        4 |  6283 | `		}` |
|       21 |  6284 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  6285 | `			PH7_MemObjToReal(pNos);` |
|        3 |  6286 | `		}` |
|       21 |  6287 | `		a = pNos->rVal;` |
|       21 |  6288 | `		b = pTos->rVal;` |
|       21 |  6289 | `		r = a * b;` |
|        - |  6290 | `		/* Push the result */` |
|       21 |  6291 | `		pNos->rVal = r;` |
|       21 |  6292 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6293 | `		/* Try to get an integer representation */` |
|       21 |  6294 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  6295 | `	}else{` |
|        - |  6296 | `		/* Integer arithmetic */` |
|        - |  6297 | `		sxi64 a,b,r;` |
|     2576 |  6298 | `		a = pNos->x.iVal;` |
|     2576 |  6299 | `		b = pTos->x.iVal;` |
|     2576 |  6300 | `		r = a * b;` |
|        - |  6301 | `		/* Push the result */` |
|     2576 |  6302 | `		pNos->x.iVal = r;` |
|     2576 |  6303 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6304 | `	}` |
|     2596 |  6305 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  6306 | `		ph7_value *pObj;` |
|       32 |  6307 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6308 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  6309 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  6310 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  6311 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  6312 | `		}` |
|       15 |  6313 | `	}` |
|     2596 |  6314 | `	VmPopOperand(&pTos,1);` |
|     2596 |  6315 | `	break;` |
|        - |  6316 | `				 }` |
|        - |  6317 | `/* OP_POW * * *` |
|        - |  6318 | ` * OP_POW_STORE * * *` |
|        - |  6319 | ` *` |
|        - |  6320 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  6321 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  6322 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  6323 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  6324 | ` */` |
|       67 |  6325 | `case PH7_OP_POW:` |
|        - |  6326 | `case PH7_OP_POW_STORE: {` |
|      135 |  6327 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  6328 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  6329 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  6330 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  6331 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  6332 | `	 */` |
|      135 |  6333 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  6334 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  6335 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  6336 | `	int bBothInt;` |
|      135 |  6337 | `	int usedInt = 0;` |
|        - |  6338 | `	ph7_real a, b, r;` |
|        - |  6339 | `#endif` |
|      135 |  6340 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  6341 | `#ifdef UNTRUST` |
|        - |  6342 | `	if( pNos < pStack ){` |
|        - |  6343 | `		goto Abort;` |
|        - |  6344 | `	}` |
|        - |  6345 | `#endif` |
|      135 |  6346 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  6347 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  6348 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  6349 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  6350 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  6351 | `	if( bBothInt ){` |
|      123 |  6352 | `		base_i = pBase->x.iVal;` |
|      123 |  6353 | `		exp_i  = pExp->x.iVal;` |
|       61 |  6354 | `	}` |
|      135 |  6355 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  6356 | `		PH7_MemObjToReal(pBase);` |
|       62 |  6357 | `	}` |
|      135 |  6358 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  6359 | `		PH7_MemObjToReal(pExp);` |
|       66 |  6360 | `	}` |
|      135 |  6361 | `	a = pBase->rVal;` |
|      135 |  6362 | `	b = pExp->rVal;` |
|      135 |  6363 | `	r = pow(a, b);` |
|        - |  6364 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  6365 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  6366 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  6367 | `	 * representable as double but not as signed int64. */` |
|      135 |  6368 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  6369 | `		sxi64 result_i = 1;` |
|      117 |  6370 | `		sxi64 cur_base = base_i;` |
|      117 |  6371 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  6372 | `		int overflow = 0;` |
|      401 |  6373 | `		while( cur_exp > 0 ){` |
|      289 |  6374 | `			if( cur_exp & 1 ){` |
|      189 |  6375 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  6376 | `					overflow = 1;` |
|        3 |  6377 | `					break;` |
|        - |  6378 | `				}` |
|       93 |  6379 | `			}` |
|      287 |  6380 | `			cur_exp >>= 1;` |
|      287 |  6381 | `			if( cur_exp > 0 ){` |
|      181 |  6382 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  6383 | `					overflow = 1;` |
|        3 |  6384 | `					break;` |
|        - |  6385 | `				}` |
|       89 |  6386 | `			}` |
|        1 |  6387 | `		}` |
|      117 |  6388 | `		if( !overflow ){` |
|      113 |  6389 | `			pNos->x.iVal = result_i;` |
|      113 |  6390 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  6391 | `			usedInt = 1;` |
|       56 |  6392 | `		}` |
|       58 |  6393 | `	}` |
|      135 |  6394 | `	if( !usedInt ){` |
|       23 |  6395 | `		pNos->rVal = r;` |
|       23 |  6396 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  6397 | `	}` |
|        - |  6398 | `#else` |
|        - |  6399 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  6400 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  6401 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  6402 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  6403 | `	 * represented. */` |
|        - |  6404 | `	base_i = pBase->x.iVal;` |
|        - |  6405 | `	exp_i  = pExp->x.iVal;` |
|        - |  6406 | `	{` |
|        - |  6407 | `		sxi64 result_i = 1;` |
|        - |  6408 | `		sxi64 cur_base = base_i;` |
|        - |  6409 | `		sxi64 cur_exp  = exp_i;` |
|        - |  6410 | `		if( cur_exp < 0 ){` |
|        - |  6411 | `			result_i = 0;` |
|        - |  6412 | `		}else{` |
|        - |  6413 | `			while( cur_exp > 0 ){` |
|        - |  6414 | `				if( cur_exp & 1 ){` |
|        - |  6415 | `					result_i *= cur_base;` |
|        - |  6416 | `				}` |
|        - |  6417 | `				cur_exp >>= 1;` |
|        - |  6418 | `				if( cur_exp > 0 ){` |
|        - |  6419 | `					cur_base *= cur_base;` |
|        - |  6420 | `				}` |
|        - |  6421 | `			}` |
|        - |  6422 | `		}` |
|        - |  6423 | `		pNos->x.iVal = result_i;` |
|        - |  6424 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  6425 | `	}` |
|        - |  6426 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  6427 | `	if( bStore ){` |
|        - |  6428 | `		ph7_value *pObj;` |
|       23 |  6429 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6430 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  6431 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  6432 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  6433 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  6434 | `		}` |
|       11 |  6435 | `	}` |
|      135 |  6436 | `	VmPopOperand(&pTos,1);` |
|      135 |  6437 | `	break;` |
|        - |  6438 | `				 }` |
|        - |  6439 | `/* OP_ADD * * *` |
|        - |  6440 | ` *` |
|        - |  6441 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6442 | ` * and push the result back onto the stack.` |
|        - |  6443 | ` */` |
|      537 |  6444 | `case PH7_OP_ADD:{` |
|     1079 |  6445 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6446 | `#ifdef UNTRUST` |
|        - |  6447 | `	if( pNos < pStack ){` |
|        - |  6448 | `		goto Abort;` |
|        - |  6449 | `	}` |
|        - |  6450 | `#endif` |
|        - |  6451 | `	/* Perform the addition */` |
|     1079 |  6452 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1079 |  6453 | `	VmPopOperand(&pTos,1);` |
|     1079 |  6454 | `	break;` |
|        - |  6455 | `				}` |
|        - |  6456 | `/*` |
|        - |  6457 | ` * OP_ADD_STORE * * *` |
|        - |  6458 | ` *` |
|        - |  6459 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  6460 | ` * and push the result back onto the stack.` |
|        - |  6461 | ` */` |
|      502 |  6462 | `case PH7_OP_ADD_STORE:{` |
|     1008 |  6463 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6464 | `	ph7_value *pObj;` |
|        - |  6465 | `	sxu32 nIdx;` |
|        - |  6466 | `#ifdef UNTRUST` |
|        - |  6467 | `	if( pNos < pStack ){` |
|        - |  6468 | `		goto Abort;` |
|        - |  6469 | `	}` |
|        - |  6470 | `#endif` |
|        - |  6471 | `	/* Perform the addition */` |
|     1008 |  6472 | `	nIdx = pTos->nIdx;` |
|     1008 |  6473 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  6474 | `	/* Peform the store operation */` |
|     1008 |  6475 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6476 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1008 |  6477 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1008 |  6478 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1008 |  6479 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  6480 | `	}` |
|        - |  6481 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1008 |  6482 | `	PH7_MemObjStore(pTos,pNos);` |
|     1008 |  6483 | `	VmPopOperand(&pTos,1);` |
|     1008 |  6484 | `	break;` |
|        - |  6485 | `				}` |
|        - |  6486 | `/* OP_SUB * * *` |
|        - |  6487 | ` *` |
|        - |  6488 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6489 | ` * first (what was next on the stack) from the second (the` |
|        - |  6490 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6491 | ` */` |
|      352 |  6492 | `case PH7_OP_SUB: {` |
|      709 |  6493 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6494 | `#ifdef UNTRUST` |
|        - |  6495 | `	if( pNos < pStack ){` |
|        - |  6496 | `		goto Abort;` |
|        - |  6497 | `	}` |
|        - |  6498 | `#endif` |
|      709 |  6499 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6500 | `		/* Floating point arithemic */` |
|        - |  6501 | `		ph7_real a,b,r;` |
|      103 |  6502 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6503 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6504 | `		}` |
|      103 |  6505 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6506 | `			PH7_MemObjToReal(pNos);` |
|        2 |  6507 | `		}` |
|      103 |  6508 | `		a = pNos->rVal;` |
|      103 |  6509 | `		b = pTos->rVal;` |
|      103 |  6510 | `		r = a - b;` |
|        - |  6511 | `		/* Push the result */` |
|      103 |  6512 | `		pNos->rVal = r;` |
|      103 |  6513 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6514 | `		/* Try to get an integer representation */` |
|      103 |  6515 | `		PH7_MemObjTryInteger(pNos);` |
|       52 |  6516 | `	}else{` |
|        - |  6517 | `		/* Integer arithmetic */` |
|        - |  6518 | `		sxi64 a,b,r;` |
|      607 |  6519 | `		a = pNos->x.iVal;` |
|      607 |  6520 | `		b = pTos->x.iVal;` |
|      607 |  6521 | `		r = a - b;` |
|        - |  6522 | `		/* Push the result */` |
|      607 |  6523 | `		pNos->x.iVal = r;` |
|      607 |  6524 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6525 | `	}` |
|      709 |  6526 | `	VmPopOperand(&pTos,1);` |
|      709 |  6527 | `	break;` |
|        - |  6528 | `				 }` |
|        - |  6529 | `/* OP_SUB_STORE * * *` |
|        - |  6530 | ` *` |
|        - |  6531 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  6532 | ` * first (what was next on the stack) from the second (the` |
|        - |  6533 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  6534 | ` */` |
|        4 |  6535 | `case PH7_OP_SUB_STORE: {` |
|       10 |  6536 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6537 | `	ph7_value *pObj;` |
|        - |  6538 | `#ifdef UNTRUST` |
|        - |  6539 | `	if( pNos < pStack ){` |
|        - |  6540 | `		goto Abort;` |
|        - |  6541 | `	}` |
|        - |  6542 | `#endif` |
|       10 |  6543 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  6544 | `		/* Floating point arithemic */` |
|        - |  6545 | `		ph7_real a,b,r;` |
|      ! 0 |  6546 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6547 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  6548 | `		}` |
|      ! 0 |  6549 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  6550 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  6551 | `		}` |
|      ! 0 |  6552 | `		a = pTos->rVal;` |
|      ! 0 |  6553 | `		b = pNos->rVal;` |
|      ! 0 |  6554 | `		r = a - b;` |
|        - |  6555 | `		/* Push the result */` |
|      ! 0 |  6556 | `		pNos->rVal = r;` |
|      ! 0 |  6557 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6558 | `		/* Try to get an integer representation */` |
|      ! 0 |  6559 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  6560 | `	}else{` |
|        - |  6561 | `		/* Integer arithmetic */` |
|        - |  6562 | `		sxi64 a,b,r;` |
|       10 |  6563 | `		a = pTos->x.iVal;` |
|       10 |  6564 | `		b = pNos->x.iVal;` |
|       10 |  6565 | `		r = a - b;` |
|        - |  6566 | `		/* Push the result */` |
|       10 |  6567 | `		pNos->x.iVal = r;` |
|       10 |  6568 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  6569 | `	}` |
|       10 |  6570 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6571 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  6572 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  6573 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  6574 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  6575 | `	}` |
|       10 |  6576 | `	VmPopOperand(&pTos,1);` |
|       10 |  6577 | `	break;` |
|        - |  6578 | `				 }` |
|        - |  6579 |  |
|        - |  6580 | `/*` |
|        - |  6581 | ` * OP_MOD * * *` |
|        - |  6582 | ` *` |
|        - |  6583 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6584 | ` * first (what was next on the stack) from the second (the` |
|        - |  6585 | ` * top of the stack) and push the remainder after division` |
|        - |  6586 | ` * onto the stack.` |
|        - |  6587 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6588 | ` */` |
|      309 |  6589 | `case PH7_OP_MOD:{` |
|      623 |  6590 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6591 | `	sxi64 a,b,r;` |
|        - |  6592 | `#ifdef UNTRUST` |
|        - |  6593 | `	if( pNos < pStack ){` |
|        - |  6594 | `		goto Abort;` |
|        - |  6595 | `	}` |
|        - |  6596 | `#endif` |
|        - |  6597 | `	/* Force the operands to be integer */` |
|      623 |  6598 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6599 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6600 | `	}` |
|      623 |  6601 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  6602 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  6603 | `	}` |
|        - |  6604 | `	/* Perform the requested operation */` |
|      623 |  6605 | `	a = pNos->x.iVal;` |
|      623 |  6606 | `	b = pTos->x.iVal;` |
|      623 |  6607 | `	if( b == 0 ){` |
|        3 |  6608 | `		r = 0;` |
|        3 |  6609 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6610 | `		/* goto Abort; */` |
|        2 |  6611 | `	}else{` |
|      621 |  6612 | `		r = a%b;` |
|        - |  6613 | `	}` |
|        - |  6614 | `	/* Push the result */` |
|      623 |  6615 | `	pNos->x.iVal = r;` |
|      623 |  6616 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      623 |  6617 | `	VmPopOperand(&pTos,1);` |
|      623 |  6618 | `	break;` |
|        - |  6619 | `				}` |
|        - |  6620 | `/*` |
|        - |  6621 | ` * OP_MOD_STORE * * *` |
|        - |  6622 | ` *` |
|        - |  6623 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6624 | ` * first (what was next on the stack) from the second (the` |
|        - |  6625 | ` * top of the stack) and push the remainder after division` |
|        - |  6626 | ` * onto the stack.` |
|        - |  6627 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6628 | ` */` |
|        1 |  6629 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6630 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6631 | `	ph7_value *pObj;` |
|        - |  6632 | `	sxi64 a,b,r;` |
|        - |  6633 | `#ifdef UNTRUST` |
|        - |  6634 | `	if( pNos < pStack ){` |
|        - |  6635 | `		goto Abort;` |
|        - |  6636 | `	}` |
|        - |  6637 | `#endif` |
|        - |  6638 | `	/* Force the operands to be integer */` |
|        3 |  6639 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6640 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6641 | `	}` |
|        3 |  6642 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6643 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6644 | `	}` |
|        - |  6645 | `	/* Perform the requested operation */` |
|        3 |  6646 | `	a = pTos->x.iVal;` |
|        3 |  6647 | `	b = pNos->x.iVal;` |
|        3 |  6648 | `	if( b == 0 ){` |
|      ! 0 |  6649 | `		r = 0;` |
|      ! 0 |  6650 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6651 | `		/* goto Abort; */` |
|      ! 0 |  6652 | `	}else{` |
|        3 |  6653 | `		r = a%b;` |
|        - |  6654 | `	}` |
|        - |  6655 | `	/* Push the result */` |
|        3 |  6656 | `	pNos->x.iVal = r;` |
|        3 |  6657 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6658 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6659 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6660 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6661 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6662 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6663 | `	}` |
|        3 |  6664 | `	VmPopOperand(&pTos,1);` |
|        3 |  6665 | `	break;` |
|        - |  6666 | `				}` |
|        - |  6667 | `/*` |
|        - |  6668 | ` * OP_DIV * * *` |
|        - |  6669 | ` *` |
|        - |  6670 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6671 | ` * first (what was next on the stack) from the second (the` |
|        - |  6672 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6673 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6674 | ` */` |
|       33 |  6675 | `case PH7_OP_DIV:{` |
|       68 |  6676 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6677 | `	ph7_real a,b,r;` |
|        - |  6678 | `#ifdef UNTRUST` |
|        - |  6679 | `	if( pNos < pStack ){` |
|        - |  6680 | `		goto Abort;` |
|        - |  6681 | `	}` |
|        - |  6682 | `#endif` |
|        - |  6683 | `	/* Force the operands to be real */` |
|       68 |  6684 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6685 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6686 | `	}` |
|       68 |  6687 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6688 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6689 | `	}` |
|        - |  6690 | `	/* Perform the requested operation */` |
|       68 |  6691 | `	a = pNos->rVal;` |
|       68 |  6692 | `	b = pTos->rVal;` |
|       68 |  6693 | `	if( b == 0 ){` |
|        - |  6694 | `		/* Division by zero */` |
|        3 |  6695 | `		pNos->rVal = 0;` |
|        3 |  6696 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6697 | `		/* goto Abort; */` |
|        2 |  6698 | `	}else{` |
|       65 |  6699 | `		r = a/b;` |
|        - |  6700 | `		/* Push the result */` |
|       65 |  6701 | `		pNos->rVal = r;` |
|       65 |  6702 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6703 | `		/* Try to get an integer representation */` |
|       65 |  6704 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6705 | `	}` |
|       68 |  6706 | `	VmPopOperand(&pTos,1);` |
|       68 |  6707 | `	break;` |
|        - |  6708 | `				}` |
|        - |  6709 | `/*` |
|        - |  6710 | ` * OP_DIV_STORE * * *` |
|        - |  6711 | ` *` |
|        - |  6712 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6713 | ` * first (what was next on the stack) from the second (the` |
|        - |  6714 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6715 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6716 | ` */` |
|        2 |  6717 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6718 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6719 | `	ph7_value *pObj;` |
|        - |  6720 | `	ph7_real a,b,r;` |
|        - |  6721 | `#ifdef UNTRUST` |
|        - |  6722 | `	if( pNos < pStack ){` |
|        - |  6723 | `		goto Abort;` |
|        - |  6724 | `	}` |
|        - |  6725 | `#endif` |
|        - |  6726 | `	/* Force the operands to be real */` |
|        5 |  6727 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6728 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6729 | `	}` |
|        5 |  6730 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6731 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6732 | `	}` |
|        - |  6733 | `	/* Perform the requested operation */` |
|        5 |  6734 | `	a = pTos->rVal;` |
|        5 |  6735 | `	b = pNos->rVal;` |
|        5 |  6736 | `	if( b == 0 ){` |
|        - |  6737 | `		/* Division by zero */` |
|      ! 0 |  6738 | `		r = 0;` |
|      ! 0 |  6739 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6740 | `		/* goto Abort; */` |
|      ! 0 |  6741 | `	}else{` |
|        5 |  6742 | `		r = a/b;` |
|        - |  6743 | `		/* Push the result */` |
|        5 |  6744 | `		pNos->rVal = r;` |
|        5 |  6745 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6746 | `		/* Try to get an integer representation */` |
|        5 |  6747 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6748 | `	}` |
|        5 |  6749 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6750 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6751 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6752 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6753 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6754 | `	}` |
|        5 |  6755 | `	VmPopOperand(&pTos,1);` |
|        5 |  6756 | `	break;` |
|        - |  6757 | `				}` |
|        - |  6758 | `/* OP_BAND * * *` |
|        - |  6759 | ` *` |
|        - |  6760 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6761 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6762 | ` * two elements.` |
|        - |  6763 | `*/` |
|        - |  6764 | `/* OP_BOR * * *` |
|        - |  6765 | ` *` |
|        - |  6766 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6767 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6768 | ` * two elements.` |
|        - |  6769 | ` */` |
|        - |  6770 | `/* OP_BXOR * * *` |
|        - |  6771 | ` *` |
|        - |  6772 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6773 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6774 | ` * two elements.` |
|        - |  6775 | ` */` |
|       43 |  6776 | `case PH7_OP_BAND:` |
|        - |  6777 | `case PH7_OP_BOR:` |
|        - |  6778 | `case PH7_OP_BXOR:{` |
|       91 |  6779 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6780 | `	sxi64 a,b,r;` |
|        - |  6781 | `#ifdef UNTRUST` |
|        - |  6782 | `	if( pNos < pStack ){` |
|        - |  6783 | `		goto Abort;` |
|        - |  6784 | `	}` |
|        - |  6785 | `#endif` |
|        - |  6786 | `	/* Force the operands to be integer */` |
|       91 |  6787 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6788 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6789 | `	}` |
|       91 |  6790 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6791 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6792 | `	}` |
|        - |  6793 | `	/* Perform the requested operation */` |
|       91 |  6794 | `	a = pNos->x.iVal;` |
|       91 |  6795 | `	b = pTos->x.iVal;` |
|       91 |  6796 | `	switch(pInstr->iOp){` |
|        7 |  6797 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6798 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6799 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6800 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       29 |  6801 | `	case PH7_OP_BAND_STORE:` |
|       29 |  6802 | `	case PH7_OP_BAND:` |
|       63 |  6803 | `	default:          r = a&b; break;` |
|        - |  6804 | `	}` |
|        - |  6805 | `	/* Push the result */` |
|       91 |  6806 | `	pNos->x.iVal = r;` |
|       91 |  6807 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       91 |  6808 | `	VmPopOperand(&pTos,1);` |
|       91 |  6809 | `	break;` |
|        - |  6810 | `				 }` |
|        - |  6811 | `/* OP_BAND_STORE * * *` |
|        - |  6812 | ` *` |
|        - |  6813 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6814 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6815 | ` * two elements.` |
|        - |  6816 | `*/` |
|        - |  6817 | `/* OP_BOR_STORE * * *` |
|        - |  6818 | ` *` |
|        - |  6819 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6820 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6821 | ` * two elements.` |
|        - |  6822 | ` */` |
|        - |  6823 | `/* OP_BXOR_STORE * * *` |
|        - |  6824 | ` *` |
|        - |  6825 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6826 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6827 | ` * two elements.` |
|        - |  6828 | ` */` |
|       10 |  6829 | `case PH7_OP_BAND_STORE:` |
|        - |  6830 | `case PH7_OP_BOR_STORE:` |
|        - |  6831 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6832 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6833 | `	ph7_value *pObj;` |
|        - |  6834 | `	sxi64 a,b,r;` |
|        - |  6835 | `#ifdef UNTRUST` |
|        - |  6836 | `	if( pNos < pStack ){` |
|        - |  6837 | `		goto Abort;` |
|        - |  6838 | `	}` |
|        - |  6839 | `#endif` |
|        - |  6840 | `	/* Force the operands to be integer */` |
|       21 |  6841 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6842 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6843 | `	}` |
|       21 |  6844 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6845 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6846 | `	}` |
|        - |  6847 | `	/* Perform the requested operation */` |
|       21 |  6848 | `	a = pTos->x.iVal;` |
|       21 |  6849 | `	b = pNos->x.iVal;` |
|       21 |  6850 | `	switch(pInstr->iOp){` |
|        3 |  6851 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6852 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6853 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6854 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6855 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6856 | `	case PH7_OP_BAND:` |
|        7 |  6857 | `	default:          r = a&b; break;` |
|        - |  6858 | `	}` |
|        - |  6859 | `	/* Push the result */` |
|       21 |  6860 | `	pNos->x.iVal = r;` |
|       21 |  6861 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6862 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6863 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6864 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6865 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6866 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6867 | `	}` |
|       21 |  6868 | `	VmPopOperand(&pTos,1);` |
|       21 |  6869 | `	break;` |
|        - |  6870 | `				 }` |
|        - |  6871 | `/* OP_SHL * * *` |
|        - |  6872 | ` *` |
|        - |  6873 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6874 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6875 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6876 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6877 | ` */` |
|        - |  6878 | `/* OP_SHR * * *` |
|        - |  6879 | ` *` |
|        - |  6880 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6881 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6882 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6883 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6884 | ` */` |
|       12 |  6885 | `case PH7_OP_SHL:` |
|        - |  6886 | `case PH7_OP_SHR: {` |
|       25 |  6887 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6888 | `	sxi64 a,r;` |
|        - |  6889 | `	sxi32 b;` |
|        - |  6890 | `#ifdef UNTRUST` |
|        - |  6891 | `	if( pNos < pStack ){` |
|        - |  6892 | `		goto Abort;` |
|        - |  6893 | `	}` |
|        - |  6894 | `#endif` |
|        - |  6895 | `	/* Force the operands to be integer */` |
|       25 |  6896 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6897 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6898 | `	}` |
|       25 |  6899 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6900 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6901 | `	}` |
|        - |  6902 | `	/* Perform the requested operation */` |
|       25 |  6903 | `	a = pNos->x.iVal;` |
|       25 |  6904 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6905 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6906 | `		r = a << b;` |
|        8 |  6907 | `	}else{` |
|       11 |  6908 | `		r = a >> b;` |
|        - |  6909 | `	}` |
|        - |  6910 | `	/* Push the result */` |
|       25 |  6911 | `	pNos->x.iVal = r;` |
|       25 |  6912 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6913 | `	VmPopOperand(&pTos,1);` |
|       25 |  6914 | `	break;` |
|        - |  6915 | `				 }` |
|        - |  6916 | `/*  OP_SHL_STORE * * *` |
|        - |  6917 | ` *` |
|        - |  6918 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6919 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6920 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6921 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6922 | ` */` |
|        - |  6923 | `/* OP_SHR_STORE * * *` |
|        - |  6924 | ` *` |
|        - |  6925 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6926 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6927 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6928 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6929 | ` */` |
|        9 |  6930 | `case PH7_OP_SHL_STORE:` |
|        - |  6931 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6932 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6933 | `	ph7_value *pObj;` |
|        - |  6934 | `	sxi64 a,r;` |
|        - |  6935 | `	sxi32 b;` |
|        - |  6936 | `#ifdef UNTRUST` |
|        - |  6937 | `	if( pNos < pStack ){` |
|        - |  6938 | `		goto Abort;` |
|        - |  6939 | `	}` |
|        - |  6940 | `#endif` |
|        - |  6941 | `	/* Force the operands to be integer */` |
|       19 |  6942 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6943 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6944 | `	}` |
|       19 |  6945 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6946 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6947 | `	}` |
|        - |  6948 | `	/* Perform the requested operation */` |
|       19 |  6949 | `	a = pTos->x.iVal;` |
|       19 |  6950 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6951 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6952 | `		r = a << b;` |
|        5 |  6953 | `	}else{` |
|       11 |  6954 | `		r = a >> b;` |
|        - |  6955 | `	}` |
|        - |  6956 | `	/* Push the result */` |
|       19 |  6957 | `	pNos->x.iVal = r;` |
|       19 |  6958 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6959 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6960 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6961 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6962 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6963 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6964 | `	}` |
|       19 |  6965 | `	VmPopOperand(&pTos,1);` |
|       19 |  6966 | `	break;` |
|        - |  6967 | `				 }` |
|        - |  6968 | `/* CAT:  P1 * *` |
|        - |  6969 | ` *` |
|        - |  6970 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6971 | ` * back.` |
|        - |  6972 | ` */` |
|    72383 |  6973 | `case PH7_OP_CAT:{` |
|        - |  6974 | `	ph7_value *pNos,*pCur;` |
|   144771 |  6975 | `	if( pInstr->iP1 < 1 ){` |
|   117283 |  6976 | `		pNos = &pTos[-1];` |
|    58644 |  6977 | `	}else{` |
|    27493 |  6978 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6979 | `	}` |
|        - |  6980 | `#ifdef UNTRUST` |
|        - |  6981 | `	if( pNos < pStack ){` |
|        - |  6982 | `		goto Abort;` |
|        - |  6983 | `	}` |
|        - |  6984 | `#endif` |
|        - |  6985 | `	/* Force a string cast */` |
|   144771 |  6986 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1685 |  6987 | `		PH7_MemObjToString(pNos);` |
|      840 |  6988 | `	}` |
|   144771 |  6989 | `	pCur = &pNos[1];` |
|   292269 |  6990 | `	while( pCur <= pTos ){` |
|   147503 |  6991 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50969 |  6992 | `			PH7_MemObjToString(pCur);` |
|    25482 |  6993 | `		}` |
|        - |  6994 | `		/* Perform the concatenation */` |
|   147503 |  6995 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   147459 |  6996 | `			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){` |
|        - |  6997 | `				/* Allocation failure: raise a fatal instead of a truncated concat */` |
|      ! 0 |  6998 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  6999 | `				goto Abort;` |
|        - |  7000 | `			}` |
|    73727 |  7001 | `		}` |
|   147503 |  7002 | `		SyBlobRelease(&pCur->sBlob);` |
|   147503 |  7003 | `		pCur++;` |
|        5 |  7004 | `	}` |
|   144771 |  7005 | `	pTos = pNos;` |
|   144771 |  7006 | `	break;` |
|        - |  7007 | `				}` |
|        - |  7008 | `/*  CAT_STORE: * * *` |
|        - |  7009 | ` *` |
|        - |  7010 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  7011 | ` * back.` |
|        - |  7012 | ` */` |
|     4149 |  7013 | `case PH7_OP_CAT_STORE:{` |
|     8303 |  7014 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7015 | `	ph7_value *pObj;` |
|        - |  7016 | `	sxu32 nIdx;` |
|        - |  7017 | `#ifdef UNTRUST` |
|        - |  7018 | `	if( pNos < pStack ){` |
|        - |  7019 | `		goto Abort;` |
|        - |  7020 | `	}` |
|        - |  7021 | `#endif` |
|        - |  7022 | `	/* The right operand must be a string to append it */` |
|     8303 |  7023 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7024 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7025 | `	}` |
|     8303 |  7026 | `	nIdx = pTos->nIdx;` |
|        - |  7027 | `	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer` |
|        - |  7028 | `	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then` |
|        - |  7029 | ``	 * storing the whole buffer back twice. This turns `$s .= ...` (and the`` |
|        - |  7030 | `	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).` |
|        - |  7031 | `	 * Guards: a real owned slot; the right operand must NOT alias that same slot` |
|        - |  7032 | ``	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under`` |
|        - |  7033 | `	 * the source we copy from — references share the slot index, so one check` |
|        - |  7034 | `	 * covers both); and not a typed property, whose store-time type check/coercion` |
|        - |  7035 | `	 * must run before any mutation (left to the slow path).` |
|        - |  7036 | ``	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here`` |
|        - |  7037 | `	 * and remains O(n^2) by design. */` |
|     8301 |  7038 | `	if( nIdx != SXU32_HIGH` |
|     8298 |  7039 | `	 && nIdx != pNos->nIdx` |
|     8294 |  7040 | `	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0` |
|     8295 |  7041 | `	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0` |
|     4148 |  7042 | `	     \|\| SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){` |
|     8289 |  7043 | `		if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7044 | `			/* e.g. $x = 5; $x .= "a";  ->  "5a" */` |
|        3 |  7045 | `			PH7_MemObjToString(pObj);` |
|        1 |  7046 | `		}` |
|     8289 |  7047 | `		if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8287 |  7048 | `			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7049 | `				/* Allocation failure: the grow happens before the copy, so pObj` |
|        - |  7050 | `				 * keeps its prior valid contents — raise the fatal uncorrupted. */` |
|      ! 0 |  7051 | `				PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7052 | `				goto Abort;` |
|        - |  7053 | `			}` |
|     4141 |  7054 | `		}` |
|        - |  7055 | ``		/* Produce the expression result. A `.=` result is a temporary, never an`` |
|        - |  7056 | ``		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a`` |
|        - |  7057 | ``		 * by-ref param, or `&($s .= "x")`, would alias the live variable).`` |
|        - |  7058 | ``		 * In the dominant statement form `$s .= "x";` the result is discarded by the`` |
|        - |  7059 | `		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)` |
|        - |  7060 | `		 * RHS operand for the POP to drop — keeping the hot path allocation-free.` |
|        - |  7061 | `		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy` |
|        - |  7062 | `		 * of the updated value: a read-only alias into pObj's buffer would dangle if` |
|        - |  7063 | `		 * the same slot is appended to again later in the statement` |
|        - |  7064 | ``		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result`` |
|        - |  7065 | `		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a` |
|        - |  7066 | `		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */` |
|     8289 |  7067 | `		if( (pInstr+1)->iOp != PH7_OP_POP ){` |
|        9 |  7068 | `			PH7_MemObjStore(pObj,pNos);` |
|        4 |  7069 | `		}` |
|     8289 |  7070 | `		pNos->nIdx = SXU32_HIGH;` |
|     8289 |  7071 | `		VmPopOperand(&pTos,1);` |
|     8296 |  7072 | `		break;` |
|        - |  7073 | `	}` |
|        - |  7074 | `	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */` |
|       16 |  7075 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7076 | `		/* Force a string cast */` |
|        6 |  7077 | `		PH7_MemObjToString(pTos);` |
|        2 |  7078 | `	}` |
|        - |  7079 | `	/* Perform the concatenation (Reverse order) */` |
|       16 |  7080 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       16 |  7081 | `		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){` |
|        - |  7082 | `			/* Allocation failure: raise a fatal before committing the store so` |
|        - |  7083 | `			 * no partially-concatenated value is written to the lvalue. */` |
|      ! 0 |  7084 | `			PH7_VmMemoryError(&(*pVm));` |
|      ! 0 |  7085 | `			goto Abort;` |
|        - |  7086 | `		}` |
|        7 |  7087 | `	}` |
|        - |  7088 | `	/* Perform the store operation */` |
|       16 |  7089 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7090 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       16 |  7091 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       16 |  7092 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|       11 |  7093 | `		PH7_MemObjStore(pTos,pObj);` |
|        5 |  7094 | `	}` |
|       11 |  7095 | `	PH7_MemObjStore(pTos,pNos);` |
|       11 |  7096 | `	VmPopOperand(&pTos,1);` |
|       11 |  7097 | `	break;` |
|        - |  7098 | `				}` |
|        - |  7099 | `/* OP_AND: * * *` |
|        - |  7100 | ` *` |
|        - |  7101 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  7102 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7103 | ` * stack.` |
|        - |  7104 | ` */` |
|        - |  7105 | `/* OP_OR: * * *` |
|        - |  7106 | ` *` |
|        - |  7107 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  7108 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7109 | ` * stack.` |
|        - |  7110 | ` */` |
|   108737 |  7111 | `case PH7_OP_LAND:` |
|        - |  7112 | `case PH7_OP_LOR: {` |
|   217523 |  7113 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7114 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  7115 | `#ifdef UNTRUST` |
|        - |  7116 | `	if( pNos < pStack ){` |
|        - |  7117 | `		goto Abort;` |
|        - |  7118 | `	}` |
|        - |  7119 | `#endif` |
|        - |  7120 | `	/* Force a boolean cast */` |
|   217523 |  7121 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  7122 | `		PH7_MemObjToBool(pTos);` |
|        1 |  7123 | `	}` |
|   217523 |  7124 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7125 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7126 | `	}` |
|   217523 |  7127 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   217523 |  7128 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   217523 |  7129 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  7130 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   100227 |  7131 | `		v1 = and_logic[v1*3+v2];` |
|    50138 |  7132 | `	}else{` |
|        - |  7133 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117301 |  7134 | `		v1 = or_logic[v1*3+v2];` |
|        - |  7135 | `	}` |
|   217523 |  7136 | `	if( v1 == 2 ){` |
|      ! 0 |  7137 | `		v1 = 1;` |
|      ! 0 |  7138 | `	}` |
|   217523 |  7139 | `	VmPopOperand(&pTos,1);` |
|   217523 |  7140 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   217523 |  7141 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   217523 |  7142 | `	break;` |
|        - |  7143 | `				 }` |
|        - |  7144 | `/*` |
|        - |  7145 | ` * OP_NULLC: * * *` |
|        - |  7146 | ` * Null coalescing operator '??'.` |
|        - |  7147 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  7148 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  7149 | ` */` |
|        - |  7150 | `/*` |
|        - |  7151 | ` * OP_NULLC: * P2 *` |
|        - |  7152 | ` * Short-circuit null coalescing '??'.` |
|        - |  7153 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  7154 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  7155 | ` */` |
|       99 |  7156 | `case PH7_OP_NULLC: {` |
|        - |  7157 | `#ifdef UNTRUST` |
|        - |  7158 | `	if( pTos < pStack ){` |
|        - |  7159 | `		goto Abort;` |
|        - |  7160 | `	}` |
|        - |  7161 | `#endif` |
|      203 |  7162 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7163 | `		/* Left is not null — keep it and skip the RHS */` |
|      123 |  7164 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       64 |  7165 | `	}else{` |
|        - |  7166 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       85 |  7167 | `		VmPopOperand(&pTos, 1);` |
|        - |  7168 | `	}` |
|      203 |  7169 | `	break;` |
|        - |  7170 |  |
|        - |  7171 | `/*` |
|        - |  7172 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  7173 | ` * Null coalescing assignment short-circuit.` |
|        - |  7174 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  7175 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  7176 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  7177 | ` */` |
|       28 |  7178 | `case PH7_OP_NULLC_JMP: {` |
|        - |  7179 | `#ifdef UNTRUST` |
|        - |  7180 | `	if( pTos < pStack ){` |
|        - |  7181 | `		goto Abort;` |
|        - |  7182 | `	}` |
|        - |  7183 | `#endif` |
|       59 |  7184 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  7185 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  7186 | `	}` |
|       59 |  7187 | `	break;` |
|        - |  7188 |  |
|        - |  7189 | `/*` |
|        - |  7190 | ` * OP_NULLC_STORE: * * *` |
|        - |  7191 | ` * Null coalescing assignment store.` |
|        - |  7192 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  7193 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  7194 | ` * expression result.` |
|        - |  7195 | ` */` |
|        - |  7196 | `/*` |
|        - |  7197 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  7198 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  7199 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  7200 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  7201 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  7202 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  7203 | ` */` |
|       51 |  7204 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  7205 | `#ifdef UNTRUST` |
|        - |  7206 | `	if( pTos < pStack ){` |
|        - |  7207 | `		goto Abort;` |
|        - |  7208 | `	}` |
|        - |  7209 | `#endif` |
|      105 |  7210 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  7211 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  7212 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  7213 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  7214 | `	}` |
|      105 |  7215 | `	break;` |
|        - |  7216 |  |
|       17 |  7217 | `case PH7_OP_NULLC_STORE: {` |
|       37 |  7218 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7219 | `	ph7_value *pObj;` |
|        - |  7220 | `	sxu32 nIdx;` |
|        - |  7221 | `#ifdef UNTRUST` |
|        - |  7222 | `	if( pNos < pStack ){` |
|        - |  7223 | `		goto Abort;` |
|        - |  7224 | `	}` |
|        - |  7225 | `#endif` |
|        - |  7226 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  7227 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  7228 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       37 |  7229 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  7230 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  7231 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  7232 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  7233 | `		ph7_value *apArg[2];` |
|        5 |  7234 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  7235 | `		apArg[1] = pTos;` |
|        5 |  7236 | `		if( pSet ){` |
|        5 |  7237 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  7238 | `		}` |
|        - |  7239 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  7240 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  7241 | `		VmPopOperand(&pTos,1);` |
|        - |  7242 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  7243 | `		VmCoalesceDisarm(pVm);` |
|        5 |  7244 | `		break;` |
|        - |  7245 | `	}` |
|       32 |  7246 | `	nIdx = pNos->nIdx;` |
|       32 |  7247 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  7248 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7249 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  7250 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  7251 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  7252 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  7253 | `	}` |
|       32 |  7254 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  7255 | `	VmPopOperand(&pTos,1);` |
|       32 |  7256 | `	break;` |
|        - |  7257 |  |
|        - |  7258 | `/*` |
|        - |  7259 | ` * OP_SPREAD: * * *` |
|        - |  7260 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  7261 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  7262 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  7263 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  7264 | ` */` |
|       10 |  7265 | `case PH7_OP_SPREAD: {` |
|        - |  7266 | `#ifdef UNTRUST` |
|        - |  7267 | `	if( pTos < pStack ){` |
|        - |  7268 | `		goto Abort;` |
|        - |  7269 | `	}` |
|        - |  7270 | `#endif` |
|        - |  7271 | `	/* Traversable argument unpacking f(...$it): materialize the iterator into a` |
|        - |  7272 | `	 * temp array (positional values), then expand it onto the operand stack` |
|        - |  7273 | `	 * like an array. Materialising first leaves the stack untouched until the` |
|        - |  7274 | `	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can` |
|        - |  7275 | `	 * be freed immediately. */` |
|       23 |  7276 | `	if( VmValueIsTraversable(pVm,pTos) ){` |
|        3 |  7277 | `		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);` |
|        - |  7278 | `		sxi32 rcW;` |
|        - |  7279 | `		sxu32 nEnt;` |
|        3 |  7280 | `		if( pTmpMap == 0 ){ goto Abort; }` |
|        3 |  7281 | `		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);` |
|        3 |  7282 | `		if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      ! 0 |  7283 | `			PH7_HashmapRelease(pTmpMap,TRUE);` |
|      ! 0 |  7284 | `			if( rcW == PH7_ABORT ){ goto Abort; }` |
|      ! 0 |  7285 | `			goto Exception;` |
|        - |  7286 | `		}` |
|        3 |  7287 | `		nEnt = pTmpMap->nEntry;` |
|        3 |  7288 | `		if( nEnt == 0 ){` |
|      ! 0 |  7289 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7290 | `			pVm->iSpreadExtra--;` |
|        3 |  7291 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEnt - 1) >= VM_STACK_GUARD ){` |
|      ! 0 |  7292 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7293 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)", VM_STACK_GUARD);` |
|      ! 0 |  7294 | `		}else{` |
|        3 |  7295 | `			ph7_hashmap_node *pNodeT = pTmpMap->pFirst;` |
|        - |  7296 | `			ph7_value *pElemT;` |
|        - |  7297 | `			sxu32 iT;` |
|        3 |  7298 | `			pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        3 |  7299 | `			if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }else{ PH7_MemObjRelease(pTos); }` |
|        3 |  7300 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  7301 | `			pNodeT = pNodeT->pPrev;` |
|        7 |  7302 | `			for( iT = 1; iT < nEnt; iT++ ){` |
|        5 |  7303 | `				pTos++;` |
|        5 |  7304 | `				PH7_MemObjInit(pVm, pTos);` |
|        5 |  7305 | `				pTos->nIdx = SXU32_HIGH;` |
|        5 |  7306 | `				pElemT = (ph7_value *)SySetAt(&pVm->aMemObj, pNodeT->nValIdx);` |
|        5 |  7307 | `				if( pElemT ){ PH7_MemObjStore(pElemT, pTos); }` |
|        5 |  7308 | `				pNodeT = pNodeT->pPrev;` |
|        3 |  7309 | `			}` |
|        3 |  7310 | `			pVm->iSpreadExtra += (sxi32)(nEnt - 1);` |
|        - |  7311 | `		}` |
|        3 |  7312 | `		PH7_HashmapRelease(pTmpMap,TRUE);` |
|        3 |  7313 | `		break;` |
|        - |  7314 | `	}` |
|       21 |  7315 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       21 |  7316 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       21 |  7317 | `		sxu32 nEntry = pMap->nEntry;` |
|       21 |  7318 | `		if( nEntry == 0 ){` |
|        - |  7319 | `			/* Empty array — remove from stack */` |
|        3 |  7320 | `			VmPopOperand(&pTos, 1);` |
|        3 |  7321 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       20 |  7322 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  7323 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  7324 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  7325 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  7326 | `				VM_STACK_GUARD);` |
|      ! 0 |  7327 | `		}else{` |
|        - |  7328 | `			ph7_hashmap_node *pNode2;` |
|        - |  7329 | `			ph7_value *pElem;` |
|        - |  7330 | `			sxu32 i;` |
|        - |  7331 | `			/* Overwrite TOS with first element */` |
|       19 |  7332 | `			pNode2 = pMap->pFirst;` |
|       19 |  7333 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       19 |  7334 | `			PH7_MemObjRelease(pTos);` |
|       19 |  7335 | `			if( pElem ){` |
|       19 |  7336 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  7337 | `			}` |
|       19 |  7338 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7339 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  7340 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       19 |  7341 | `			pNode2 = pNode2->pPrev;` |
|        - |  7342 | `			/* Push remaining elements */` |
|       45 |  7343 | `			for( i = 1; i < nEntry; i++ ){` |
|       29 |  7344 | `				pTos++;` |
|       29 |  7345 | `				PH7_MemObjInit(pVm, pTos);` |
|       29 |  7346 | `				pTos->nIdx = SXU32_HIGH;` |
|       29 |  7347 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       29 |  7348 | `				if( pElem ){` |
|       29 |  7349 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  7350 | `				}` |
|       29 |  7351 | `				pNode2 = pNode2->pPrev;` |
|       16 |  7352 | `			}` |
|       19 |  7353 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  7354 | `		}` |
|        9 |  7355 | `	}` |
|        - |  7356 | `	/* else: not an array — leave as-is (single arg) */` |
|       21 |  7357 | `	break;` |
|        - |  7358 |  |
|        - |  7359 | `/*` |
|        - |  7360 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  7361 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  7362 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  7363 | ` */` |
|       37 |  7364 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  7365 | `#ifdef UNTRUST` |
|        - |  7366 | `	if( pTos < pStack ){` |
|        - |  7367 | `		goto Abort;` |
|        - |  7368 | `	}` |
|        - |  7369 | `#endif` |
|       77 |  7370 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       77 |  7371 | `	break;` |
|        - |  7372 |  |
|        - |  7373 | `/* OP_LXOR: * * *` |
|        - |  7374 | ` *` |
|        - |  7375 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  7376 | ` * two values and push the resulting boolean value back onto the` |
|        - |  7377 | ` * stack.` |
|        - |  7378 | ` * According to the PHP language reference manual:` |
|        - |  7379 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  7380 | ` *  TRUE,but not both.` |
|        - |  7381 | ` */` |
|        5 |  7382 | `case PH7_OP_LXOR:{` |
|       11 |  7383 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  7384 | `	sxi32 v = 0;` |
|        - |  7385 | `#ifdef UNTRUST` |
|        - |  7386 | `	if( pNos < pStack ){` |
|        - |  7387 | `		goto Abort;` |
|        - |  7388 | `	}` |
|        - |  7389 | `#endif` |
|        - |  7390 | `	/* Force a boolean cast */` |
|       11 |  7391 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7392 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  7393 | `	}` |
|       11 |  7394 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  7395 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  7396 | `	}` |
|       11 |  7397 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  7398 | `		v = 1;` |
|        3 |  7399 | `	}` |
|       11 |  7400 | `	VmPopOperand(&pTos,1);` |
|       11 |  7401 | `	pTos->x.iVal = v;` |
|       11 |  7402 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  7403 | `	break;` |
|        - |  7404 | `				 }` |
|        - |  7405 | `/* OP_EQ P1 P2 P3` |
|        - |  7406 | ` *` |
|        - |  7407 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  7408 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7409 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7410 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7411 | ` */` |
|        - |  7412 | `/* OP_NEQ P1 P2 P3` |
|        - |  7413 | ` *` |
|        - |  7414 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  7415 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7416 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7417 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7418 | ` */` |
|     4630 |  7419 | `case PH7_OP_EQ:` |
|        - |  7420 | `case PH7_OP_NEQ: {` |
|     9265 |  7421 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7422 | `	/* Perform the comparison and act accordingly */` |
|        - |  7423 | `#ifdef UNTRUST` |
|        - |  7424 | `	if( pNos < pStack ){` |
|        - |  7425 | `		goto Abort;` |
|        - |  7426 | `	}` |
|        - |  7427 | `#endif` |
|     9265 |  7428 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     9265 |  7429 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  7430 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     9256 |  7431 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     9221 |  7432 | `		rc = rc == 0;` |
|     4613 |  7433 | `	}else{` |
|       31 |  7434 | `		rc = rc != 0;` |
|        - |  7435 | `	}` |
|     9265 |  7436 | `	VmPopOperand(&pTos,1);` |
|     9265 |  7437 | `	if( !pInstr->iP2 ){` |
|        - |  7438 | `		/* Push comparison result without taking the jump */` |
|     9265 |  7439 | `		PH7_MemObjRelease(pTos);` |
|     9265 |  7440 | `		pTos->x.iVal = rc;` |
|        - |  7441 | `		/* Invalidate any prior representation */` |
|     9265 |  7442 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4635 |  7443 | `	}else{` |
|      ! 0 |  7444 | `		if( rc ){` |
|        - |  7445 | `			/* Jump to the desired location */` |
|      ! 0 |  7446 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7447 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7448 | `		}` |
|        - |  7449 | `	}` |
|     9265 |  7450 | `	break;` |
|        - |  7451 | `				 }` |
|        - |  7452 | `/* OP_TEQ P1 P2 *` |
|        - |  7453 | ` *` |
|        - |  7454 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  7455 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  7456 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7457 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7458 | ` */` |
|   163074 |  7459 | `case PH7_OP_TEQ: {` |
|   326153 |  7460 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7461 | `	/* Perform the comparison and act accordingly */` |
|        - |  7462 | `#ifdef UNTRUST` |
|        - |  7463 | `	if( pNos < pStack ){` |
|        - |  7464 | `		goto Abort;` |
|        - |  7465 | `	}` |
|        - |  7466 | `#endif` |
|   326153 |  7467 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   326153 |  7468 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7469 | `		rc = 0;` |
|        2 |  7470 | `	}else{` |
|   326151 |  7471 | `		rc = rc == 0;` |
|        - |  7472 | `	}` |
|   326153 |  7473 | `	VmPopOperand(&pTos,1);` |
|   326153 |  7474 | `	if( !pInstr->iP2 ){` |
|        - |  7475 | `		/* Push comparison result without taking the jump */` |
|   326153 |  7476 | `		PH7_MemObjRelease(pTos);` |
|   326153 |  7477 | `		pTos->x.iVal = rc;` |
|        - |  7478 | `		/* Invalidate any prior representation */` |
|   326153 |  7479 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   163079 |  7480 | `	}else{` |
|      ! 0 |  7481 | `		if( rc ){` |
|        - |  7482 | `			/* Jump to the desired location */` |
|      ! 0 |  7483 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7484 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7485 | `		}` |
|        - |  7486 | `	}` |
|   326153 |  7487 | `	break;` |
|        - |  7488 | `				 }` |
|        - |  7489 | `/* OP_TNE P1 P2 *` |
|        - |  7490 | ` *` |
|        - |  7491 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  7492 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  7493 | ` * instruction.` |
|        - |  7494 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7495 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7496 | ` *` |
|        - |  7497 | ` */` |
|   125408 |  7498 | `case PH7_OP_TNE: {` |
|   250821 |  7499 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7500 | `	/* Perform the comparison and act accordingly */` |
|        - |  7501 | `#ifdef UNTRUST` |
|        - |  7502 | `	if( pNos < pStack ){` |
|        - |  7503 | `		goto Abort;` |
|        - |  7504 | `	}` |
|        - |  7505 | `#endif` |
|   250821 |  7506 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   250821 |  7507 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  7508 | `		rc = 1;` |
|        2 |  7509 | `	}else{` |
|   250819 |  7510 | `		rc = rc != 0;` |
|        - |  7511 | `	}` |
|   250821 |  7512 | `	VmPopOperand(&pTos,1);` |
|   250821 |  7513 | `	if( !pInstr->iP2 ){` |
|        - |  7514 | `		/* Push comparison result without taking the jump */` |
|   250821 |  7515 | `		PH7_MemObjRelease(pTos);` |
|   250821 |  7516 | `		pTos->x.iVal = rc;` |
|        - |  7517 | `		/* Invalidate any prior representation */` |
|   250821 |  7518 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   125413 |  7519 | `	}else{` |
|      ! 0 |  7520 | `		if( rc ){` |
|        - |  7521 | `			/* Jump to the desired location */` |
|      ! 0 |  7522 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7523 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7524 | `		}` |
|        - |  7525 | `	}` |
|   250821 |  7526 | `	break;` |
|        - |  7527 | `				 }` |
|        - |  7528 | `/* OP_LT P1 P2 P3` |
|        - |  7529 | ` *` |
|        - |  7530 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7531 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7532 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7533 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7534 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7535 | ` *` |
|        - |  7536 | ` */` |
|        - |  7537 | `/* OP_LE P1 P2 P3` |
|        - |  7538 | ` *` |
|        - |  7539 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7540 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7541 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7542 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7543 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7544 | ` *` |
|        - |  7545 | ` */` |
|   112622 |  7546 | `case PH7_OP_LT:` |
|        - |  7547 | `case PH7_OP_LE: {` |
|   225293 |  7548 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7549 | `	/* Perform the comparison and act accordingly */` |
|        - |  7550 | `#ifdef UNTRUST` |
|        - |  7551 | `	if( pNos < pStack ){` |
|        - |  7552 | `		goto Abort;` |
|        - |  7553 | `	}` |
|        - |  7554 | `#endif` |
|   225293 |  7555 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   225293 |  7556 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7557 | `		rc = 0;` |
|   225289 |  7558 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1611 |  7559 | `		rc = rc < 1;` |
|      808 |  7560 | `	}else{` |
|   223679 |  7561 | `		rc = rc < 0;` |
|        - |  7562 | `	}` |
|   225293 |  7563 | `	VmPopOperand(&pTos,1);` |
|   225293 |  7564 | `	if( !pInstr->iP2 ){` |
|        - |  7565 | `		/* Push comparison result without taking the jump */` |
|   225293 |  7566 | `		PH7_MemObjRelease(pTos);` |
|   225293 |  7567 | `		pTos->x.iVal = rc;` |
|        - |  7568 | `		/* Invalidate any prior representation */` |
|   225293 |  7569 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112671 |  7570 | `	}else{` |
|      ! 0 |  7571 | `		if( rc ){` |
|        - |  7572 | `			/* Jump to the desired location */` |
|      ! 0 |  7573 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7574 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7575 | `		}` |
|        - |  7576 | `	}` |
|   225293 |  7577 | `	break;` |
|        - |  7578 | `				}` |
|        - |  7579 | `/* OP_GT P1 P2 P3` |
|        - |  7580 | ` *` |
|        - |  7581 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7582 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  7583 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7584 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7585 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7586 | ` *` |
|        - |  7587 | ` */` |
|        - |  7588 | `/* OP_GE P1 P2 P3` |
|        - |  7589 | ` *` |
|        - |  7590 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  7591 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  7592 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  7593 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7594 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7595 | ` *` |
|        - |  7596 | ` */` |
|    55703 |  7597 | `case PH7_OP_GT:` |
|        - |  7598 | `case PH7_OP_GE: {` |
|   111411 |  7599 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7600 | `	/* Perform the comparison and act accordingly */` |
|        - |  7601 | `#ifdef UNTRUST` |
|        - |  7602 | `	if( pNos < pStack ){` |
|        - |  7603 | `		goto Abort;` |
|        - |  7604 | `	}` |
|        - |  7605 | `#endif` |
|   111411 |  7606 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111411 |  7607 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  7608 | `		rc = 0;` |
|   111407 |  7609 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110971 |  7610 | `		rc = rc >= 0;` |
|    55488 |  7611 | `	}else{` |
|      437 |  7612 | `		rc = rc > 0;` |
|        - |  7613 | `	}` |
|   111411 |  7614 | `	VmPopOperand(&pTos,1);` |
|   111411 |  7615 | `	if( !pInstr->iP2 ){` |
|        - |  7616 | `		/* Push comparison result without taking the jump */` |
|   111411 |  7617 | `		PH7_MemObjRelease(pTos);` |
|   111411 |  7618 | `		pTos->x.iVal = rc;` |
|        - |  7619 | `		/* Invalidate any prior representation */` |
|   111411 |  7620 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55708 |  7621 | `	}else{` |
|      ! 0 |  7622 | `		if( rc ){` |
|        - |  7623 | `			/* Jump to the desired location */` |
|      ! 0 |  7624 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7625 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7626 | `		}` |
|        - |  7627 | `	}` |
|   111411 |  7628 | `	break;` |
|        - |  7629 | `				}` |
|        - |  7630 | `/* OP_SPACESHIP * * *` |
|        - |  7631 | ` *` |
|        - |  7632 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  7633 | ` *   -1 if left < right` |
|        - |  7634 | ` *    0 if left == right` |
|        - |  7635 | ` *    1 if left > right` |
|        - |  7636 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  7637 | ` */` |
|       25 |  7638 | `case PH7_OP_SPACESHIP: {` |
|       51 |  7639 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7640 | `#ifdef UNTRUST` |
|        - |  7641 | `	if( pNos < pStack ){` |
|        - |  7642 | `		goto Abort;` |
|        - |  7643 | `	}` |
|        - |  7644 | `#endif` |
|       51 |  7645 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  7646 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  7647 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  7648 | `		rc = 1;` |
|        4 |  7649 | `	}else{` |
|        - |  7650 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  7651 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  7652 | `	}` |
|       51 |  7653 | `	VmPopOperand(&pTos,1);` |
|       51 |  7654 | `	PH7_MemObjRelease(pTos);` |
|       51 |  7655 | `	pTos->x.iVal = rc;` |
|       51 |  7656 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  7657 | `	break;` |
|        - |  7658 | `				}` |
|        - |  7659 | `/* OP_SEQ P1 P2 *` |
|        - |  7660 | ` * Strict string comparison.` |
|        - |  7661 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  7662 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7663 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7664 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7665 | ` * use PH7_OP_EQ.` |
|        - |  7666 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7667 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7668 | ` */` |
|        - |  7669 | `/* OP_SNE P1 P2 *` |
|        - |  7670 | ` * Strict string comparison.` |
|        - |  7671 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  7672 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  7673 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  7674 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  7675 | ` * use PH7_OP_EQ.` |
|        - |  7676 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  7677 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  7678 | ` */` |
|       18 |  7679 | `case PH7_OP_SEQ:` |
|        - |  7680 | `case PH7_OP_SNE: {` |
|       38 |  7681 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  7682 | `	SyString s1,s2;` |
|        - |  7683 | `	/* Perform the comparison and act accordingly */` |
|        - |  7684 | `#ifdef UNTRUST` |
|        - |  7685 | `	if( pNos < pStack ){` |
|        - |  7686 | `		goto Abort;` |
|        - |  7687 | `	}` |
|        - |  7688 | `#endif` |
|        - |  7689 | `	/* Force a string cast */` |
|       38 |  7690 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  7691 | `		PH7_MemObjToString(pTos);` |
|        2 |  7692 | `	}` |
|       38 |  7693 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  7694 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  7695 | `	}` |
|       38 |  7696 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  7697 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  7698 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  7699 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  7700 | `		rc = rc != 0;` |
|      ! 0 |  7701 | `	}else{` |
|       38 |  7702 | `		rc = rc == 0;` |
|        - |  7703 | `	}` |
|       38 |  7704 | `	VmPopOperand(&pTos,1);` |
|       38 |  7705 | `	if( !pInstr->iP2 ){` |
|        - |  7706 | `		/* Push comparison result without taking the jump */` |
|       38 |  7707 | `		PH7_MemObjRelease(pTos);` |
|       38 |  7708 | `		pTos->x.iVal = rc;` |
|        - |  7709 | `		/* Invalidate any prior representation */` |
|       38 |  7710 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  7711 | `	}else{` |
|      ! 0 |  7712 | `		if( rc ){` |
|        - |  7713 | `			/* Jump to the desired location */` |
|      ! 0 |  7714 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7715 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  7716 | `		}` |
|        - |  7717 | `	}` |
|       38 |  7718 | `	break;` |
|        - |  7719 | `				 }` |
|        - |  7720 | `/*` |
|        - |  7721 | ` * OP_LOAD_REF * * *` |
|        - |  7722 | ` * Push the index of a referenced object on the stack.` |
|        - |  7723 | ` */` |
|       60 |  7724 | `case PH7_OP_LOAD_REF: {` |
|        - |  7725 | `	sxu32 nIdx;` |
|        - |  7726 | `#ifdef UNTRUST` |
|        - |  7727 | `	if( pTos < pStack ){` |
|        - |  7728 | `		goto Abort;` |
|        - |  7729 | `	}` |
|        - |  7730 | `#endif` |
|        - |  7731 | `	/* Extract memory object index */` |
|      121 |  7732 | `	nIdx = pTos->nIdx;` |
|      121 |  7733 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7734 | `		/* Nullify the object */` |
|      101 |  7735 | `		PH7_MemObjRelease(pTos);` |
|        - |  7736 | `		/* Mark as constant and store the index on the top of the stack */` |
|      101 |  7737 | `		pTos->x.iVal = (sxi64)nIdx;` |
|      101 |  7738 | `		pTos->nIdx = SXU32_HIGH;` |
|      101 |  7739 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       50 |  7740 | `	}` |
|      121 |  7741 | `	break;` |
|        - |  7742 | `					  }` |
|        - |  7743 | `/*` |
|        - |  7744 | ` * OP_STORE_REF * * P3` |
|        - |  7745 | ` * Perform an assignment operation by reference.` |
|        - |  7746 | ` */` |
|       18 |  7747 | ` case PH7_OP_STORE_REF: {` |
|       38 |  7748 | `	 SyString sName = { 0 , 0 };` |
|        - |  7749 | `	 VmFrame *pFrameLocal;` |
|        - |  7750 | `	SyHashEntry *pEntry;` |
|        - |  7751 | `	sxu32 nIdx;` |
|        - |  7752 | `#ifdef UNTRUST` |
|        - |  7753 | `	if( pTos < pStack ){` |
|        - |  7754 | `		goto Abort;` |
|        - |  7755 | `	}` |
|        - |  7756 | `#endif` |
|       38 |  7757 | `	if( pInstr->p3 == 0 ){` |
|        - |  7758 | `		char *zName;` |
|        - |  7759 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7760 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7761 | `			/* Force a string cast */` |
|      ! 0 |  7762 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7763 | `		}` |
|      ! 0 |  7764 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7765 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7766 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7767 | `			if( zName ){` |
|      ! 0 |  7768 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7769 | `			}` |
|      ! 0 |  7770 | `		}` |
|      ! 0 |  7771 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7772 | `		pTos--;` |
|      ! 0 |  7773 | `	}else{` |
|       38 |  7774 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7775 | `	}` |
|       38 |  7776 | `	nIdx = pTos->nIdx;` |
|       38 |  7777 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7778 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7779 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7780 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7781 | `		}else{` |
|        - |  7782 | `			ph7_value *pObj;` |
|        - |  7783 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7784 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7785 | `			if( pObj == 0 ){` |
|      ! 0 |  7786 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7787 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7788 | `				goto Abort;` |
|        - |  7789 | `			}` |
|        - |  7790 | `			/* Perform the store operation */` |
|      ! 0 |  7791 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7792 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7793 | `		}` |
|       38 |  7794 | `	}else if( sName.nByte > 0){` |
|       38 |  7795 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7796 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7797 | `		}else{` |
|       38 |  7798 | `			pFrameLocal = pVm->pFrame;` |
|       38 |  7799 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7800 | `			/* Query the local frame */` |
|       38 |  7801 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       38 |  7802 | `			if( pEntry ){` |
|      ! 0 |  7803 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7804 | `			}else{` |
|       38 |  7805 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       38 |  7806 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7807 | `					/* Insert in the $GLOBALS array */` |
|       34 |  7808 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       16 |  7809 | `				}` |
|       38 |  7810 | `				if( rc == SXRET_OK ){` |
|       38 |  7811 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       18 |  7812 | `				}` |
|        - |  7813 | `			}` |
|        - |  7814 | `		}` |
|       18 |  7815 | `	}` |
|       38 |  7816 | `	break;` |
|        - |  7817 | `				 }` |
|        - |  7818 | `/*` |
|        - |  7819 | ` * OP_UPLINK P1 * *` |
|        - |  7820 | ` * Link a variable to the top active VM frame.` |
|        - |  7821 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7822 | ` */` |
|       30 |  7823 | `case PH7_OP_UPLINK: {` |
|       65 |  7824 | `	if( pVm->pFrame->pParent ){` |
|       65 |  7825 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7826 | `		SyString sName;` |
|        - |  7827 | `		/* Perform the link */` |
|      135 |  7828 | `		while( pLink <= pTos ){` |
|       75 |  7829 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7830 | `				/* Force a string cast */` |
|      ! 0 |  7831 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7832 | `			}` |
|       75 |  7833 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       75 |  7834 | `			if( sName.nByte > 0 ){` |
|       75 |  7835 | `				VmFrameLink(&(*pVm),&sName);` |
|       35 |  7836 | `			}` |
|       75 |  7837 | `			pLink++;` |
|        5 |  7838 | `		}` |
|       30 |  7839 | `	}` |
|       65 |  7840 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       65 |  7841 | `	break;` |
|        - |  7842 | `					}` |
|        - |  7843 | `/*` |
|        - |  7844 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7845 | ` * Push an exception in the corresponding container so that` |
|        - |  7846 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7847 | ` */` |
|      222 |  7848 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      449 |  7849 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7850 | `	VmFrame *pFrameLocal;` |
|        - |  7851 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      449 |  7852 | `	pException->iFinallyDone = 0;` |
|      449 |  7853 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7854 | `	/* Create the exception frame */` |
|      449 |  7855 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      449 |  7856 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7857 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7858 | `		goto Abort;` |
|        - |  7859 | `	}` |
|        - |  7860 | `	/* Mark the special frame */` |
|      449 |  7861 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      449 |  7862 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7863 | `	/* Point to the frame that trigger the exception */` |
|      449 |  7864 | `	pFrameLocal = pFrameLocal->pParent;` |
|      449 |  7865 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      449 |  7866 | `	pException->pFrame = pFrameLocal;` |
|      449 |  7867 | `	break;` |
|        - |  7868 | `							}` |
|        - |  7869 | `/*` |
|        - |  7870 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7871 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7872 | ` */` |
|      216 |  7873 | `case PH7_OP_POP_EXCEPTION: {` |
|      437 |  7874 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      437 |  7875 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7876 | `		ph7_exception **apException;` |
|        - |  7877 | `		/* Pop the loaded exception */` |
|       36 |  7878 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       36 |  7879 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       32 |  7880 | `			(void)SySetPop(&pVm->aException);` |
|       15 |  7881 | `		}` |
|       17 |  7882 | `	}` |
|      437 |  7883 | `	pException->pFrame = 0;` |
|        - |  7884 | `	/* Leave the exception frame */` |
|      437 |  7885 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7886 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      437 |  7887 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7888 | `		sxi32 rcFinally;` |
|       22 |  7889 | `		pException->iFinallyDone = 1;` |
|       22 |  7890 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|       22 |  7891 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7892 | `			goto Abort;` |
|        - |  7893 | `		}` |
|       10 |  7894 | `	}` |
|      437 |  7895 | `	if( pVm->bReturnRequested ){` |
|        - |  7896 | ``		/* `return` inside the finally (normal try completion) returns from the`` |
|        - |  7897 | `		 * function. Drain outer finally blocks first, then — only in the real` |
|        - |  7898 | `		 * function body — materialize; inside a mini-program propagate outward. */` |
|       29 |  7899 | `		rc = VmDrainFinally(&(*pVm),nExceptionBase);` |
|       29 |  7900 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7901 | `			goto Abort;` |
|        - |  7902 | `		}` |
|       29 |  7903 | `		if( !bReturnPropagates ){` |
|       27 |  7904 | `			VmMaterializeCatchReturn(&(*pVm),pResult,pEntryFrame);` |
|       13 |  7905 | `		}` |
|       29 |  7906 | `		goto Done;` |
|        - |  7907 | `	}` |
|      409 |  7908 | `	break;` |
|        - |  7909 | `							}` |
|        - |  7910 |  |
|        - |  7911 | `/*` |
|        - |  7912 | ` * OP_THROW * P2 *` |
|        - |  7913 | ` * Throw an user exception.` |
|        - |  7914 | ` */` |
|      104 |  7915 | `case PH7_OP_THROW: {` |
|      213 |  7916 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      213 |  7917 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7918 | `#ifdef UNTRUST` |
|        - |  7919 | `	if( pTos < pStack ){` |
|        - |  7920 | `		goto Abort;` |
|        - |  7921 | `	}` |
|        - |  7922 | `#endif` |
|      213 |  7923 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7924 | `	/* Tell the upper layer that an exception was thrown */` |
|      213 |  7925 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      213 |  7926 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      213 |  7927 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7928 | `		ph7_class *pThrowable;` |
|        - |  7929 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      213 |  7930 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      214 |  7931 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7932 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7933 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7934 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7935 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7936 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7937 | `			if( pErrorClass ){` |
|        3 |  7938 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7939 | `			}` |
|        3 |  7940 | `			if( pErrInst ){` |
|        - |  7941 | `				ph7_class_method *pCons;` |
|        3 |  7942 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7943 | `				if( pCons ){` |
|        - |  7944 | `					ph7_value sArg;` |
|        - |  7945 | `					ph7_value *apArg[1];` |
|        - |  7946 | `					SyString sMsgStr;` |
|        - |  7947 | `					static const char zErrMsg[] =` |
|        - |  7948 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7949 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7950 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7951 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7952 | `					apArg[0] = &sArg;` |
|        3 |  7953 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7954 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7955 | `				}` |
|        3 |  7956 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7957 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7958 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7959 | `					goto Abort;` |
|        - |  7960 | `				}` |
|        2 |  7961 | `			}else{` |
|        - |  7962 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7963 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7964 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7965 | `					goto Abort;` |
|        - |  7966 | `				}` |
|        - |  7967 | `			}` |
|        2 |  7968 | `		}else{` |
|        - |  7969 | `			/* Throw the exception */` |
|      211 |  7970 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      211 |  7971 | `			if( rc == SXERR_ABORT ){` |
|        - |  7972 | `				/* Abort processing immediately */` |
|       14 |  7973 | `				goto Abort;` |
|        - |  7974 | `			}` |
|        - |  7975 | `		}` |
|      104 |  7976 | `	}else{` |
|        - |  7977 | `		/* Expecting a class instance */` |
|      ! 0 |  7978 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7979 | `		if( rc == SXERR_ABORT ){` |
|        - |  7980 | `			/* Abort processing immediately */` |
|      ! 0 |  7981 | `			goto Abort;` |
|        - |  7982 | `		}` |
|        - |  7983 | `	}` |
|        - |  7984 | `	/* Pop the top entry */` |
|      203 |  7985 | `	VmPopOperand(&pTos,1);` |
|        - |  7986 | `	/* Perform an unconditional jump to the try's OP_POP_EXCEPTION landing pad,` |
|        - |  7987 | `	 * which tears down the try frame, runs finally, and (when a catch/finally` |
|        - |  7988 | ``	 * issued a `return`) consumes pVm->bReturnRequested. Routing the return`` |
|        - |  7989 | `	 * through OP_POP_EXCEPTION keeps the frame stack balanced. */` |
|      203 |  7990 | `	pc = nJump - 1;` |
|      203 |  7991 | `	break;` |
|        - |  7992 | `				   }` |
|        - |  7993 | `/*` |
|        - |  7994 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7995 | ` * Prepare a foreach step.` |
|        - |  7996 | ` */` |
|     6258 |  7997 | `case PH7_OP_FOREACH_INIT: {` |
|    12521 |  7998 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7999 | `	void *pName;` |
|        - |  8000 | `#ifdef UNTRUST` |
|        - |  8001 | `	if( pTos < pStack ){` |
|        - |  8002 | `		goto Abort;` |
|        - |  8003 | `	}` |
|        - |  8004 | `#endif` |
|    12521 |  8005 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  8006 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  8007 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8008 | `			/* Force a string cast */` |
|      ! 0 |  8009 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  8010 | `		}` |
|        - |  8011 | `		/* Duplicate name */` |
|      ! 0 |  8012 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  8013 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8014 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8015 | `		}` |
|      ! 0 |  8016 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  8017 | `	}` |
|    12521 |  8018 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  8019 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  8020 | `			/* Force a string cast */` |
|      ! 0 |  8021 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  8022 | `		}` |
|        - |  8023 | `		/* Duplicate name */` |
|      ! 0 |  8024 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  8025 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8026 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  8027 | `		}` |
|      ! 0 |  8028 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  8029 | `	}` |
|        - |  8030 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12521 |  8031 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  8032 | `		/* Jump out of the loop */` |
|      ! 0 |  8033 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8034 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  8035 | `		}` |
|      ! 0 |  8036 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  8037 | `	}else{` |
|        - |  8038 | `		ph7_foreach_step *pStep;` |
|    12521 |  8039 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12521 |  8040 | `		if( pStep == 0 ){` |
|      ! 0 |  8041 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  8042 | `			/* Jump out of the loop */` |
|      ! 0 |  8043 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  8044 | `		}else{` |
|        - |  8045 | `			/* Zero the structure */` |
|    12521 |  8046 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  8047 | `			/* Prepare the step */` |
|    12521 |  8048 | `			pStep->iFlags = pInfo->iFlags;` |
|    12521 |  8049 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8050 | `				ph7_hashmap *pMap;` |
|        - |  8051 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  8052 | `				 * source array so mutations don't affect other sharers. */` |
|    12487 |  8053 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  8054 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  8055 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  8056 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  8057 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  8058 | `						 * variable still points at the same hashmap as` |
|        - |  8059 | `						 * the stack value. */` |
|        9 |  8060 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  8061 | `							pCur->iRef--;` |
|        - |  8062 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  8063 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  8064 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  8065 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  8066 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  8067 | `						}` |
|        4 |  8068 | `					}` |
|        4 |  8069 | `				}` |
|    12487 |  8070 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  8071 | `				/* Reset the internal loop cursor */` |
|    12487 |  8072 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8073 | `				/* Mark the step */` |
|    12487 |  8074 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12487 |  8075 | `				pStep->xIter.pMap = pMap;` |
|    12487 |  8076 | `				pMap->iRef++;` |
|     6246 |  8077 | `			}else{` |
|       39 |  8078 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8079 | `				ph7_class *pIteratorClass;` |
|        - |  8080 | `				/* Check if the object implements Iterator */` |
|       39 |  8081 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       50 |  8082 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  8083 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  8084 | `					ph7_class_method *pRewind;` |
|       26 |  8085 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       26 |  8086 | `					pStep->xIter.pThis = pThis;` |
|       26 |  8087 | `					pThis->iRef++;` |
|       26 |  8088 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       26 |  8089 | `					if( pRewind ){` |
|       26 |  8090 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  8091 | `					}` |
|       15 |  8092 | `				}else{` |
|        - |  8093 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  8094 | `					ph7_class *pIterAggClass;` |
|       14 |  8095 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  8096 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  8097 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  8098 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  8099 | `						ph7_class_method *pGetIter;` |
|        3 |  8100 | `						int iterAggOk = 0;` |
|        3 |  8101 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  8102 | `						if( pGetIter ){` |
|        - |  8103 | `							ph7_value sResult;` |
|        3 |  8104 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  8105 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  8106 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  8107 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  8108 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  8109 | `									ph7_class_method *pRewind;` |
|        3 |  8110 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  8111 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  8112 | `									pIterObj->iRef++;` |
|        - |  8113 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  8114 | `									pStep->pOwner = pThis;` |
|        3 |  8115 | `									pThis->iRef++;` |
|        3 |  8116 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  8117 | `									if( pRewind ){` |
|        3 |  8118 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  8119 | `									}` |
|        3 |  8120 | `									iterAggOk = 1;` |
|        1 |  8121 | `								}` |
|        1 |  8122 | `							}` |
|        3 |  8123 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  8124 | `						}` |
|        3 |  8125 | `						if( !iterAggOk ){` |
|        - |  8126 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  8127 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8128 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  8129 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  8130 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  8131 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  8132 | `						}` |
|        2 |  8133 | `					}else{` |
|        - |  8134 | `						/* Plain object iteration via hAttr */` |
|       12 |  8135 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  8136 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  8137 | `						pStep->xIter.pThis = pThis;` |
|       12 |  8138 | `						pThis->iRef++;` |
|        - |  8139 | `					}` |
|        - |  8140 | `				}` |
|        - |  8141 | `			}` |
|        - |  8142 | `		}` |
|    12521 |  8143 | `		if( pStep ){` |
|    12521 |  8144 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  8145 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  8146 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  8147 | `				/* Jump out of the loop */` |
|      ! 0 |  8148 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  8149 | `			}` |
|     6258 |  8150 | `		}` |
|        - |  8151 | `	}` |
|    12521 |  8152 | `	VmPopOperand(&pTos,1);` |
|    12521 |  8153 | `	break;` |
|        - |  8154 | `						  }` |
|        - |  8155 | `/*` |
|        - |  8156 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  8157 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  8158 | ` */` |
|   102968 |  8159 | `case PH7_OP_FOREACH_STEP: {` |
|   205941 |  8160 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  8161 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  8162 | `	ph7_value *pValue;` |
|        - |  8163 | `	VmFrame *pFrameLocal;` |
|        - |  8164 | `	/* Peek the last step */` |
|   205941 |  8165 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   205941 |  8166 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   205941 |  8167 | `	pFrameLocal = pVm->pFrame;` |
|   205941 |  8168 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   205941 |  8169 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   205807 |  8170 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  8171 | `		ph7_hashmap_node *pNode;` |
|        - |  8172 | `		/* Extract the current node value */` |
|   205807 |  8173 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   205807 |  8174 | `		if( pNode == 0 ){` |
|        - |  8175 | `			/* No more entry to process */` |
|    12485 |  8176 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12485 |  8177 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8178 | `				/* Break the reference with the last element */` |
|        7 |  8179 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  8180 | `			}` |
|        - |  8181 | `			/* Automatically reset the loop cursor */` |
|    12485 |  8182 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  8183 | `			/* Cleanup the mess left behind */` |
|    12485 |  8184 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12485 |  8185 | `			SySetPop(&pInfo->aStep);` |
|    12485 |  8186 | `			PH7_HashmapUnref(pMap);` |
|     6245 |  8187 | `		}else{` |
|   193327 |  8188 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      531 |  8189 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      531 |  8190 | `				if( pKey ){` |
|      531 |  8191 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  8192 | `				}` |
|      263 |  8193 | `			}` |
|   193327 |  8194 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8195 | `				SyHashEntry *pEntry;` |
|        - |  8196 | `				/* Pass by reference */` |
|       23 |  8197 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  8198 | `				if( pEntry ){` |
|       21 |  8199 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  8200 | `				}else{` |
|        4 |  8201 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  8202 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  8203 | `				}` |
|       12 |  8204 | `			}else{` |
|        - |  8205 | `				/* Make a copy of the entry value */` |
|   193305 |  8206 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   193305 |  8207 | `				if( pValue ){` |
|   193305 |  8208 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    96650 |  8209 | `				}` |
|        - |  8210 | `			}` |
|        5 |  8211 | `		}` |
|   103040 |  8212 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  8213 | `		/* Iterator-based iteration.` |
|        - |  8214 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  8215 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  8216 | `		 */` |
|      109 |  8217 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  8218 | `		ph7_class_method *pMethod;` |
|        - |  8219 | `		ph7_value sResult;` |
|      109 |  8220 | `		int isValid = 0;` |
|        - |  8221 | `		/* Call next() to advance — but skip on the first iteration */` |
|      109 |  8222 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       29 |  8223 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       17 |  8224 | `		}else{` |
|       85 |  8225 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       85 |  8226 | `			if( pMethod ){` |
|       85 |  8227 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  8228 | `			}` |
|        - |  8229 | `		}` |
|        - |  8230 | `		/* Call valid() */` |
|      109 |  8231 | `		PH7_MemObjInit(pVm,&sResult);` |
|      109 |  8232 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      109 |  8233 | `		if( pMethod ){` |
|      109 |  8234 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      109 |  8235 | `			PH7_MemObjToBool(&sResult);` |
|      109 |  8236 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  8237 | `		}` |
|      109 |  8238 | `		PH7_MemObjRelease(&sResult);` |
|      109 |  8239 | `		if( !isValid ){` |
|        - |  8240 | `			/* Iterator exhausted */` |
|       27 |  8241 | `			pc = pInstr->iP2 - 1;` |
|        - |  8242 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       27 |  8243 | `			if( pStep->pOwner ){` |
|        3 |  8244 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  8245 | `			}` |
|       27 |  8246 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       27 |  8247 | `			SySetPop(&pInfo->aStep);` |
|       27 |  8248 | `			PH7_ClassInstanceUnref(pThis);` |
|       16 |  8249 | `		}else{` |
|        - |  8250 | `			/* Call current() to get value */` |
|       87 |  8251 | `			PH7_MemObjInit(pVm,&sResult);` |
|       87 |  8252 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       87 |  8253 | `			if( pMethod ){` |
|       87 |  8254 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  8255 | `			}` |
|       87 |  8256 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       87 |  8257 | `			if( pValue ){` |
|       87 |  8258 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  8259 | `			}` |
|       87 |  8260 | `			PH7_MemObjRelease(&sResult);` |
|        - |  8261 | `			/* Call key() if needed */` |
|       87 |  8262 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  8263 | `				ph7_value sKey;` |
|       37 |  8264 | `				PH7_MemObjInit(pVm,&sKey);` |
|       37 |  8265 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       37 |  8266 | `				if( pMethod ){` |
|       37 |  8267 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  8268 | `				}` |
|       37 |  8269 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       37 |  8270 | `				if( pValue ){` |
|       37 |  8271 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  8272 | `				}` |
|       37 |  8273 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  8274 | `			}` |
|        - |  8275 | `		}` |
|       57 |  8276 | `	}else{` |
|       32 |  8277 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  8278 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  8279 | `		SyHashEntry *pEntry;` |
|        - |  8280 | `		/* Point to the next attribute */` |
|       36 |  8281 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  8282 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  8283 | `			/* Check access permission */` |
|       38 |  8284 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  8285 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  8286 | `					break; /* Access is granted */` |
|        - |  8287 | `			}` |
|        1 |  8288 | `		}` |
|       32 |  8289 | `		if( pEntry == 0 ){` |
|        - |  8290 | `			/* Clean up the mess left behind */` |
|       12 |  8291 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  8292 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8293 | `				/* Break the reference with the last element */` |
|        3 |  8294 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  8295 | `			}` |
|       12 |  8296 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  8297 | `			SySetPop(&pInfo->aStep);` |
|       12 |  8298 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  8299 | `		}else{` |
|       22 |  8300 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  8301 | `			ph7_value *pAttrValue;` |
|       22 |  8302 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  8303 | `				/* Fill with the current attribute name */` |
|       22 |  8304 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8305 | `				if( pKey ){` |
|       22 |  8306 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8307 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8308 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8309 | `				}` |
|       10 |  8310 | `			}` |
|        - |  8311 | `			/* Extract attribute value */` |
|       22 |  8312 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8313 | `			if( pAttrValue ){` |
|       22 |  8314 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8315 | `					/* Pass by reference */` |
|        3 |  8316 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8317 | `					if( pEntry ){` |
|        3 |  8318 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8319 | `					}else{` |
|      ! 0 |  8320 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8321 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8322 | `					}` |
|        2 |  8323 | `				}else{` |
|        - |  8324 | `					/* Make a copy of the attribute value */` |
|       20 |  8325 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8326 | `					if( pValue ){` |
|       20 |  8327 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8328 | `					}` |
|        - |  8329 | `				}` |
|       10 |  8330 | `			}` |
|        - |  8331 | `		}` |
|        - |  8332 | `	}` |
|   205941 |  8333 | `	break;` |
|        - |  8334 | `						  }` |
|        - |  8335 | `/*` |
|        - |  8336 | ` * OP_MEMBER P1 P2` |
|        - |  8337 | ` * Load class attribute/method on the stack.` |
|        - |  8338 | ` */` |
|     4281 |  8339 | `case PH7_OP_MEMBER: {` |
|        - |  8340 | `	ph7_class_instance *pThis;` |
|        - |  8341 | `	ph7_value *pNos;` |
|        - |  8342 | `	SyString sName;` |
|     8567 |  8343 | `	if( !pInstr->iP1 ){` |
|     8327 |  8344 | `		pNos = &pTos[-1];` |
|        - |  8345 | `#ifdef UNTRUST` |
|        - |  8346 | `		if( pNos < pStack ){` |
|        - |  8347 | `			goto Abort;` |
|        - |  8348 | `		}` |
|        - |  8349 | `#endif` |
|     8327 |  8350 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8351 | `			ph7_class *pClass;` |
|        - |  8352 | `			/* Class already instantiated */` |
|     8325 |  8353 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8354 | `			/* Point to the instantiated class */` |
|     8325 |  8355 | `			pClass = pThis->pClass;` |
|        - |  8356 | `			/* Extract attribute name first */` |
|     8325 |  8357 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     8325 |  8358 | `			if( pInstr->iP2 ){` |
|        - |  8359 | `				/* Method call */` |
|      805 |  8360 | `				ph7_class_method *pMeth = 0;` |
|      805 |  8361 | `				if( sName.nByte > 0 ){` |
|        - |  8362 | `					/* Extract the target method */` |
|      805 |  8363 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      400 |  8364 | `				}` |
|      805 |  8365 | `				if( pMeth == 0 ){` |
|      ! 0 |  8366 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8367 | `						&pClass->sName,&sName` |
|        - |  8368 | `						);` |
|        - |  8369 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8370 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8371 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8372 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8373 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8374 | `				}else{` |
|        - |  8375 | `					/* Push method name on the stack */` |
|      805 |  8376 | `					PH7_MemObjRelease(pTos);` |
|      805 |  8377 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      805 |  8378 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8379 | `				}` |
|      805 |  8380 | `				pTos->nIdx = SXU32_HIGH;` |
|      405 |  8381 | `			}else{` |
|        - |  8382 | `				/* Attribute access */` |
|     7525 |  8383 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8384 | `				SyHashEntry *pEntry;` |
|        - |  8385 | `				/* Extract the target attribute */` |
|     7525 |  8386 | `				if( sName.nByte > 0 ){` |
|     7525 |  8387 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7525 |  8388 | `					if( pEntry ){` |
|        - |  8389 | `						/* Point to the attribute value */` |
|     7523 |  8390 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3759 |  8391 | `					}` |
|     3760 |  8392 | `				}` |
|     7525 |  8393 | `				if( pObjAttr == 0 ){` |
|        - |  8394 | `					/* No such attribute,load null */` |
|        4 |  8395 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8396 | `						&pClass->sName,&sName);` |
|        - |  8397 | `					/* Call the __get magic method if available */` |
|        3 |  8398 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8399 | `				}` |
|     7525 |  8400 | `				VmPopOperand(&pTos,1);` |
|        - |  8401 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8402 | `				 * This is due to the following case:` |
|        - |  8403 | `				 *     (new TestClass())->foo;` |
|        - |  8404 | `				 */` |
|     7525 |  8405 | `				pThis->iRef++;` |
|     7525 |  8406 | `				PH7_MemObjRelease(pTos);` |
|     7525 |  8407 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7525 |  8408 | `				if( pObjAttr ){` |
|     7523 |  8409 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8410 | `					/* Check attribute access */` |
|     7523 |  8411 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8412 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8413 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8414 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8415 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8416 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7518 |  8417 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3804 |  8418 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       84 |  8419 | `							VmInstr *pNext = pInstr + 1;` |
|       84 |  8420 | `							int bIsLhs = 0;` |
|       84 |  8421 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       82 |  8422 | `								bIsLhs = 1;` |
|       39 |  8423 | `							}` |
|       84 |  8424 | `							if( !bIsLhs ){` |
|        3 |  8425 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8426 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8427 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8428 | `									goto Abort;` |
|        - |  8429 | `								}` |
|        - |  8430 | `								{` |
|        3 |  8431 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8432 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8433 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4281 |  8434 | `										break;` |
|        - |  8435 | `									}` |
|        - |  8436 | `								}` |
|      ! 0 |  8437 | `								goto Exception;` |
|        - |  8438 | `							}` |
|       39 |  8439 | `						}` |
|        - |  8440 | `						/* Load attribute */` |
|     7521 |  8441 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7521 |  8442 | `						if( pValue ){` |
|     7521 |  8443 | `							if( pThis->iRef < 2 ){` |
|        - |  8444 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8445 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8446 | `								 */` |
|        7 |  8447 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8448 | `							}else{` |
|        - |  8449 | `								/* Simple load */` |
|     7515 |  8450 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8451 | `							}` |
|     7521 |  8452 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7519 |  8453 | `								if( pThis->iRef > 1 ){` |
|        - |  8454 | `									/* Load attribute index */` |
|     7513 |  8455 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3754 |  8456 | `								}` |
|     3757 |  8457 | `							}` |
|     3758 |  8458 | `						}` |
|     3763 |  8459 | `					}else{` |
|        - |  8460 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8461 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8462 | `						char zMsg[256];` |
|      ! 0 |  8463 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8464 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8465 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8466 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8467 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8468 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8469 | `						goto Abort;` |
|        - |  8470 | `					}` |
|     3758 |  8471 | `				}` |
|        - |  8472 | `				/* Safely unreference the object */` |
|     7523 |  8473 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8474 | `			}` |
|     4164 |  8475 | `		}else{` |
|        3 |  8476 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8477 | `			VmPopOperand(&pTos,1);` |
|        3 |  8478 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8479 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8480 | `		}` |
|     4165 |  8481 | `	}else{` |
|        - |  8482 | `		/* Static member access using class name */` |
|      245 |  8483 | `		pNos = pTos;` |
|      245 |  8484 | `		pThis = 0;` |
|      245 |  8485 | `		if( !pInstr->p3 ){` |
|      195 |  8486 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      195 |  8487 | `			pNos--;` |
|        - |  8488 | `#ifdef UNTRUST` |
|        - |  8489 | `			if( pNos < pStack ){` |
|        - |  8490 | `				goto Abort;` |
|        - |  8491 | `			}` |
|        - |  8492 | `#endif` |
|      100 |  8493 | `		}else{` |
|        - |  8494 | `			/* Attribute name already computed */` |
|       54 |  8495 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8496 | `		}` |
|      245 |  8497 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      245 |  8498 | `			ph7_class *pClass = 0;` |
|      245 |  8499 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8500 | `				/* Class already instantiated */` |
|        5 |  8501 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8502 | `				pClass = pThis->pClass;` |
|        5 |  8503 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8504 | `			}else{` |
|        - |  8505 | `				/* Try to extract the target class */` |
|      241 |  8506 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      241 |  8507 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      241 |  8508 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8509 | `					/* Handle self/static/parent keywords */` |
|      241 |  8510 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       65 |  8511 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       65 |  8512 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8513 | `							/* In a trait method, self:: resolves to the using class */` |
|       14 |  8514 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       11 |  8515 | `						}` |
|      211 |  8516 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       29 |  8517 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      181 |  8518 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       30 |  8519 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  8520 | `						if( pSelf && pSelf->pBase ){` |
|       30 |  8521 | `							pClass = pSelf->pBase;` |
|       13 |  8522 | `						}` |
|       17 |  8523 | `					}else{` |
|      129 |  8524 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8525 | `					}` |
|      118 |  8526 | `				}` |
|        - |  8527 | `			}` |
|      245 |  8528 | `			if( pClass == 0 ){` |
|        - |  8529 | `				/* Undefined class */` |
|      ! 0 |  8530 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8531 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8532 | `					);` |
|      ! 0 |  8533 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8534 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8535 | `				}` |
|      ! 0 |  8536 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8537 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8538 | `			}else{` |
|      245 |  8539 | `				if( pInstr->iP2 ){` |
|        - |  8540 | `					/* Method call */` |
|       89 |  8541 | `					ph7_class_method *pMeth = 0;` |
|       89 |  8542 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8543 | `						/* Extract the target method */` |
|       89 |  8544 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8545 | `					}` |
|       89 |  8546 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8547 | `						if( pMeth ){` |
|      ! 0 |  8548 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8549 | `								&pClass->sName,&sName` |
|        - |  8550 | `								);` |
|      ! 0 |  8551 | `						}else{` |
|      ! 0 |  8552 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8553 | `								&pClass->sName,&sName` |
|        - |  8554 | `								);` |
|        - |  8555 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8556 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8557 | `						}` |
|        - |  8558 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8559 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8560 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8561 | `						}` |
|      ! 0 |  8562 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8563 | `					}else{` |
|        - |  8564 | `						/* Push method name on the stack */` |
|       89 |  8565 | `						PH7_MemObjRelease(pTos);` |
|       89 |  8566 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       89 |  8567 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8568 | `					}` |
|       89 |  8569 | `					pTos->nIdx = SXU32_HIGH;` |
|       47 |  8570 | `				}else{` |
|        - |  8571 | `					/* Attribute access */` |
|      161 |  8572 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8573 | `					/* Check for special ::class pseudo-constant */` |
|      207 |  8574 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8575 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8576 | `						/* ::class returns the fully qualified class name */` |
|        - |  8577 | `						/* Pop the attribute name from the stack */` |
|       62 |  8578 | `						if( !pInstr->p3 ){` |
|       62 |  8579 | `							VmPopOperand(&pTos,1);` |
|       29 |  8580 | `						}` |
|       62 |  8581 | `						PH7_MemObjRelease(pTos);` |
|        - |  8582 | `						/* Load the class name */` |
|       62 |  8583 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       62 |  8584 | `						pTos->nIdx = SXU32_HIGH;` |
|       33 |  8585 | `					}else{` |
|        - |  8586 | `						/* Extract the target attribute */` |
|      103 |  8587 | `						if( sName.nByte > 0 ){` |
|      103 |  8588 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8589 | `						}` |
|      103 |  8590 | `						if( pAttr == 0 ){` |
|        - |  8591 | `							/* No such attribute,load null */` |
|      ! 0 |  8592 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8593 | `								&pClass->sName,&sName);` |
|        - |  8594 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8595 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8596 | `						}` |
|        - |  8597 | `						/* Pop the attribute name from the stack */` |
|      103 |  8598 | `						if( !pInstr->p3 ){` |
|       51 |  8599 | `							VmPopOperand(&pTos,1);` |
|       24 |  8600 | `						}` |
|      103 |  8601 | `						PH7_MemObjRelease(pTos);` |
|      103 |  8602 | `						pTos->nIdx = SXU32_HIGH;` |
|      103 |  8603 | `						if( pAttr ){` |
|      103 |  8604 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8605 | `								/* Access to a non static attribute */` |
|      ! 0 |  8606 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8607 | `									&pClass->sName,&pAttr->sName` |
|        - |  8608 | `									);` |
|      ! 0 |  8609 | `							}else{` |
|        - |  8610 | `								ph7_value *pValue;` |
|        - |  8611 | `								/* Check if the access to the attribute is allowed */` |
|      103 |  8612 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8613 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8614 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8615 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       71 |  8616 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       60 |  8617 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8618 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       41 |  8619 | `										if( pS ){` |
|       41 |  8620 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       41 |  8621 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8622 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8623 | `												int bIsLhs = 0;` |
|        8 |  8624 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8625 | `													bIsLhs = 1;` |
|        2 |  8626 | `												}` |
|        8 |  8627 | `												if( !bIsLhs ){` |
|        3 |  8628 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8629 | `													if( pThis ){` |
|      ! 0 |  8630 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8631 | `													}` |
|        3 |  8632 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8633 | `														goto Abort;` |
|        - |  8634 | `													}` |
|        - |  8635 | `													{` |
|        3 |  8636 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8637 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8638 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8639 | `															break;` |
|        - |  8640 | `														}` |
|        - |  8641 | `													}` |
|      ! 0 |  8642 | `													goto Exception;` |
|        - |  8643 | `												}` |
|        2 |  8644 | `											}` |
|       18 |  8645 | `										}` |
|       18 |  8646 | `									}` |
|        - |  8647 | `									/* Load the desired attribute */` |
|       97 |  8648 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       97 |  8649 | `									if( pValue ){` |
|       97 |  8650 | `										PH7_MemObjLoad(pValue,pTos);` |
|       97 |  8651 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8652 | `											/* Load index number */` |
|       52 |  8653 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8654 | `										}` |
|       46 |  8655 | `									}` |
|       51 |  8656 | `								}else{` |
|        - |  8657 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8658 | `									char zMsg[256];` |
|        6 |  8659 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        6 |  8660 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        8 |  8661 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8662 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8663 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        4 |  8664 | `									}else{` |
|      ! 0 |  8665 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8666 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8667 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8668 | `									}` |
|        6 |  8669 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        6 |  8670 | `									goto Abort;` |
|        - |  8671 | `								}` |
|        - |  8672 | `							}` |
|       46 |  8673 | `						}` |
|        - |  8674 | `					}` |
|        - |  8675 | `				}` |
|      239 |  8676 | `				if( pThis ){` |
|        - |  8677 | `					/* Safely unreference the object */` |
|        5 |  8678 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8679 | `				}` |
|        - |  8680 | `			}` |
|      122 |  8681 | `		}else{` |
|        - |  8682 | `			/* Pop operands */` |
|      ! 0 |  8683 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8684 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8685 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8686 | `			}` |
|      ! 0 |  8687 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8688 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8689 | `		}` |
|        - |  8690 | `	}` |
|     8559 |  8691 | `	break;` |
|        - |  8692 | `					}` |
|        - |  8693 | `/*` |
|        - |  8694 | ` * OP_NEW P1 * * *` |
|        - |  8695 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8696 | ` */` |
|      700 |  8697 | `case PH7_OP_NEW: {` |
|     1405 |  8698 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1405 |  8699 | `	ph7_class *pClass = 0;` |
|        - |  8700 | `	ph7_class_instance *pNew;` |
|     1405 |  8701 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8702 | `		/* Try to extract the desired class */` |
|     2105 |  8703 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1400 |  8704 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      700 |  8705 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8706 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8707 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8708 | `	}` |
|     1405 |  8709 | `	if( pClass == 0 ){` |
|        - |  8710 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8711 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8712 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8713 | `			);` |
|        - |  8714 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8715 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8716 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8717 | `			/* Pop given arguments */` |
|      ! 0 |  8718 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8719 | `		}` |
|      ! 0 |  8720 | `		goto Abort;` |
|      ! 0 |  8721 | `	}else{` |
|        - |  8722 | `		ph7_class_method *pCons;` |
|        - |  8723 | `		/* Create a new class instance */` |
|     1405 |  8724 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1405 |  8725 | `		if( pNew == 0 ){` |
|      ! 0 |  8726 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8727 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8728 | `				&pClass->sName` |
|        - |  8729 | `			);` |
|      ! 0 |  8730 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8731 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8732 | `				/* Pop given arguments */` |
|      ! 0 |  8733 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8734 | `			}` |
|      ! 0 |  8735 | `			break;` |
|        - |  8736 | `		}` |
|        - |  8737 | `		/* Check if a constructor is available */` |
|     1405 |  8738 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1405 |  8739 | `		if( pCons == 0 ){` |
|      955 |  8740 | `			SyString *pName = &pClass->sName;` |
|        - |  8741 | `			/* Check for a constructor with the same base class name */` |
|      955 |  8742 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      475 |  8743 | `		}` |
|     1405 |  8744 | `		if( pCons ){` |
|        - |  8745 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8746 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8747 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8748 | `			 * (including variadic string-key packing). */` |
|      455 |  8749 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8750 | `			sxi32 rcCons;` |
|      455 |  8751 | `			SySetReset(&aArg);` |
|      883 |  8752 | `			while( pArg < pTos ){` |
|      433 |  8753 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      433 |  8754 | `				pArg++;` |
|        5 |  8755 | `			}` |
|      455 |  8756 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8757 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8758 | `				sxu32 n;` |
|      121 |  8759 | `				n = SySetUsed(&aArg);` |
|        - |  8760 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8761 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8762 | `				 * after resolution). */` |
|      237 |  8763 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      121 |  8764 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      121 |  8765 | `					if( pFuncArg ){` |
|      121 |  8766 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        8 |  8767 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8768 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8769 | `						}` |
|       58 |  8770 | `					}` |
|      121 |  8771 | `					n++;` |
|        5 |  8772 | `				}` |
|       58 |  8773 | `			}` |
|      455 |  8774 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8775 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      455 |  8776 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8777 | `				pNew->iRef = 1;` |
|      ! 0 |  8778 | `			}` |
|      455 |  8779 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8780 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8781 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8782 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8783 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8784 | `				sxi32 iResumePc;` |
|        5 |  8785 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8786 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8787 | `					goto Abort;` |
|        - |  8788 | `				}` |
|        5 |  8789 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8790 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8791 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8792 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8793 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8794 | `					}` |
|        5 |  8795 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8796 | `					pc = iResumePc;` |
|        5 |  8797 | `					break;` |
|        - |  8798 | `				}` |
|      ! 0 |  8799 | `				goto Exception;` |
|        - |  8800 | `			}` |
|      223 |  8801 | `		}` |
|     1401 |  8802 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8803 | `			/* Pop given arguments */` |
|      363 |  8804 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      179 |  8805 | `		}` |
|     1401 |  8806 | `		PH7_MemObjRelease(pTos);` |
|     1401 |  8807 | `		pTos->x.pOther = pNew;` |
|     1401 |  8808 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8809 | `	}` |
|     1401 |  8810 | `	break;` |
|        - |  8811 | `				 }` |
|        - |  8812 | `/*` |
|        - |  8813 | ` * OP_CLONE * * *` |
|        - |  8814 | ` * Perfome a clone operation.` |
|        - |  8815 | ` */` |
|       24 |  8816 | `case PH7_OP_CLONE: {` |
|        - |  8817 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8818 | `#ifdef UNTRUST` |
|        - |  8819 | `	if( pTos < pStack ){` |
|        - |  8820 | `		goto Abort;` |
|        - |  8821 | `	}` |
|        - |  8822 | `#endif` |
|        - |  8823 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8824 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8825 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8826 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8827 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8828 | `		break;` |
|        - |  8829 | `	}` |
|        - |  8830 | `	/* Point to the source */` |
|       46 |  8831 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8832 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8833 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8834 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8835 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8836 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8837 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8838 | `		break;` |
|        - |  8839 | `	}` |
|        - |  8840 | `	/* Perform the clone operation */` |
|       46 |  8841 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8842 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8843 | `	if( pClone == 0 ){` |
|      ! 0 |  8844 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8845 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8846 | `	}else{` |
|        - |  8847 | `		/* Load the cloned object */` |
|       46 |  8848 | `		pTos->x.pOther = pClone;` |
|       46 |  8849 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8850 | `	}` |
|       46 |  8851 | `	break;` |
|        - |  8852 | `				   }` |
|        - |  8853 | `/*` |
|        - |  8854 | ` * OP_SWITCH * * P3` |
|        - |  8855 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8856 | ` */` |
|       26 |  8857 | `case PH7_OP_SWITCH: {` |
|       57 |  8858 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8859 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8860 | `	ph7_value sValue,sCaseValue;` |
|        - |  8861 | `	sxu32 n,nEntry;` |
|        - |  8862 | `#ifdef UNTRUST` |
|        - |  8863 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8864 | `		goto Abort;` |
|        - |  8865 | `	}` |
|        - |  8866 | `#endif` |
|        - |  8867 | `	/* Point to the case table  */` |
|       57 |  8868 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       57 |  8869 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8870 | `	/* Select the appropriate case block to execute */` |
|       57 |  8871 | `	PH7_MemObjInit(pVm,&sValue);` |
|       57 |  8872 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      135 |  8873 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      133 |  8874 | `		pCase = &aCase[n];` |
|      133 |  8875 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8876 | `		/* Execute the case expression first */` |
|      133 |  8877 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|        - |  8878 | `		/* Compare the two expression */` |
|      133 |  8879 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      133 |  8880 | `		PH7_MemObjRelease(&sValue);` |
|      133 |  8881 | `		PH7_MemObjRelease(&sCaseValue);` |
|      133 |  8882 | `		if( rc == 0 ){` |
|        - |  8883 | `			/* Value match,jump to this block */` |
|       55 |  8884 | `			pc = pCase->nStart - 1;` |
|       55 |  8885 | `			break;` |
|        - |  8886 | `		}` |
|       44 |  8887 | `	}` |
|       57 |  8888 | `	VmPopOperand(&pTos,1);` |
|       57 |  8889 | `	if( n >= nEntry ){` |
|        - |  8890 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8891 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8892 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8893 | `		}else{` |
|        - |  8894 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8895 | `			pc = pSwitch->nOut - 1;` |
|        - |  8896 | `		}` |
|        1 |  8897 | `	}` |
|       57 |  8898 | `	break;` |
|        - |  8899 | `					}` |
|        - |  8900 | `/*` |
|        - |  8901 | ` * OP_MATCH * * P3` |
|        - |  8902 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8903 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8904 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8905 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8906 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8907 | ` */` |
|       54 |  8908 | `case PH7_OP_MATCH: {` |
|      111 |  8909 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      111 |  8910 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8911 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8912 | `	sxu32 i,j,nArm,nCond;` |
|      111 |  8913 | `	int matched = 0;` |
|        - |  8914 | `#ifdef UNTRUST` |
|        - |  8915 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8916 | `		goto Abort;` |
|        - |  8917 | `	}` |
|        - |  8918 | `#endif` |
|      111 |  8919 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      111 |  8920 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      111 |  8921 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      111 |  8922 | `	PH7_MemObjInit(pVm,&sCond);` |
|      111 |  8923 | `	PH7_MemObjInit(pVm,&sResult);` |
|      111 |  8924 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      349 |  8925 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8926 | `		pArm = &aArm[i];` |
|      240 |  8927 | `		if( pArm->bDefault ){` |
|       13 |  8928 | `			pDefault = pArm;` |
|       13 |  8929 | `			continue;` |
|        - |  8930 | `		}` |
|      228 |  8931 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8932 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8933 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8934 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8935 | `				continue;` |
|        - |  8936 | `			}` |
|      260 |  8937 | `			VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|      260 |  8938 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8939 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8940 | `			if( rc == 0 ){` |
|       93 |  8941 | `				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|       93 |  8942 | `				matched = 1;` |
|       93 |  8943 | `				break;` |
|        - |  8944 | `			}` |
|       85 |  8945 | `		}` |
|      115 |  8946 | `	}` |
|      111 |  8947 | `	if( !matched && pDefault ){` |
|       13 |  8948 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|       13 |  8949 | `		matched = 1;` |
|        6 |  8950 | `	}` |
|      111 |  8951 | `	if( !matched ){` |
|        6 |  8952 | `		const char *zType = "unknown";` |
|        - |  8953 | `		char zMsg[128];` |
|        - |  8954 | `		sxu32 nMsg;` |
|        6 |  8955 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8956 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8957 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        6 |  8958 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8959 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8960 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8961 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8962 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8963 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8964 | `		default: break;` |
|        - |  8965 | `		}` |
|        8 |  8966 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8967 | `			"Unhandled match case of type %s",zType);` |
|        8 |  8968 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8969 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        6 |  8970 | `		PH7_MemObjRelease(&sSubject);` |
|        6 |  8971 | `		PH7_MemObjRelease(&sResult);` |
|        6 |  8972 | `		goto Abort;` |
|        - |  8973 | `	}` |
|      105 |  8974 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8975 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8976 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8977 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8978 | `	break;` |
|        - |  8979 | `					}` |
|        - |  8980 | `/*` |
|        - |  8981 | ` * OP_YIELD P1 P2 *` |
|        - |  8982 | ` *  Yield a value from a generator function.` |
|        - |  8983 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8984 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8985 | ` */` |
|       62 |  8986 | `case PH7_OP_YIELD: {` |
|        - |  8987 | `	ph7_generator *pGen;` |
|      129 |  8988 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8989 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8990 | `		goto Abort;` |
|        - |  8991 | `	}` |
|      129 |  8992 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|      129 |  8993 | `	if( pInstr->iP2 ){` |
|        - |  8994 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8995 | `#ifdef UNTRUST` |
|        - |  8996 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8997 | `#endif` |
|       20 |  8998 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       20 |  8999 | `		VmPopOperand(&pTos, 1);` |
|       20 |  9000 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|       20 |  9001 | `		VmPopOperand(&pTos, 1);` |
|        - |  9002 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|       20 |  9003 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  9004 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  9005 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  9006 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  9007 | `			}` |
|        2 |  9008 | `		}` |
|      120 |  9009 | `	}else if( pInstr->iP1 ){` |
|        - |  9010 | `		/* yield $value */` |
|        - |  9011 | `#ifdef UNTRUST` |
|        - |  9012 | `		if( pTos < pStack ) goto Abort;` |
|        - |  9013 | `#endif` |
|      111 |  9014 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|      111 |  9015 | `		VmPopOperand(&pTos, 1);` |
|        - |  9016 | `		/* Auto-increment key */` |
|      111 |  9017 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      111 |  9018 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      111 |  9019 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       58 |  9020 | `	}else{` |
|        - |  9021 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  9022 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9023 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9024 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  9025 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  9026 | `	}` |
|        - |  9027 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|      129 |  9028 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|      129 |  9029 | `	goto Suspend;` |
|        - |  9030 |  |
|        - |  9031 | `/*` |
|        - |  9032 | ` * OP_CALL P1 * *` |
|        - |  9033 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  9034 | ` *  function on the stack.` |
|        - |  9035 | ` */` |
|   362365 |  9036 | `case PH7_OP_CALL: {` |
|   724779 |  9037 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  9038 | `	ph7_value *pArg;` |
|   724779 |  9039 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   724779 |  9040 | `	pArg = &pTos[-nCallArgs];` |
|        - |  9041 | `	SyHashEntry *pEntry;` |
|        - |  9042 | `	SyString sName;` |
|        - |  9043 | `	/* Extract function name */` |
|   724779 |  9044 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  9045 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  9046 | `			ph7_value sResult;` |
|        - |  9047 | `			sxi32 rcArr;` |
|        3 |  9048 | `			SySetReset(&aArg);` |
|        3 |  9049 | `			while( pArg < pTos ){` |
|      ! 0 |  9050 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  9051 | `				pArg++;` |
|      ! 0 |  9052 | `			}` |
|        3 |  9053 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9054 | `			/* May be a class instance and it's static method */` |
|        3 |  9055 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  9056 | `			SySetReset(&aArg);` |
|        - |  9057 | `			/* Pop given arguments */` |
|        3 |  9058 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9059 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9060 | `			}` |
|        3 |  9061 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  9062 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9063 | `				goto Abort;` |
|        - |  9064 | `			}` |
|        3 |  9065 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  9066 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  9067 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  9068 | `				sxi32 iResumePc;` |
|        3 |  9069 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  9070 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  9071 | `					PH7_MemObjRelease(pTos);` |
|        3 |  9072 | `					pc = iResumePc;` |
|        3 |  9073 | `					break;` |
|        - |  9074 | `				}` |
|      ! 0 |  9075 | `				goto Exception;` |
|        - |  9076 | `			}` |
|        - |  9077 | `			/* Copy result */` |
|      ! 0 |  9078 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  9079 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  9080 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  9081 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  9082 | `			ph7_value sResult;` |
|        - |  9083 | `			sxi32 rcInv;` |
|       84 |  9084 | `			SySetReset(&aArg);` |
|      200 |  9085 | `			while( pArg < pTos ){` |
|      118 |  9086 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  9087 | `				pArg++;` |
|        2 |  9088 | `			}` |
|       84 |  9089 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  9090 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  9091 | `				(int)SySetUsed(&aArg),` |
|       82 |  9092 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  9093 | `				&sResult,` |
|       82 |  9094 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  9095 | `			SySetReset(&aArg);` |
|       84 |  9096 | `			if( nCallArgs > 0 ){` |
|       76 |  9097 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  9098 | `			}` |
|       84 |  9099 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  9100 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  9101 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  9102 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  9103 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  9104 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  9105 | `				pThis->iRef++;` |
|       13 |  9106 | `				PH7_MemObjRelease(pTos);` |
|       13 |  9107 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  9108 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  9109 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9110 | `					goto Abort;` |
|        - |  9111 | `				}` |
|        - |  9112 | `				{` |
|       13 |  9113 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  9114 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  9115 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  9116 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  9117 | `						break;` |
|        - |  9118 | `					}` |
|        - |  9119 | `				}` |
|      ! 0 |  9120 | `				goto Exception;` |
|        - |  9121 | `			}` |
|       72 |  9122 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  9123 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9124 | `				goto Abort;` |
|        - |  9125 | `			}` |
|       72 |  9126 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  9127 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  9128 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  9129 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  9130 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  9131 | `				sxi32 iResumePc;` |
|        7 |  9132 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  9133 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  9134 | `					PH7_MemObjRelease(pTos);` |
|        5 |  9135 | `					pc = iResumePc;` |
|        5 |  9136 | `					break;` |
|        - |  9137 | `				}` |
|        3 |  9138 | `				goto Exception;` |
|        - |  9139 | `			}` |
|       66 |  9140 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  9141 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  9142 | `		}else{` |
|        - |  9143 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  9144 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  9145 | `			/* Pop given arguments */` |
|      ! 0 |  9146 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9147 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9148 | `			}` |
|        - |  9149 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9150 | `			PH7_MemObjRelease(pTos);` |
|        - |  9151 | `		}` |
|       66 |  9152 | `		break;` |
|        - |  9153 | `	}` |
|   724695 |  9154 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  9155 | `	/* Check for a compiled function first.` |
|        - |  9156 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  9157 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   724695 |  9158 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9159 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  9160 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  9161 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  9162 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  9163 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  9164 | `	{` |
|   724695 |  9165 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   724695 |  9166 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  9167 | `		const char *zFunc;` |
|        - |  9168 | `		const char *zEnd;` |
|        - |  9169 | `		const char *z;` |
|        - |  9170 | `		SyString sGlobal;` |
|       24 |  9171 | `		zFunc = sName.zString;` |
|       24 |  9172 | `		zEnd  = zFunc + sName.nByte;` |
|       24 |  9173 | `		z = zEnd;` |
|        - |  9174 | `		/* Find last namespace separator */` |
|      196 |  9175 | `		while( z > zFunc ){` |
|      196 |  9176 | `			if( z[-1] == '\\' ){` |
|       24 |  9177 | `				break;` |
|        - |  9178 | `			}` |
|      176 |  9179 | `			z--;` |
|        4 |  9180 | `		}` |
|       24 |  9181 | `		if( z > zFunc && z < zEnd ){` |
|        - |  9182 | `			/* Retry lookup using the unqualified/global function name */` |
|       24 |  9183 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       24 |  9184 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  9185 | `		}` |
|       10 |  9186 | `	}` |
|        - |  9187 | `	} /* end VmCallArgMap namespace scope */` |
|   724695 |  9188 | `	if( pEntry ){` |
|        - |  9189 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  9190 | `		ph7_class_instance *pThis;` |
|        - |  9191 | `		ph7_value *pFrameStack;` |
|        - |  9192 | `		ph7_vm_func *pVmFunc;` |
|        - |  9193 | `		ph7_class *pSelf;` |
|        - |  9194 | `		VmFrame *pFrame;` |
|        - |  9195 | `		ph7_value *pObj;` |
|        - |  9196 | `		VmSlot sArg;` |
|        - |  9197 | `		sxu32 n;` |
|        - |  9198 | `		/* initialize fields */` |
|    19293 |  9199 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    19293 |  9200 | `		pThis = 0;` |
|    19293 |  9201 | `		pSelf = 0;` |
|    19293 |  9202 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  9203 | `			ph7_class_method *pMeth;` |
|        - |  9204 | `			/* Class method call */` |
|     3769 |  9205 | `			ph7_value *pTarget = &pTos[-1];` |
|     3769 |  9206 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  9207 | `				/* Extract the 'this' pointer */` |
|     3769 |  9208 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  9209 | `					/* Instance already loaded */` |
|     3679 |  9210 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3679 |  9211 | `					pThis->iRef++;` |
|     3679 |  9212 | `					pSelf = pThis->pClass;` |
|     1837 |  9213 | `				}` |
|     3769 |  9214 | `				if( pSelf == 0 ){` |
|       95 |  9215 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  9216 | `						/* "Late Static Binding" class name */` |
|      131 |  9217 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  9218 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  9219 | `					}` |
|       95 |  9220 | `					if( pSelf == 0 ){` |
|       21 |  9221 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  9222 | `					}` |
|       45 |  9223 | `				}` |
|     3769 |  9224 | `				if( pThis == 0  ){` |
|       95 |  9225 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       95 |  9226 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       95 |  9227 | `					if( pFrameLocal->pParent ){` |
|        - |  9228 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       69 |  9229 | `						pThis = pFrameLocal->pThis;` |
|       69 |  9230 | `						if( pThis ){` |
|       21 |  9231 | `							pThis->iRef++;` |
|       10 |  9232 | `						}` |
|       32 |  9233 | `					}` |
|       45 |  9234 | `				}` |
|     3769 |  9235 | `				VmPopOperand(&pTos,1);` |
|     3769 |  9236 | `				PH7_MemObjRelease(pTos);` |
|        - |  9237 | `				/* Synchronize pointers */` |
|     3769 |  9238 | `				pArg = &pTos[-nCallArgs];` |
|        - |  9239 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  9240 | `				 * user have already computed the random generated unique class method name` |
|        - |  9241 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  9242 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  9243 | `				 */` |
|     3769 |  9244 | `				while( pArg < pStack ){` |
|      ! 0 |  9245 | `					pArg++;` |
|      ! 0 |  9246 | `				}` |
|     3769 |  9247 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  9248 | `					/* Check if the call is allowed */` |
|     3769 |  9249 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3769 |  9250 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  9251 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  9252 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  9253 | `							char zMsg[256];` |
|      ! 0 |  9254 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  9255 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  9256 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  9257 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  9258 | `							/* Pop given arguments */` |
|      ! 0 |  9259 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  9260 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9261 | `							}` |
|      ! 0 |  9262 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  9263 | `							goto Abort;` |
|        - |  9264 | `						}` |
|        6 |  9265 | `					}` |
|     1882 |  9266 | `				}` |
|     1882 |  9267 | `			}` |
|     1882 |  9268 | `		}` |
|        - |  9269 | `		/* Check The recursion limit. Hitting it raises a clean, non-catchable` |
|        - |  9270 | `		 * fatal (was: silently set NULL and continue) and halts. The check is` |
|        - |  9271 | `		 * before VmEnterFrame/the recursive VmByteCodeExec below, so a` |
|        - |  9272 | `		 * correctly-set cap also keeps deep recursion off the native stack. */` |
|    19293 |  9273 | `		if( VmRecursionExceeded(pVm) ){` |
|        - |  9274 | `			/* Args and the function-name slot are released by the Abort label,` |
|        - |  9275 | `			 * which walks the whole operand stack — don't release them here. */` |
|        6 |  9276 | `			VmRecursionFatal(&(*pVm));` |
|        6 |  9277 | `			goto Abort;` |
|        - |  9278 | `		}` |
|    19289 |  9279 | `		if( pVmFunc->pNextName ){` |
|        - |  9280 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  9281 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  9282 | `		}` |
|    19289 |  9283 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  9284 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  9285 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  9286 | `			ph7_generator *pGenerator;` |
|        - |  9287 | `			ph7_class_instance *pGenObj;` |
|        - |  9288 | `			ph7_value *pCtxAttr;` |
|        - |  9289 | `			SyString sAttrName;` |
|        - |  9290 | `			ph7_value **apCallArgs;` |
|        - |  9291 | `			int nGenArgs, iArg;` |
|        - |  9292 | `			/* Collect arguments from the operand stack */` |
|       53 |  9293 | `			nGenArgs = (int)(pTos - pArg);` |
|       53 |  9294 | `			apCallArgs = 0;` |
|       53 |  9295 | `			if( nGenArgs > 0 ){` |
|       14 |  9296 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9297 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  9298 | `				if( apCallArgs == 0 ){` |
|        - |  9299 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  9300 | `					nGenArgs = 0;` |
|      ! 0 |  9301 | `				}else{` |
|       10 |  9302 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9303 | `					int didReorder = 0;` |
|       10 |  9304 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9305 | `						/* Named-argument reordering for generator */` |
|        5 |  9306 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9307 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9308 | `						sxu32 nNV = nF;` |
|        5 |  9309 | `						sxi32 iVIdx = -1;` |
|        - |  9310 | `						sxi32 *aGSlot;` |
|        - |  9311 | `						sxu8 *aGUsed;` |
|        - |  9312 | `						sxu32 gi;` |
|       13 |  9313 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9314 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9315 | `						}` |
|        7 |  9316 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9317 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9318 | `						if( aGSlot ){` |
|        5 |  9319 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9320 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9321 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9322 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9323 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9324 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9325 | `								goto Abort;` |
|        - |  9326 | `							}` |
|        - |  9327 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9328 | `							 * append overflow (variadic / positional beyond` |
|        - |  9329 | `							 * formals) so downstream sees every argument. */` |
|        - |  9330 | `							{` |
|        5 |  9331 | `								int nOut = 0;` |
|       13 |  9332 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9333 | `									sxu32 gj;` |
|       13 |  9334 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9335 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9336 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9337 | `											break;` |
|        - |  9338 | `										}` |
|        3 |  9339 | `									}` |
|        5 |  9340 | `								}` |
|       13 |  9341 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9342 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9343 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9344 | `									}` |
|        5 |  9345 | `								}` |
|        5 |  9346 | `								nGenArgs = nOut;` |
|        - |  9347 | `							}` |
|        5 |  9348 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9349 | `							didReorder = 1;` |
|        2 |  9350 | `						}` |
|        - |  9351 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9352 | `						 * positional fill below — preserves arg order rather` |
|        - |  9353 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9354 | `					}` |
|       10 |  9355 | `					if( !didReorder ){` |
|       12 |  9356 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9357 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9358 | `						}` |
|        2 |  9359 | `					}` |
|        - |  9360 | `				}` |
|        4 |  9361 | `			}` |
|        - |  9362 | `			/* Create execution context and generator wrapper */` |
|       53 |  9363 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       53 |  9364 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9365 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9366 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9367 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9368 | `				break;` |
|        - |  9369 | `			}` |
|       53 |  9370 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       53 |  9371 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9372 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9373 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9374 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9375 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9376 | `				break;` |
|        - |  9377 | `			}` |
|        - |  9378 | `			/* Set up the frame with arguments, closure env, $this */` |
|       53 |  9379 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       53 |  9380 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       53 |  9381 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       53 |  9382 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       53 |  9383 | `			pExecCtx->pFrame->pParent = 0;` |
|       53 |  9384 | `			if( apCallArgs ){` |
|       10 |  9385 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9386 | `			}` |
|       53 |  9387 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9388 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9389 | `				if( pThis ){` |
|      ! 0 |  9390 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9391 | `				}` |
|      ! 0 |  9392 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9393 | `					goto Abort;` |
|        - |  9394 | `				}` |
|      ! 0 |  9395 | `				break;` |
|        - |  9396 | `			}` |
|        - |  9397 | `			/* Create Generator class instance */` |
|       53 |  9398 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       53 |  9399 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9400 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9401 | `				break;` |
|        - |  9402 | `			}` |
|        - |  9403 | `			/* Store generator in __ctx attribute */` |
|       53 |  9404 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       53 |  9405 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       53 |  9406 | `			if( pCtxAttr ){` |
|       53 |  9407 | `				pCtxAttr->x.pOther = pGenerator;` |
|       53 |  9408 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       24 |  9409 | `			}` |
|        - |  9410 | `			/* Pop args and function name, push Generator object */` |
|       53 |  9411 | `			PH7_MemObjRelease(pTos);` |
|       53 |  9412 | `			pTos = &pTos[-nCallArgs];` |
|       53 |  9413 | `			pTos->x.pOther = pGenObj;` |
|       53 |  9414 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       53 |  9415 | `			pGenObj->iRef++;` |
|       53 |  9416 | `			if( pThis ){` |
|      ! 0 |  9417 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9418 | `			}` |
|       53 |  9419 | `			break;` |
|        - |  9420 | `		}` |
|        - |  9421 | `		/* Extract the formal argument set */` |
|    19241 |  9422 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9423 | `		/* Create a new VM frame  */` |
|    19241 |  9424 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    19241 |  9425 | `		if( rc != SXRET_OK ){` |
|        - |  9426 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9427 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9428 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9429 | `				&pVmFunc->sName);` |
|        - |  9430 | `			/* Pop given arguments */` |
|      ! 0 |  9431 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9432 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9433 | `			}` |
|        - |  9434 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9435 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9436 | `			break;` |
|        - |  9437 | `		}` |
|    19241 |  9438 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9439 | `			/* Install the '$this' variable */` |
|        - |  9440 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3697 |  9441 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3697 |  9442 | `			if( pObj ){` |
|        - |  9443 | `				/* Reflect the change */` |
|     3697 |  9444 | `				pObj->x.pOther = pThis;` |
|     3697 |  9445 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1846 |  9446 | `			}` |
|     1846 |  9447 | `		}` |
|    19241 |  9448 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9449 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9450 | `			/* Install static variables */` |
|       13 |  9451 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       25 |  9452 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|       13 |  9453 | `				pStatic = &aStatic[n];` |
|       13 |  9454 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9455 | `					/* Initialize the static variables */` |
|        9 |  9456 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        9 |  9457 | `					if( pObj ){` |
|        - |  9458 | `						/* Assume a NULL initialization value */` |
|        9 |  9459 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        9 |  9460 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9461 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        9 |  9462 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);` |
|        4 |  9463 | `						}` |
|        9 |  9464 | `						pObj->nIdx = pStatic->nIdx;` |
|        5 |  9465 | `					}else{` |
|      ! 0 |  9466 | `						continue;` |
|        - |  9467 | `					}` |
|        4 |  9468 | `				}` |
|        - |  9469 | `				/* Install in the current frame */` |
|       19 |  9470 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|       12 |  9471 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        7 |  9472 | `			}` |
|        6 |  9473 | `		}` |
|        - |  9474 | `		/* Push arguments in the local frame */` |
|        - |  9475 | `		{` |
|    19241 |  9476 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9477 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9478 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    19241 |  9479 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    19241 |  9480 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9481 | `			/* ============================================================` |
|        - |  9482 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9483 | `			 *` |
|        - |  9484 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9485 | `			 * or position, then install them in the frame.` |
|        - |  9486 | `			 * ============================================================ */` |
|       97 |  9487 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       97 |  9488 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       97 |  9489 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9490 | `			sxu32 nNonVariadic;` |
|        - |  9491 | `			sxi32 *aSlot;` |
|        - |  9492 | `			sxu8  *aUsed;` |
|        - |  9493 | `			sxu32 i;` |
|        - |  9494 | `			/* Find variadic parameter index */` |
|      293 |  9495 | `			for( i = 0; i < nFormal; i++ ){` |
|      207 |  9496 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9497 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9498 | `					break;` |
|        - |  9499 | `				}` |
|      101 |  9500 | `			}` |
|       97 |  9501 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9502 | `			/* Allocate mapping arrays */` |
|      144 |  9503 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9504 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       97 |  9505 | `			if( aSlot == 0 ){` |
|      ! 0 |  9506 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9507 | `				goto Abort;` |
|        - |  9508 | `			}` |
|       97 |  9509 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9510 | `			/* Resolve named arguments to formal parameters */` |
|      144 |  9511 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9512 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       97 |  9513 | `			if( rc == PH7_ABORT ){` |
|        8 |  9514 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        8 |  9515 | `				goto Abort;` |
|        - |  9516 | `			}` |
|        - |  9517 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9518 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9519 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9520 | `				sxi32 iSrc = -1;` |
|      309 |  9521 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9522 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9523 | `						iSrc = (sxi32)i;` |
|      169 |  9524 | `						break;` |
|        - |  9525 | `					}` |
|       62 |  9526 | `				}` |
|      187 |  9527 | `				if( iSrc >= 0 ){` |
|        - |  9528 | `					/* Argument was provided — install with type checking */` |
|      169 |  9529 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9530 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9531 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9532 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9533 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal,FALSE);` |
|      ! 0 |  9534 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9535 | `					}` |
|        - |  9536 | `					/* Type checking: union types */` |
|      169 |  9537 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9538 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9539 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9540 | `							bCallIsStrict);` |
|       13 |  9541 | `						if( rcU != SXRET_OK ){` |
|        - |  9542 | `							const char *zGiven;` |
|      ! 0 |  9543 | `							const char *zExpected = "union";` |
|        - |  9544 | `							char zBuf[128];` |
|        - |  9545 | `							char zTypeBuf[128];` |
|      ! 0 |  9546 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9547 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9548 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9549 | `								zGiven = "null";` |
|      ! 0 |  9550 | `							}else{` |
|      ! 0 |  9551 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9552 | `							}` |
|      ! 0 |  9553 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9554 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9555 | `							}` |
|      ! 0 |  9556 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9557 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9558 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9559 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9560 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9561 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9562 | `							pFrameStack = 0;` |
|      ! 0 |  9563 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9564 | `							goto SkipFuncBody;` |
|        - |  9565 | `						}` |
|      171 |  9566 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9567 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9568 | `						/* Scalar/class type checking */` |
|       17 |  9569 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9570 | `							SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9571 | `							ph7_class *pClass;` |
|      ! 0 |  9572 | `							int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|      ! 0 |  9573 | `							if( rcPseudo == 0 ){` |
|        - |  9574 | `								/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9575 | `								char zTypeBuf[128],zGivenBuf[128];` |
|      ! 0 |  9576 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9577 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9578 | `									VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  9579 | `									VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|      ! 0 |  9580 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9581 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9582 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9583 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9584 | `								pFrameStack = 0;` |
|      ! 0 |  9585 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9586 | `								goto SkipFuncBody;` |
|        - |  9587 | `							}` |
|        - |  9588 | `							/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class */` |
|      ! 0 |  9589 | `							pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9590 | `							if( pClass ){` |
|      ! 0 |  9591 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9592 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9593 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9594 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9595 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9596 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9597 | `									}` |
|      ! 0 |  9598 | `								}else{` |
|      ! 0 |  9599 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9600 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9601 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9602 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9603 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9604 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9605 | `									}` |
|        - |  9606 | `								}` |
|      ! 0 |  9607 | `							}` |
|       17 |  9608 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9609 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9610 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9611 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9612 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9613 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9614 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9615 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9616 | `								pFrameStack = 0;` |
|      ! 0 |  9617 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9618 | `								goto SkipFuncBody;` |
|        7 |  9619 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9620 | `								char zTypeBuf[128];` |
|      ! 0 |  9621 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9622 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9623 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9624 | `									ph7_type_name(pVal));` |
|      ! 0 |  9625 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9626 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9627 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9628 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9629 | `								pFrameStack = 0;` |
|      ! 0 |  9630 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9631 | `								goto SkipFuncBody;` |
|        - |  9632 | `							}` |
|        3 |  9633 | `						}` |
|        8 |  9634 | `					}` |
|        - |  9635 | `					/* Install: by reference or by value */` |
|      169 |  9636 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9637 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9638 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9639 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9640 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9641 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9642 | `							}` |
|      ! 0 |  9643 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9644 | `						}else{` |
|        7 |  9645 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9646 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9647 | `							if( pRefEntry == 0 ){` |
|        7 |  9648 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9649 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9650 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9651 | `								sArg.pUserData = 0;` |
|        5 |  9652 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9653 | `							}` |
|        5 |  9654 | `							pObj = 0;` |
|        - |  9655 | `						}` |
|        3 |  9656 | `					}else{` |
|      165 |  9657 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9658 | `					}` |
|      169 |  9659 | `					if( pObj ){` |
|      165 |  9660 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9661 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9662 | `						sArg.pUserData = 0;` |
|      165 |  9663 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9664 | `					}` |
|       85 |  9665 | `				}else{` |
|        - |  9666 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9667 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9668 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9669 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9670 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9671 | `						if( pObj ){` |
|       19 |  9672 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|       19 |  9673 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9674 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9675 | `							sArg.pUserData = 0;` |
|       19 |  9676 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9677 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9678 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9679 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9680 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9681 | `							}` |
|        9 |  9682 | `						}` |
|        9 |  9683 | `					}` |
|        - |  9684 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9685 | `				}` |
|       94 |  9686 | `			}` |
|        - |  9687 | `			/* Handle variadic parameter */` |
|       89 |  9688 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9689 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9690 | `				if( pObj ){` |
|        9 |  9691 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9692 | `					{` |
|        9 |  9693 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9694 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9695 | `							if( aSlot[i] == -1 ){` |
|       16 |  9696 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9697 | `									/* Named variadic entry: insert with string key */` |
|        - |  9698 | `									ph7_value sKey;` |
|       11 |  9699 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9700 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9701 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9702 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9703 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9704 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9705 | `								}else{` |
|        - |  9706 | `									/* Positional variadic entry */` |
|      ! 0 |  9707 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9708 | `								}` |
|        5 |  9709 | `							}` |
|       12 |  9710 | `						}` |
|        - |  9711 | `					}` |
|        9 |  9712 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9713 | `					sArg.pUserData = 0;` |
|        9 |  9714 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9715 | `				}` |
|        5 |  9716 | `			}else{` |
|        - |  9717 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9718 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9719 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9720 | `				 * the positional-only path's behavior. */` |
|       81 |  9721 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9722 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9723 | `					if( aSlot[i] == -2 ){` |
|        - |  9724 | `						char zAnonBuf[32];` |
|        - |  9725 | `						SyString sAnonName;` |
|      ! 0 |  9726 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9727 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9728 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9729 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9730 | `						if( pObj ){` |
|      ! 0 |  9731 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9732 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9733 | `							sArg.pUserData = 0;` |
|      ! 0 |  9734 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9735 | `						}` |
|      ! 0 |  9736 | `						nAnon++;` |
|      ! 0 |  9737 | `					}` |
|       79 |  9738 | `				}` |
|        - |  9739 | `			}` |
|        - |  9740 | `			/* Release all stack arguments */` |
|      267 |  9741 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9742 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9743 | `			}` |
|       89 |  9744 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9745 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9746 | `			n = nFormal;` |
|       45 |  9747 | `		}else{` |
|        - |  9748 | `		/* ============================================================` |
|        - |  9749 | `		 * Positional-only matching path (original)` |
|        - |  9750 | `		 * ============================================================ */` |
|    19147 |  9751 | `		n = 0;` |
|    50063 |  9752 | `		while( pArg < pTos ){` |
|    31001 |  9753 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9754 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       45 |  9755 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       45 |  9756 | `				if( pObj ){` |
|        - |  9757 | `					/* Initialize as empty array */` |
|       45 |  9758 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9759 | `					{` |
|       45 |  9760 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      161 |  9761 | `						while( pArg < pTos ){` |
|        - |  9762 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9763 | `							 *` |
|        - |  9764 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9765 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9766 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9767 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9768 | `							 * fixing both wants a separate counter for elements` |
|        - |  9769 | `							 * already packed into the variadic array. */` |
|      123 |  9770 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9771 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9772 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9773 | `									bCallIsStrict);` |
|       16 |  9774 | `								if( rcU != SXRET_OK ){` |
|        - |  9775 | `									const char *zGiven;` |
|        3 |  9776 | `									const char *zExpected = "union";` |
|        - |  9777 | `									char zBuf[128];` |
|        - |  9778 | `									char zTypeBuf[128];` |
|        3 |  9779 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9780 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9781 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9782 | `										zGiven = "null";` |
|      ! 0 |  9783 | `									}else{` |
|        3 |  9784 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9785 | `									}` |
|        3 |  9786 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9787 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9788 | `									}` |
|        4 |  9789 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9790 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9791 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9792 | `										goto Abort;` |
|        - |  9793 | `									}` |
|        3 |  9794 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9795 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9796 | `									pFrameStack = 0;` |
|        3 |  9797 | `									rc = PH7_EXCEPTION;` |
|        3 |  9798 | `									goto SkipFuncBody;` |
|        - |  9799 | `								}` |
|       14 |  9800 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9801 | `								pArg++;` |
|       14 |  9802 | `								continue;` |
|        - |  9803 | `							}` |
|        - |  9804 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9805 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      120 |  9806 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9807 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       44 |  9808 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9809 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9810 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9811 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9812 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9813 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9814 | `										goto Abort;` |
|        - |  9815 | `									}` |
|        - |  9816 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9817 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9818 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9819 | `									pFrameStack = 0;` |
|      ! 0 |  9820 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9821 | `									goto SkipFuncBody;` |
|       13 |  9822 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9823 | `									char zTypeBuf[128];` |
|      ! 0 |  9824 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9825 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9826 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9827 | `										ph7_type_name(pArg));` |
|      ! 0 |  9828 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9829 | `										goto Abort;` |
|        - |  9830 | `									}` |
|      ! 0 |  9831 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9832 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9833 | `									pFrameStack = 0;` |
|      ! 0 |  9834 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9835 | `									goto SkipFuncBody;` |
|        - |  9836 | `								}` |
|        6 |  9837 | `							}` |
|      109 |  9838 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      109 |  9839 | `							pArg++;` |
|        5 |  9840 | `						}` |
|        - |  9841 | `					}` |
|       43 |  9842 | `					sArg.nIdx = pObj->nIdx;` |
|       43 |  9843 | `					sArg.pUserData = 0;` |
|       43 |  9844 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       19 |  9845 | `				}` |
|       43 |  9846 | `				break; /* All remaining args consumed */` |
|        - |  9847 | `			}` |
|    30961 |  9848 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30740 |  9849 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       44 |  9850 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9851 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9852 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg,FALSE);` |
|      ! 0 |  9853 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9854 | `						goto Abort;` |
|        - |  9855 | `					}` |
|      ! 0 |  9856 | `				}` |
|        - |  9857 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30745 |  9858 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       98 |  9859 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       62 |  9860 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       31 |  9861 | `						bCallIsStrict);` |
|       67 |  9862 | `					if( rcU != SXRET_OK ){` |
|        - |  9863 | `						const char *zGiven;` |
|       22 |  9864 | `						const char *zExpected = "union";` |
|        - |  9865 | `						char zBuf[128];` |
|        - |  9866 | `						char zTypeBuf[128];` |
|       22 |  9867 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        8 |  9868 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       18 |  9869 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|       10 |  9870 | `							zGiven = "null";` |
|        6 |  9871 | `						}else{` |
|        6 |  9872 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9873 | `						}` |
|       22 |  9874 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       22 |  9875 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9876 | `						}` |
|       31 |  9877 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9878 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       22 |  9879 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9880 | `							goto Abort;` |
|        - |  9881 | `						}` |
|       22 |  9882 | `						PH7_MemObjRelease(pTos);` |
|       22 |  9883 | `						pTos = &pTos[-nCallArgs];` |
|       22 |  9884 | `						pFrameStack = 0;` |
|       22 |  9885 | `						rc = PH7_EXCEPTION;` |
|       22 |  9886 | `						goto SkipFuncBody;` |
|        - |  9887 | `					}` |
|       23 |  9888 | `				}else` |
|        - |  9889 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9890 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30708 |  9891 | `				if( aFormalArg[n].nType > 0` |
|    16070 |  9892 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1427 |  9893 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9894 | `						/* Argument must be a class instance [i.e: object] */` |
|       37 |  9895 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9896 | `						ph7_class *pClass;` |
|       37 |  9897 | `						int rcPseudo = VmCheckPseudoType(&(*pVm),pArg,pName);` |
|       37 |  9898 | `						if( rcPseudo == 0 ){` |
|        - |  9899 | `							/* Recognised pseudo-type (true/false/iterable); value mismatches */` |
|        - |  9900 | `							char zTypeBuf[128],zGivenBuf[128];` |
|        7 |  9901 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  9902 | `								&aFormalArg[n].sName,` |
|        2 |  9903 | `								VmSyStringToCStr(pName,zTypeBuf,sizeof(zTypeBuf)),` |
|        2 |  9904 | `								VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf)));` |
|        5 |  9905 | `							if( rc == PH7_ABORT ) goto Abort;` |
|        5 |  9906 | `							PH7_MemObjRelease(pTos);` |
|        5 |  9907 | `							pTos = &pTos[-nCallArgs];` |
|        5 |  9908 | `							pFrameStack = 0;` |
|        5 |  9909 | `							rc = PH7_EXCEPTION;` |
|        5 |  9910 | `							goto SkipFuncBody;` |
|        - |  9911 | `						}` |
|        - |  9912 | `						/* Try to extract the desired class (rcPseudo==1 accepts; -1 real class) */` |
|       33 |  9913 | `						pClass = (rcPseudo == 1) ? 0 : PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       33 |  9914 | `						if( pClass ){` |
|       23 |  9915 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9916 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9917 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9918 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9919 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9920 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9921 | `								}` |
|      ! 0 |  9922 | `							}else{` |
|        - |  9923 | `								/* reuse pThis declared in outer scope */` |
|       23 |  9924 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9925 | `								/* Make sure the object is an instance of the given class */` |
|       23 |  9926 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9927 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9928 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9929 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9930 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9931 | `								}` |
|        - |  9932 | `							}` |
|       13 |  9933 | `						}` |
|     1408 |  9934 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       30 |  9935 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9936 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9937 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9938 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9939 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9940 | `								goto Abort;` |
|        - |  9941 | `							}` |
|        - |  9942 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9943 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9944 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9945 | `							pFrameStack = 0;` |
|       11 |  9946 | `							rc = PH7_EXCEPTION;` |
|       11 |  9947 | `							goto SkipFuncBody;` |
|       19 |  9948 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9949 | `							char zTypeBuf[128];` |
|       15 |  9950 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        8 |  9951 | `								&aFormalArg[n].sName,` |
|        8 |  9952 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        4 |  9953 | `								ph7_type_name(pArg));` |
|       11 |  9954 | `							if( rc == PH7_ABORT ){` |
|        6 |  9955 | `								goto Abort;` |
|        - |  9956 | `							}` |
|        5 |  9957 | `							PH7_MemObjRelease(pTos);` |
|        5 |  9958 | `							pTos = &pTos[-nCallArgs];` |
|        5 |  9959 | `							pFrameStack = 0;` |
|        5 |  9960 | `							rc = PH7_EXCEPTION;` |
|        5 |  9961 | `							goto SkipFuncBody;` |
|        - |  9962 | `						}` |
|        4 |  9963 | `					}` |
|      700 |  9964 | `				}` |
|    30705 |  9965 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9966 | `					/* Pass by reference */` |
|       58 |  9967 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9968 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9969 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9970 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9971 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9972 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9973 | `						}` |
|        - |  9974 | `						/* Switch to pass by value */` |
|      ! 0 |  9975 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9976 | `					}else{` |
|        - |  9977 | `						SyHashEntry *pRefEntry;` |
|        - |  9978 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9979 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9980 | `						if( pRefEntry == 0 ){` |
|       86 |  9981 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9982 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9983 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9984 | `							sArg.pUserData = 0;` |
|       58 |  9985 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9986 | `						}` |
|       58 |  9987 | `						pObj = 0;` |
|        - |  9988 | `					}` |
|       30 |  9989 | `				}else{` |
|        - |  9990 | `					/* Pass by value,make a copy of the given argument */` |
|    30649 |  9991 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9992 | `				}` |
|    15355 |  9993 | `			}else{` |
|        - |  9994 | `				char zName[32];` |
|        - |  9995 | `				SyString sArgName;` |
|        - |  9996 | `				/* Set a dummy name */` |
|      220 |  9997 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      220 |  9998 | `				sArgName.zString = zName;` |
|        - |  9999 | `				/* Annonymous argument */` |
|      220 | 10000 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - | 10001 | `			}` |
|    30921 | 10002 | `			if( pObj ){` |
|    30865 | 10003 | `				PH7_MemObjStore(pArg,pObj);` |
|        - | 10004 | `				/* Insert argument index  */` |
|    30865 | 10005 | `				sArg.nIdx = pObj->nIdx;` |
|    30865 | 10006 | `				sArg.pUserData = 0;` |
|    30865 | 10007 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15430 | 10008 | `			}` |
|    30921 | 10009 | `			PH7_MemObjRelease(pArg);` |
|    30921 | 10010 | `			pArg++;` |
|    30921 | 10011 | `			++n;` |
|        5 | 10012 | `		}` |
|        - | 10013 | `		} /* end named vs positional branch */` |
|        - | 10014 | `		/* Set up closure environment */` |
|    19193 | 10015 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10016 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - | 10017 | `			ph7_value *pValue;` |
|        - | 10018 | `			sxu32 iEnv;` |
|      184 | 10019 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 | 10020 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 | 10021 | `				pEnv = &aEnv[iEnv];` |
|      252 | 10022 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - | 10023 | `					/* Do not install null value */` |
|      178 | 10024 | `					continue;` |
|        - | 10025 | `				}` |
|       76 | 10026 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 | 10027 | `				if( pValue == 0 ){` |
|      ! 0 | 10028 | `					continue;` |
|        - | 10029 | `				}` |
|        - | 10030 | `				/* Invalidate any prior representation */` |
|       76 | 10031 | `				PH7_MemObjRelease(pValue);` |
|        - | 10032 | `				/* Duplicate bound variable value */` |
|       76 | 10033 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 | 10034 | `			}` |
|       91 | 10035 | `		}` |
|        - | 10036 | `		/* Process default values for remaining formal parameters */` |
|    22233 | 10037 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     3093 | 10038 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 10039 | `				/* Variadic parameter with no extra args — create empty array */` |
|       53 | 10040 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       53 | 10041 | `				if( pObj ){` |
|       53 | 10042 | `					PH7_MemObjToHashmap(pObj);` |
|       53 | 10043 | `					sArg.nIdx = pObj->nIdx;` |
|       53 | 10044 | `					sArg.pUserData = 0;` |
|       53 | 10045 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 | 10046 | `				}` |
|       53 | 10047 | `				n++;` |
|       53 | 10048 | `				break; /* Variadic is always last */` |
|        - | 10049 | `			}` |
|     3045 | 10050 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     3039 | 10051 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     3039 | 10052 | `				if( pObj ){` |
|        - | 10053 | `					/* Evaluate the default value and extract it's result */` |
|     3039 | 10054 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);` |
|     3039 | 10055 | `					if( rc == PH7_ABORT ){` |
|      ! 0 | 10056 | `						goto Abort;` |
|        - | 10057 | `					}` |
|        - | 10058 | `					/* Insert argument index */` |
|     3039 | 10059 | `					sArg.nIdx = pObj->nIdx;` |
|     3039 | 10060 | `					sArg.pUserData = 0;` |
|     3039 | 10061 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - | 10062 | `					/* Make sure the default argument is of the correct type */` |
|     3034 | 10063 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1943 | 10064 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 | 10065 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - | 10066 | `						/* Cast to the desired type */` |
|        3 | 10067 | `						xCast(pObj);` |
|        1 | 10068 | `					}` |
|     1517 | 10069 | `				}` |
|     1517 | 10070 | `			}` |
|     3045 | 10071 | `			++n;` |
|        5 | 10072 | `		}` |
|        - | 10073 | `		} /* end VmCallArgMap scope */` |
|        - | 10074 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - | 10075 | `		 * does not return anything.` |
|        - | 10076 | `		 */` |
|    19193 | 10077 | `		PH7_MemObjRelease(pTos);` |
|    19193 | 10078 | `		pTos = &pTos[-nCallArgs];` |
|        - | 10079 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    19193 | 10080 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    19193 | 10081 | `		if( pFrameStack == 0 ){` |
|        - | 10082 | `			/* Raise exception: Out of memory */` |
|      ! 0 | 10083 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 | 10084 | `				&pVmFunc->sName);` |
|      ! 0 | 10085 | `			if( nCallArgs > 0 ){` |
|      ! 0 | 10086 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 | 10087 | `			}` |
|      ! 0 | 10088 | `			break;` |
|        - | 10089 | `		}` |
|     9594 | 10090 | `SkipFuncBody:` |
|    19231 | 10091 | `		if( pSelf ){` |
|        - | 10092 | `			/* Push class name */` |
|     3767 | 10093 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1881 | 10094 | `		}` |
|        - | 10095 | `		/* Increment nesting level */` |
|    19231 | 10096 | `		pVm->nRecursionDepth++;` |
|    19231 | 10097 | `		if( rc != PH7_EXCEPTION ){` |
|        - | 10098 | `			/* Execute function body */` |
|    28787 | 10099 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    19188 | 10100 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0, FALSE);` |
|     9594 | 10101 | `		}` |
|        - | 10102 | `		/* Decrement nesting level */` |
|    19231 | 10103 | `		pVm->nRecursionDepth--;` |
|    19231 | 10104 | `		if( pSelf ){` |
|        - | 10105 | `			/* Pop class name */` |
|     3767 | 10106 | `			(void)SySetPop(&pVm->aSelf);` |
|     1881 | 10107 | `		}` |
|        - | 10108 | `		/* Cleanup the mess left behind */` |
|    19231 | 10109 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - | 10110 | `			/* Return by reference,reflect that */` |
|        9 | 10111 | `			if( n != SXU32_HIGH ){` |
|        9 | 10112 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - | 10113 | `				sxu32 i;` |
|        - | 10114 | `				/* Make sure the referenced object is not a local variable */` |
|       13 | 10115 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 | 10116 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 | 10117 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 | 10118 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10119 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10120 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 | 10121 | `								&pVmFunc->sName);` |
|      ! 0 | 10122 | `						}` |
|      ! 0 | 10123 | `						n = SXU32_HIGH;` |
|      ! 0 | 10124 | `						break;` |
|        - | 10125 | `					}` |
|        3 | 10126 | `				}` |
|        5 | 10127 | `			}else{` |
|      ! 0 | 10128 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 | 10129 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - | 10130 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 | 10131 | `						&pVmFunc->sName);` |
|      ! 0 | 10132 | `				}` |
|        - | 10133 | `			}` |
|        9 | 10134 | `			pTos->nIdx = n;` |
|        4 | 10135 | `		}` |
|        - | 10136 | `		/* Cleanup the mess left behind */` |
|    19231 | 10137 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - | 10138 | `			/* An exception was throw in this frame */` |
|      121 | 10139 | `			pFrame = pFrame->pParent;` |
|      121 | 10140 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - | 10141 | `				/* Pop the resutlt */` |
|       77 | 10142 | `				VmPopOperand(&pTos,1);` |
|        - | 10143 | `				/* Jump to this destination */` |
|       77 | 10144 | `				pc = pFrame->iExceptionJump - 1;` |
|       77 | 10145 | `				rc = PH7_OK;` |
|       41 | 10146 | `			}else{` |
|       45 | 10147 | `				if( pFrame->pParent ){` |
|       43 | 10148 | `					rc = PH7_EXCEPTION;` |
|       22 | 10149 | `				}else{` |
|        - | 10150 | `					/* Continue normal execution */` |
|        3 | 10151 | `					rc = PH7_OK;` |
|        - | 10152 | `				}` |
|        - | 10153 | `			}` |
|       58 | 10154 | `		}` |
|        - | 10155 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    19231 | 10156 | `		if( pFrameStack ){` |
|    19193 | 10157 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9594 | 10158 | `		}` |
|        - | 10159 | `		/* Leave the frame */` |
|    19231 | 10160 | `		VmLeaveFrame(&(*pVm));` |
|    19231 | 10161 | `		if( rc == PH7_ABORT ){` |
|        - | 10162 | `			/* Abort processing immeditaley */` |
|      120 | 10163 | `			goto Abort;` |
|    19115 | 10164 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10165 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - | 10166 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - | 10167 | `			 * overwriting the state saved by the inner level.` |
|        - | 10168 | `			 * pTos points to the result slot (not yet written).` |
|        - | 10169 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       43 | 10170 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       43 | 10171 | `			goto Suspend;` |
|    19077 | 10172 | `		}else if( rc == PH7_EXCEPTION ){` |
|       43 | 10173 | `			goto Exception;` |
|        - | 10174 | `		}` |
|     9520 | 10175 | `	}else{` |
|        - | 10176 | `		ph7_user_func *pFunc;` |
|        - | 10177 | `		ph7_context sCtx;` |
|        - | 10178 | `		ph7_value sRet;` |
|        - | 10179 | `		/* Look for an installed foreign function.` |
|        - | 10180 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - | 10181 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - | 10182 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - | 10183 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   705407 | 10184 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - | 10185 | `		{` |
|   705407 | 10186 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   705407 | 10187 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - | 10188 | `			/* Compiler-qualified: try short name as global fallback */` |
|       24 | 10189 | `			const char *zShort = sName.zString;` |
|        - | 10190 | `			sxu32 i;` |
|      336 | 10191 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      316 | 10192 | `				if( sName.zString[i] == '\\' ){` |
|       30 | 10193 | `					zShort = &sName.zString[i + 1];` |
|       13 | 10194 | `				}` |
|      160 | 10195 | `			}` |
|       24 | 10196 | `			if( zShort != sName.zString ){` |
|       24 | 10197 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       24 | 10198 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 | 10199 | `			}` |
|       10 | 10200 | `		}` |
|        - | 10201 | `		} /* end VmCallArgMap namespace scope */` |
|   705407 | 10202 | `		if( pEntry == 0 ){` |
|        - | 10203 | `			/* Call to undefined function */` |
|        6 | 10204 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - | 10205 | `			/* Pop given arguments */` |
|        6 | 10206 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 | 10207 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 | 10208 | `			}` |
|        - | 10209 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        6 | 10210 | `			PH7_MemObjRelease(pTos);` |
|       61 | 10211 | `			break;` |
|        - | 10212 | `		}` |
|   705403 | 10213 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - | 10214 | `		/* Start collecting function arguments */` |
|   705403 | 10215 | `		SySetReset(&aArg);` |
|  1902327 | 10216 | `		while( pArg < pTos ){` |
|  1196929 | 10217 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1196929 | 10218 | `			pArg++;` |
|        5 | 10219 | `		}` |
|        - | 10220 | `		/* Assume a null return value */` |
|   705403 | 10221 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - | 10222 | `		/* Init the call context */` |
|   705403 | 10223 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - | 10224 | `		/* Call the foreign function */` |
|   705403 | 10225 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - | 10226 | `		/* Release the call context */` |
|   705403 | 10227 | `		VmReleaseCallContext(&sCtx);` |
|   705403 | 10228 | `		if( rc == PH7_ABORT ){` |
|        - | 10229 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - | 10230 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - | 10231 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      534 | 10232 | `			PH7_MemObjRelease(&sRet);` |
|      534 | 10233 | `			goto Abort;` |
|   704873 | 10234 | `		}else if( rc == PH7_EXCEPTION ){` |
|      118 | 10235 | `			VmFrame *pFrm = pVm->pFrame;` |
|      118 | 10236 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      118 | 10237 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - | 10238 | `				/* Exception was NOT caught, propagate */` |
|        6 | 10239 | `				goto Exception;` |
|        - | 10240 | `			}` |
|        - | 10241 | `			/* Exception was caught: pop args and the result slot */` |
|      113 | 10242 | `			PH7_MemObjRelease(&sRet);` |
|      113 | 10243 | `			if( pInstr->iP1 > 0 ){` |
|       97 | 10244 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       47 | 10245 | `			}` |
|        - | 10246 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      113 | 10247 | `			VmPopOperand(&pTos,1);` |
|        - | 10248 | `			/* Jump past the try/catch block via the exception frame */` |
|      113 | 10249 | `			pFrm = pVm->pFrame;` |
|      113 | 10250 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      113 | 10251 | `				pc = pFrm->iExceptionJump - 1;` |
|       55 | 10252 | `			}` |
|      113 | 10253 | `			break;` |
|        - | 10254 | `		}` |
|   704759 | 10255 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - | 10256 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - | 10257 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - | 10258 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - | 10259 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - | 10260 | `			 * and we need to save state here. If it's a nested call (method` |
|        - | 10261 | `			 * body), the user-function path above will handle re-saving. */` |
|       43 | 10262 | `			PH7_MemObjRelease(&sRet);` |
|       43 | 10263 | `			if( pInstr->iP1 > 0 ){` |
|       43 | 10264 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 | 10265 | `			}` |
|        - | 10266 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - | 10267 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       43 | 10268 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       43 | 10269 | `			goto Suspend;` |
|        - | 10270 | `		}` |
|   704721 | 10271 | `		if( pInstr->iP1 > 0 ){` |
|        - | 10272 | `			/* Pop function name and arguments */` |
|   682363 | 10273 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   341201 | 10274 | `		}` |
|        - | 10275 | `		/* Save foreign function return value */` |
|   704721 | 10276 | `		PH7_MemObjStore(&sRet,pTos);` |
|   704721 | 10277 | `		PH7_MemObjRelease(&sRet);` |
|        - | 10278 | `	}` |
|   723751 | 10279 | `	break;` |
|        - | 10280 | `				  }` |
|        - | 10281 | `/*` |
|        - | 10282 | ` * OP_CONSUME: P1 * *` |
|        - | 10283 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - | 10284 | ` */` |
|    16227 | 10285 | `case PH7_OP_CONSUME: {` |
|    32459 | 10286 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32459 | 10287 | `	ph7_value *pCur,*pOut = pTos;` |
|        - | 10288 |  |
|    32459 | 10289 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32459 | 10290 | `	pCur = pOut;` |
|        - | 10291 | `	/* Start the consume process  */` |
|    64955 | 10292 | `	while( pOut <= pTos ){` |
|        - | 10293 | `		/* Force a string cast */` |
|    32501 | 10294 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1077 | 10295 | `			PH7_MemObjToString(pOut);` |
|      536 | 10296 | `		}` |
|    32501 | 10297 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - | 10298 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - | 10299 | `			/* Invoke the output consumer callback */` |
|    19933 | 10300 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19933 | 10301 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19933 | 10302 | `			SyBlobRelease(&pOut->sBlob);` |
|    19933 | 10303 | `			if( rc == SXERR_ABORT ){` |
|        - | 10304 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 | 10305 | `				goto Abort;` |
|        - | 10306 | `			}` |
|     9964 | 10307 | `		}` |
|    32501 | 10308 | `		pOut++;` |
|        5 | 10309 | `	}` |
|    32459 | 10310 | `	pTos = &pCur[-1];` |
|    32454 | 10311 | `	break;` |
|        - | 10312 | `					 }` |
|        - | 10313 |  |
|        - | 10314 | `		} /* Switch() */` |
| 11865347 | 10315 | `		pc++; /* Next instruction in the stream */` |
|        5 | 10316 | `	} /* For(;;) */` |
|    22984 | 10317 | `Done:` |
|    45973 | 10318 | `	SySetRelease(&aArg);` |
|    45973 | 10319 | `	return SXRET_OK;` |
|      100 | 10320 | `Suspend:` |
|      205 | 10321 | `	SySetRelease(&aArg);` |
|      205 | 10322 | `	return PH7_SUSPEND;` |
|      349 | 10323 | `Abort:` |
|      702 | 10324 | `	SySetRelease(&aArg);` |
|     2188 | 10325 | `	while( pTos >= pStack ){` |
|     1490 | 10326 | `		PH7_MemObjRelease(pTos);` |
|     1490 | 10327 | `		pTos--;` |
|        4 | 10328 | `	}` |
|      702 | 10329 | `	return PH7_ABORT;` |
|       34 | 10330 | `Exception:` |
|       72 | 10331 | `	SySetRelease(&aArg);` |
|      128 | 10332 | `	while( pTos >= pStack ){` |
|       59 | 10333 | `		PH7_MemObjRelease(pTos);` |
|       59 | 10334 | `		pTos--;` |
|        3 | 10335 | `	}` |
|       72 | 10336 | `	return PH7_EXCEPTION;` |
|    23472 | 10337 |  |
|        - | 10338 | `/*` |
|        - | 10339 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10340 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10341 | ` * See block-comment on that function for additional information.` |
|        - | 10342 | ` */` |
|    21186 | 10343 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|        5 | 10344 |  |
|        - | 10345 | `	ph7_value *pStack;` |
|        - | 10346 | `	sxi32 rc;` |
|        - | 10347 | `	/* Allocate a new operand stack */` |
|    21191 | 10348 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    21191 | 10349 | `	if( pStack == 0 ){` |
|      ! 0 | 10350 | `		return SXERR_MEM;` |
|        - | 10351 | `	}` |
|        - | 10352 | `	/* Execute the program */` |
|    21191 | 10353 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates);` |
|        - | 10354 | `	/* Free the operand stack */` |
|    21191 | 10355 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10356 | `	/* Execution result */` |
|    21191 | 10357 | `	return rc;` |
|    10598 | 10358 |  |
|        - | 10359 | `/*` |
|        - | 10360 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10361 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10362 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10363 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10364 | ` * execution ends.` |
|        - | 10365 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10366 | ` * additional information.` |
|        - | 10367 | ` */` |
|     2840 | 10368 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        5 | 10369 |  |
|        - | 10370 | `	VmShutdownCB *pEntry;` |
|        - | 10371 | `	ph7_value *apArg[10];` |
|        - | 10372 | `	sxu32 n,nEntry;` |
|        - | 10373 | `	int i;` |
|        - | 10374 | `	/* Point to the stack of registered callbacks */` |
|     2845 | 10375 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31245 | 10376 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28405 | 10377 | `		apArg[i] = 0;` |
|    14205 | 10378 | `	}` |
|        - | 10379 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10380 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10381 | `	 * callbacks, mirroring PHP.` |
|        - | 10382 | `	 */` |
|     2845 | 10383 | `	pVm->bHaltRequested = 0;` |
|     2857 | 10384 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       17 | 10385 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       17 | 10386 | `		if( pEntry ){` |
|        - | 10387 | `			/* Prepare callback arguments if any */` |
|       17 | 10388 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10389 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10390 | `					break;` |
|        - | 10391 | `				}` |
|      ! 0 | 10392 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10393 | `			}` |
|        - | 10394 | `			/* Invoke the callback */` |
|       17 | 10395 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10396 | `			/*` |
|        - | 10397 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10398 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10399 | `			 */` |
|       17 | 10400 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       17 | 10401 | `			if( pEntry ){` |
|       17 | 10402 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       17 | 10403 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10404 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10405 | `				}` |
|        6 | 10406 | `			}` |
|       17 | 10407 | `			if( pVm->bHaltRequested ){` |
|        - | 10408 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10409 | `				break;` |
|        - | 10410 | `			}` |
|        6 | 10411 | `		}` |
|       11 | 10412 | `	}` |
|     2845 | 10413 | `	SySetReset(&pVm->aShutdown);` |
|     2845 | 10414 |  |
|        - | 10415 | `/*` |
|        - | 10416 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10417 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10418 | ` * See block-comment on that function for additional information.` |
|        - | 10419 | ` */` |
|     2840 | 10420 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        5 | 10421 |  |
|        - | 10422 | `	/* Make sure we are ready to execute this program */` |
|     2845 | 10423 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10424 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10425 | `	}` |
|        - | 10426 | `	/* Set the execution magic number  */` |
|     2845 | 10427 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10428 | `	/* Execute the program */` |
|     2845 | 10429 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE);` |
|        - | 10430 | `	/* Invoke any shutdown callbacks */` |
|     2845 | 10431 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10432 | `	/*` |
|        - | 10433 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10434 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10435 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10436 | `	 */` |
|     2845 | 10437 | `	return SXRET_OK;` |
|     1425 | 10438 |  |
|        - | 10439 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10440 | `/*` |
|        - | 10441 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10442 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10443 | ` */` |
|       72 | 10444 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        5 | 10445 |  |
|        - | 10446 | `	ph7_exec_ctx *pCtx;` |
|        - | 10447 | `	ph7_value *pStack;` |
|        - | 10448 | `	VmFrame *pFrame;` |
|       77 | 10449 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       77 | 10450 | `	if( pCtx == 0 ){` |
|      ! 0 | 10451 | `		return 0;` |
|        - | 10452 | `	}` |
|       77 | 10453 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       77 | 10454 | `	pCtx->pVm = pVm;` |
|       77 | 10455 | `	pCtx->pFunc = pFunc;` |
|       77 | 10456 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       77 | 10457 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       77 | 10458 | `	pCtx->pc = 0;` |
|       77 | 10459 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       77 | 10460 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10461 | `	/* Allocate a private operand stack */` |
|       77 | 10462 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       77 | 10463 | `	if( pStack == 0 ){` |
|      ! 0 | 10464 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10465 | `		return 0;` |
|        - | 10466 | `	}` |
|       77 | 10467 | `	pCtx->pStack = pStack;` |
|        - | 10468 | `	/* Create a detached frame for the fiber */` |
|       77 | 10469 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       77 | 10470 | `	if( pFrame == 0 ){` |
|      ! 0 | 10471 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10472 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10473 | `		return 0;` |
|        - | 10474 | `	}` |
|       77 | 10475 | `	pCtx->pFrame = pFrame;` |
|       77 | 10476 | `	return pCtx;` |
|       41 | 10477 |  |
|        - | 10478 | `/*` |
|        - | 10479 | ` * Start executing a fiber context for the first time.` |
|        - | 10480 | ` */` |
|       68 | 10481 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        5 | 10482 |  |
|        - | 10483 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10484 | `	sxi32 rc;` |
|       73 | 10485 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10486 | `		return SXERR_INVALID;` |
|        - | 10487 | `	}` |
|        - | 10488 | `	/* Bound fiber/generator nesting under the same cap (each start adds a C` |
|        - | 10489 | `	 * frame); reject before mutating VM state so the abort is clean. */` |
|       73 | 10490 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10491 | `		return VmRecursionFatal(pVm);` |
|        - | 10492 | `	}` |
|        - | 10493 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       73 | 10494 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       73 | 10495 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10496 | `	/* Save and set the active context */` |
|       73 | 10497 | `	pOldCtx = pVm->pActiveCtx;` |
|       73 | 10498 | `	pVm->pActiveCtx = pCtx;` |
|       73 | 10499 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       73 | 10500 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       73 | 10501 | `	pVm->nRecursionDepth++;` |
|        - | 10502 | `	/* Execute from the beginning */` |
|       73 | 10503 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       34 | 10504 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       68 | 10505 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0, FALSE);` |
|       73 | 10506 | `	pVm->nRecursionDepth--;` |
|        - | 10507 | `	/* Restore the previous context */` |
|       73 | 10508 | `	pVm->pActiveCtx = pOldCtx;` |
|       73 | 10509 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10510 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       69 | 10511 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       69 | 10512 | `		pCtx->pFrame->pParent = 0;` |
|       69 | 10513 | `		if( pResult ){` |
|       27 | 10514 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10515 | `		}` |
|       69 | 10516 | `		return SXRET_OK;` |
|        - | 10517 | `	}` |
|        - | 10518 | `	/* Detach frame */` |
|        6 | 10519 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        6 | 10520 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        6 | 10521 | `		pCtx->pFrame->pParent = 0;` |
|        2 | 10522 | `	}` |
|        6 | 10523 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10524 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10525 | `		return PH7_ABORT;` |
|        - | 10526 | `	}` |
|        6 | 10527 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10528 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10529 | `		return PH7_EXCEPTION;` |
|        - | 10530 | `	}` |
|        - | 10531 | `	/* Normal completion */` |
|        6 | 10532 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        6 | 10533 | `	if( pResult ){` |
|        3 | 10534 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10535 | `	}` |
|        6 | 10536 | `	return SXRET_OK;` |
|       39 | 10537 |  |
|        - | 10538 | `/*` |
|        - | 10539 | ` * Resume a suspended fiber context.` |
|        - | 10540 | ` */` |
|      150 | 10541 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        5 | 10542 |  |
|        - | 10543 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10544 | `	sxi32 rc;` |
|      155 | 10545 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10546 | `		return SXERR_INVALID;` |
|        - | 10547 | `	}` |
|        - | 10548 | `	/* Bound fiber/generator nesting under the same cap; reject before mutating` |
|        - | 10549 | `	 * VM state so the abort is clean. */` |
|      155 | 10550 | `	if( VmRecursionExceeded(pVm) ){` |
|      ! 0 | 10551 | `		return VmRecursionFatal(pVm);` |
|        - | 10552 | `	}` |
|        - | 10553 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10554 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10555 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      155 | 10556 | `	if( pResumeValue ){` |
|       43 | 10557 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       24 | 10558 | `	}else{` |
|      117 | 10559 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10560 | `	}` |
|      155 | 10561 | `	pCtx->nTos++;` |
|        - | 10562 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      155 | 10563 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      155 | 10564 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10565 | `	/* Save and set the active context */` |
|      155 | 10566 | `	pOldCtx = pVm->pActiveCtx;` |
|      155 | 10567 | `	pVm->pActiveCtx = pCtx;` |
|      155 | 10568 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      155 | 10569 | `	pVm->nRecursionDepth++;` |
|        - | 10570 | `	/* Resume execution from saved PC */` |
|      155 | 10571 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       75 | 10572 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|      150 | 10573 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0, FALSE);` |
|      155 | 10574 | `	pVm->nRecursionDepth--;` |
|        - | 10575 | `	/* Restore the previous context */` |
|      155 | 10576 | `	pVm->pActiveCtx = pOldCtx;` |
|      155 | 10577 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10578 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|      103 | 10579 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|      103 | 10580 | `		pCtx->pFrame->pParent = 0;` |
|      103 | 10581 | `		if( pResult ){` |
|       20 | 10582 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10583 | `		}` |
|      103 | 10584 | `		return SXRET_OK;` |
|        - | 10585 | `	}` |
|        - | 10586 | `	/* Detach frame */` |
|       57 | 10587 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       57 | 10588 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       57 | 10589 | `		pCtx->pFrame->pParent = 0;` |
|       26 | 10590 | `	}` |
|       57 | 10591 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10592 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10593 | `		return PH7_ABORT;` |
|        - | 10594 | `	}` |
|       57 | 10595 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10596 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10597 | `		return PH7_EXCEPTION;` |
|        - | 10598 | `	}` |
|        - | 10599 | `	/* Normal completion */` |
|       57 | 10600 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       57 | 10601 | `	if( pResult ){` |
|       23 | 10602 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10603 | `	}` |
|       57 | 10604 | `	return SXRET_OK;` |
|       80 | 10605 |  |
|        - | 10606 | `/*` |
|        - | 10607 | ` * Release an execution context and all its resources.` |
|        - | 10608 | ` */` |
|        4 | 10609 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10610 |  |
|        5 | 10611 | `	if( pCtx == 0 ){` |
|      ! 0 | 10612 | `		return;` |
|        - | 10613 | `	}` |
|        5 | 10614 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10615 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10616 | `		return;` |
|        - | 10617 | `	}` |
|        5 | 10618 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10619 | `	/* Release values */` |
|        5 | 10620 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10621 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10622 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10623 | `	if( pCtx->pFrame ){` |
|        - | 10624 | `		VmSlot *aSlot;` |
|        - | 10625 | `		sxu32 n;` |
|        - | 10626 | `		/* Free local variables */` |
|        5 | 10627 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10628 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10629 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10630 | `		}` |
|        - | 10631 | `		/* Remove local references */` |
|        5 | 10632 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10633 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10634 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10635 | `		}` |
|        5 | 10636 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10637 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10638 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10639 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10640 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10641 | `		pCtx->pFrame = 0;` |
|        2 | 10642 | `	}` |
|        - | 10643 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10644 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10645 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10646 | `	if( pCtx->pStack ){` |
|        5 | 10647 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10648 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10649 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10650 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10651 | `				pTos--;` |
|        1 | 10652 | `			}` |
|        2 | 10653 | `		}` |
|        5 | 10654 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10655 | `		pCtx->pStack = 0;` |
|        2 | 10656 | `	}` |
|        - | 10657 | `	/* Free the context itself */` |
|        5 | 10658 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10659 |  |
|        - | 10660 | `/*` |
|        - | 10661 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10662 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10663 | ` */` |
|       90 | 10664 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        5 | 10665 |  |
|        - | 10666 | `	ph7_class_instance *pThis;` |
|        - | 10667 | `	SyString sAttr;` |
|        - | 10668 | `	ph7_value *pAttr;` |
|       95 | 10669 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10670 | `		return 0;` |
|        - | 10671 | `	}` |
|       95 | 10672 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       95 | 10673 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10674 | `		return 0;` |
|        - | 10675 | `	}` |
|       95 | 10676 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       95 | 10677 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       95 | 10678 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       35 | 10679 | `		return 0;` |
|        - | 10680 | `	}` |
|       65 | 10681 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       50 | 10682 |  |
|        - | 10683 | `/*` |
|        - | 10684 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10685 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10686 | ` */` |
|       38 | 10687 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10688 |  |
|       43 | 10689 | `	ph7_vm *pVm = pCtx->pVm;` |
|       43 | 10690 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10691 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10692 | `			"Cannot suspend outside of a fiber");` |
|        - | 10693 | `	}` |
|       43 | 10694 | `	if( nArg > 0 ){` |
|       43 | 10695 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       24 | 10696 | `	}else{` |
|      ! 0 | 10697 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10698 | `	}` |
|       43 | 10699 | `	return PH7_SUSPEND;` |
|       24 | 10700 |  |
|        - | 10701 | `/*` |
|        - | 10702 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10703 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10704 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10705 | ` */` |
|       24 | 10706 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10707 |  |
|        - | 10708 | `	ph7_class_instance *pThis;` |
|        - | 10709 | `	ph7_value *pAttr;` |
|        - | 10710 | `	SyString sAttrName;` |
|       29 | 10711 | `	if( nArg < 2 ){` |
|      ! 0 | 10712 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10713 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10714 | `	}` |
|       29 | 10715 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10716 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10717 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10718 | `	}` |
|       29 | 10719 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       29 | 10720 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10721 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10722 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10723 | `	}` |
|        - | 10724 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       29 | 10725 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10726 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10727 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10728 | `	}` |
|        - | 10729 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       29 | 10730 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       29 | 10731 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       29 | 10732 | `	if( pAttr ){` |
|       29 | 10733 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10734 | `	}` |
|       29 | 10735 | `	return PH7_OK;` |
|       17 | 10736 |  |
|        - | 10737 | `/*` |
|        - | 10738 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10739 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10740 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10741 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10742 | ` */` |
|       24 | 10743 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10744 | `	ph7_class_instance **ppThis)` |
|        5 | 10745 |  |
|       29 | 10746 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10747 | `	ph7_value *pCallable;` |
|        - | 10748 | `	SyString sAttrName;` |
|       29 | 10749 | `	*ppThis = 0;` |
|       29 | 10750 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       29 | 10751 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       29 | 10752 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10753 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10754 | `		return 0;` |
|        - | 10755 | `	}` |
|       29 | 10756 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10757 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10758 | `		SyString sName;` |
|        - | 10759 | `		SyHashEntry *pEntry;` |
|        - | 10760 | `		ph7_vm_func *pFunc;` |
|       29 | 10761 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       29 | 10762 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       29 | 10763 | `		if( pEntry == 0 ){` |
|      ! 0 | 10764 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10765 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10766 | `			return 0;` |
|        - | 10767 | `		}` |
|       29 | 10768 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       29 | 10769 | `		return pFunc;` |
|      ! 0 | 10770 | `	}else{` |
|        - | 10771 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10772 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10773 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10774 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10775 | `		if( pMethod == 0 ){` |
|      ! 0 | 10776 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10777 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10778 | `			return 0;` |
|        - | 10779 | `		}` |
|      ! 0 | 10780 | `		*ppThis = pClosure;` |
|      ! 0 | 10781 | `		return &pMethod->sFunc;` |
|        - | 10782 | `	}` |
|       17 | 10783 |  |
|        - | 10784 | `/*` |
|        - | 10785 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10786 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10787 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10788 | ` */` |
|       72 | 10789 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10790 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        5 | 10791 |  |
|       77 | 10792 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10793 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10794 | `	sxu32 nFormal, n;` |
|        - | 10795 | `	VmSlot sSlot;` |
|        - | 10796 | `	sxi32 rc;` |
|        - | 10797 | `	/* Install $this for closure/method callables */` |
|       77 | 10798 | `	if( pClosureThis ){` |
|        - | 10799 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10800 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10801 | `		if( pObj ){` |
|      ! 0 | 10802 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10803 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10804 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10805 | `		}` |
|      ! 0 | 10806 | `	}` |
|        - | 10807 | `	/* Install static variables */` |
|       77 | 10808 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10809 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10810 | `		ph7_value *pVal;` |
|      ! 0 | 10811 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10812 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10813 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10814 | `			if( pVal ){` |
|      ! 0 | 10815 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10816 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10817 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10818 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10819 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10820 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|      ! 0 | 10821 | `				}` |
|      ! 0 | 10822 | `			}` |
|      ! 0 | 10823 | `		}` |
|      ! 0 | 10824 | `	}` |
|        - | 10825 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       77 | 10826 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       77 | 10827 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       95 | 10828 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10829 | `		ph7_value *pObj;` |
|       20 | 10830 | `		if( n < (sxu32)nArg ){` |
|        - | 10831 | `			/* Argument provided — install with type casting */` |
|       20 | 10832 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10833 | `			if( pObj ){` |
|       20 | 10834 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10835 | `				/* Type casting */` |
|       20 | 10836 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10837 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10838 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10839 | `						if( xCast ){` |
|      ! 0 | 10840 | `							xCast(pObj);` |
|      ! 0 | 10841 | `						}` |
|      ! 0 | 10842 | `					}` |
|      ! 0 | 10843 | `				}` |
|       20 | 10844 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10845 | `				sSlot.pUserData = 0;` |
|       20 | 10846 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10847 | `			}` |
|        9 | 10848 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10849 | `			/* Default value */` |
|      ! 0 | 10850 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10851 | `			if( pObj ){` |
|      ! 0 | 10852 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      ! 0 | 10853 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10854 | `					return rc;` |
|        - | 10855 | `				}` |
|      ! 0 | 10856 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10857 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10858 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10859 | `						if( xCast ){` |
|      ! 0 | 10860 | `							xCast(pObj);` |
|      ! 0 | 10861 | `						}` |
|      ! 0 | 10862 | `					}` |
|      ! 0 | 10863 | `				}` |
|      ! 0 | 10864 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10865 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10866 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10867 | `			}` |
|      ! 0 | 10868 | `		}` |
|       11 | 10869 | `	}` |
|        - | 10870 | `	/* Install closure environment (captured variables) */` |
|       77 | 10871 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10872 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10873 | `		ph7_value *pValue;` |
|        - | 10874 | `		sxu32 iEnv;` |
|        3 | 10875 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10876 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10877 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10878 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10879 | `				continue;` |
|        - | 10880 | `			}` |
|        5 | 10881 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10882 | `			if( pValue == 0 ){` |
|      ! 0 | 10883 | `				continue;` |
|        - | 10884 | `			}` |
|        5 | 10885 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10886 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10887 | `		}` |
|        1 | 10888 | `	}` |
|       77 | 10889 | `	return SXRET_OK;` |
|       41 | 10890 |  |
|        - | 10891 | `/*` |
|        - | 10892 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10893 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10894 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10895 | ` */` |
|       26 | 10896 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 10897 |  |
|       31 | 10898 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10899 | `	ph7_class_instance *pThis;` |
|        - | 10900 | `	ph7_class_instance *pClosureThis;` |
|        - | 10901 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10902 | `	ph7_vm_func *pFunc;` |
|        - | 10903 | `	ph7_value sResult;` |
|        - | 10904 | `	ph7_value *pCtxAttr;` |
|        - | 10905 | `	SyString sAttrName;` |
|        - | 10906 | `	sxi32 rc;` |
|       31 | 10907 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10908 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10909 | `	}` |
|       31 | 10910 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10911 | `	/* Check if already started (has a __ctx) */` |
|       31 | 10912 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       31 | 10913 | `	if( pExecCtx != 0 ){` |
|        3 | 10914 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10915 | `			"Cannot start a fiber that has already been started");` |
|        - | 10916 | `	}` |
|        - | 10917 | `	/* Resolve callable */` |
|       29 | 10918 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       29 | 10919 | `	if( pFunc == 0 ){` |
|      ! 0 | 10920 | `		return PH7_EXCEPTION;` |
|        - | 10921 | `	}` |
|        - | 10922 | `	/* Create execution context now that we know the function */` |
|       29 | 10923 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       29 | 10924 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10925 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10926 | `			"Fiber::start(): out of memory");` |
|        - | 10927 | `	}` |
|        - | 10928 | `	/* Store context in $this->__ctx */` |
|       29 | 10929 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       29 | 10930 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       29 | 10931 | `	if( pCtxAttr ){` |
|       29 | 10932 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       29 | 10933 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10934 | `	}` |
|        - | 10935 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10936 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10937 | `	 * into the fiber's frame, not the caller's. */` |
|       29 | 10938 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       29 | 10939 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10940 | `	/* Unpack the args array and install into the frame */` |
|        - | 10941 | `	{` |
|       29 | 10942 | `		ph7_value **apValues = 0;` |
|       29 | 10943 | `		ph7_value *aStore = 0;` |
|       29 | 10944 | `		int nActual = 0;` |
|       29 | 10945 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       29 | 10946 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10947 | `			ph7_hashmap_node *pNode;` |
|       29 | 10948 | `			sxu32 nCount = pMap->nEntry;` |
|       29 | 10949 | `			if( nCount > 0 ){` |
|        3 | 10950 | `				sxu32 idx = 0;` |
|        4 | 10951 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10952 | `					nCount * sizeof(ph7_value *));` |
|        4 | 10953 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10954 | `					nCount * sizeof(ph7_value));` |
|        3 | 10955 | `				if( apValues && aStore ){` |
|        3 | 10956 | `					pNode = pMap->pFirst;` |
|        7 | 10957 | `					while( pNode && idx < nCount ){` |
|        - | 10958 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 10959 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 10960 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 10961 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 10962 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 10963 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 10964 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10965 | `						if( pSrc ){` |
|        5 | 10966 | `							aStore[idx] = *pSrc;` |
|        3 | 10967 | `						}else{` |
|      ! 0 | 10968 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 10969 | `						}` |
|        5 | 10970 | `						apValues[idx] = &aStore[idx];` |
|        5 | 10971 | `						idx++;` |
|        5 | 10972 | `						pNode = pNode->pPrev;` |
|        1 | 10973 | `					}` |
|        3 | 10974 | `					nActual = (int)idx;` |
|        1 | 10975 | `				}` |
|        1 | 10976 | `			}` |
|       12 | 10977 | `		}` |
|       29 | 10978 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       29 | 10979 | `		if( aStore ){` |
|        3 | 10980 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 10981 | `		}` |
|       29 | 10982 | `		if( apValues ){` |
|        3 | 10983 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10984 | `		}` |
|        - | 10985 | `	}` |
|        - | 10986 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       29 | 10987 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       29 | 10988 | `	pExecCtx->pFrame->pParent = 0;` |
|       29 | 10989 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10990 | `		return PH7_ABORT;` |
|        - | 10991 | `	}` |
|       29 | 10992 | `	PH7_MemObjInit(pVm, &sResult);` |
|       29 | 10993 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       29 | 10994 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10995 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10996 | `		return PH7_ABORT;` |
|        - | 10997 | `	}` |
|       29 | 10998 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10999 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11000 | `		return PH7_EXCEPTION;` |
|        - | 11001 | `	}` |
|       29 | 11002 | `	ph7_result_value(pCtx, &sResult);` |
|       29 | 11003 | `	PH7_MemObjRelease(&sResult);` |
|       29 | 11004 | `	return PH7_OK;` |
|       18 | 11005 |  |
|        - | 11006 | `/*` |
|        - | 11007 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 11008 | ` */` |
|       36 | 11009 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11010 |  |
|       41 | 11011 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11012 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 11013 | `	ph7_value sResult;` |
|        - | 11014 | `	ph7_value *pResumeVal;` |
|        - | 11015 | `	sxi32 rc;` |
|       41 | 11016 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11017 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 11018 | `		return PH7_OK;` |
|        - | 11019 | `	}` |
|       41 | 11020 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       41 | 11021 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11022 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 11023 | `		return PH7_OK;` |
|        - | 11024 | `	}` |
|       41 | 11025 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11026 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11027 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 11028 | `	}` |
|       39 | 11029 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       39 | 11030 | `	PH7_MemObjInit(pVm, &sResult);` |
|       39 | 11031 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       39 | 11032 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 11033 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11034 | `		return PH7_ABORT;` |
|        - | 11035 | `	}` |
|       39 | 11036 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 11037 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 11038 | `		return PH7_EXCEPTION;` |
|        - | 11039 | `	}` |
|       39 | 11040 | `	ph7_result_value(pCtx, &sResult);` |
|       39 | 11041 | `	PH7_MemObjRelease(&sResult);` |
|       39 | 11042 | `	return PH7_OK;` |
|       23 | 11043 |  |
|        - | 11044 | `/*` |
|        - | 11045 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 11046 | ` */` |
|        6 | 11047 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 11048 |  |
|        9 | 11049 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11050 | `	ph7_exec_ctx *pExecCtx;` |
|        9 | 11051 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11052 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11053 | `		return PH7_OK;` |
|        - | 11054 | `	}` |
|        9 | 11055 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        9 | 11056 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 11057 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11058 | `		return PH7_OK;` |
|        - | 11059 | `	}` |
|        9 | 11060 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11061 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11062 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11063 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 11064 | `		}` |
|      ! 0 | 11065 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 11066 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 11067 | `	}` |
|        9 | 11068 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        9 | 11069 | `	return PH7_OK;` |
|        6 | 11070 |  |
|        - | 11071 | `/*` |
|        - | 11072 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 11073 | ` */` |
|        6 | 11074 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11075 |  |
|        - | 11076 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11077 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11078 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11079 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 11080 | `	return PH7_OK;` |
|        4 | 11081 |  |
|      ! 0 | 11082 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11083 |  |
|        - | 11084 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 11085 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 11086 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11087 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 11088 | `	return PH7_OK;` |
|      ! 0 | 11089 |  |
|        6 | 11090 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11091 |  |
|        - | 11092 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11093 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11094 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11095 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 11096 | `	return PH7_OK;` |
|        4 | 11097 |  |
|        6 | 11098 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11099 |  |
|        - | 11100 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 11101 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 11102 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 11103 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 11104 | `	return PH7_OK;` |
|        4 | 11105 |  |
|        - | 11106 | `/*` |
|        - | 11107 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 11108 | ` */` |
|        4 | 11109 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11110 |  |
|        5 | 11111 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11112 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 11113 | `	if( nArg < 1 ){` |
|      ! 0 | 11114 | `		return PH7_OK;` |
|        - | 11115 | `	}` |
|        5 | 11116 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 11117 | `	if( pExecCtx ){` |
|        5 | 11118 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 11119 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 11120 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 11121 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11122 | `			SyString sAttrName;` |
|        - | 11123 | `			ph7_value *pAttr;` |
|        5 | 11124 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 11125 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 11126 | `			if( pAttr ){` |
|        5 | 11127 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 11128 | `			}` |
|        2 | 11129 | `		}` |
|        2 | 11130 | `	}` |
|        5 | 11131 | `	return PH7_OK;` |
|        3 | 11132 |  |
|        - | 11133 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 11134 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 11135 |  |
|        - | 11136 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11137 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 11138 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 11139 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 11140 |  |
|      ! 0 | 11141 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 11142 |  |
|        - | 11143 | `	ph7_class_instance *pThis;` |
|      ! 0 | 11144 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 11145 | `	ph7_exec_ctx *pCtx;` |
|        - | 11146 | `	ph7_vm_func *pFunc;` |
|        - | 11147 | `	ph7_value *pCallable;` |
|        - | 11148 | `	ph7_value *pCtxAttr;` |
|        - | 11149 | `	SyString sAttrName;` |
|        - | 11150 | `	/* Must not already be started */` |
|      ! 0 | 11151 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11152 | `	if( pCtx != 0 ){` |
|      ! 0 | 11153 | `		return SXERR_INVALID;` |
|        - | 11154 | `	}` |
|      ! 0 | 11155 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11156 | `		return SXERR_INVALID;` |
|        - | 11157 | `	}` |
|      ! 0 | 11158 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 11159 | `	/* Get the callable */` |
|      ! 0 | 11160 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 11161 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11162 | `	if( pCallable == 0 ){` |
|      ! 0 | 11163 | `		return SXERR_INVALID;` |
|        - | 11164 | `	}` |
|        - | 11165 | `	/* Resolve callable */` |
|      ! 0 | 11166 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 11167 | `		SyString sName;` |
|        - | 11168 | `		SyHashEntry *pEntry;` |
|      ! 0 | 11169 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 11170 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 11171 | `		if( pEntry == 0 ){` |
|      ! 0 | 11172 | `			return SXERR_NOTFOUND;` |
|        - | 11173 | `		}` |
|      ! 0 | 11174 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 11175 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11176 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 11177 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 11178 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 11179 | `		if( pMethod == 0 ){` |
|      ! 0 | 11180 | `			return SXERR_INVALID;` |
|        - | 11181 | `		}` |
|      ! 0 | 11182 | `		pClosureThis = pClosure;` |
|      ! 0 | 11183 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 11184 | `	}else{` |
|      ! 0 | 11185 | `		return SXERR_INVALID;` |
|        - | 11186 | `	}` |
|        - | 11187 | `	/* Create context */` |
|      ! 0 | 11188 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 11189 | `	if( pCtx == 0 ){` |
|      ! 0 | 11190 | `		return SXERR_MEM;` |
|        - | 11191 | `	}` |
|        - | 11192 | `	/* Store in __ctx */` |
|      ! 0 | 11193 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11194 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11195 | `	if( pCtxAttr ){` |
|      ! 0 | 11196 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 11197 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 11198 | `	}` |
|        - | 11199 | `	/* Set up frame with args */` |
|      ! 0 | 11200 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 11201 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 11202 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 11203 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 11204 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 11205 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 11206 |  |
|      ! 0 | 11207 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 11208 |  |
|      ! 0 | 11209 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11210 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 11211 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 11212 |  |
|      ! 0 | 11213 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11214 |  |
|      ! 0 | 11215 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11216 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 11217 |  |
|      ! 0 | 11218 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11219 |  |
|      ! 0 | 11220 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11221 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 11222 |  |
|      ! 0 | 11223 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 11224 |  |
|      ! 0 | 11225 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 11226 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 11227 | `	return &pCtx->sRetValue;` |
|      ! 0 | 11228 |  |
|        - | 11229 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 11230 | `/*` |
|        - | 11231 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 11232 | ` */` |
|       48 | 11233 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 | 11234 |  |
|        - | 11235 | `	ph7_generator *pGen;` |
|       53 | 11236 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       53 | 11237 | `	if( pGen == 0 ){` |
|      ! 0 | 11238 | `		return 0;` |
|        - | 11239 | `	}` |
|       53 | 11240 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       53 | 11241 | `	pGen->pCtx = pCtx;` |
|       53 | 11242 | `	pGen->iImplicitKey = 0;` |
|       53 | 11243 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       53 | 11244 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 11245 | `	/* Link the generator back to the exec context */` |
|       53 | 11246 | `	pCtx->pPrivate = pGen;` |
|       53 | 11247 | `	return pGen;` |
|       29 | 11248 |  |
|        - | 11249 | `/*` |
|        - | 11250 | ` * Release a generator and its execution context.` |
|        - | 11251 | ` */` |
|      ! 0 | 11252 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 11253 |  |
|      ! 0 | 11254 | `	if( pGen == 0 ){` |
|      ! 0 | 11255 | `		return;` |
|        - | 11256 | `	}` |
|      ! 0 | 11257 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 11258 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 11259 | `	if( pGen->pCtx ){` |
|      ! 0 | 11260 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 11261 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 11262 | `		pGen->pCtx = 0;` |
|      ! 0 | 11263 | `	}` |
|      ! 0 | 11264 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 11265 |  |
|        - | 11266 | `/*` |
|        - | 11267 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 11268 | ` */` |
|      496 | 11269 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        5 | 11270 |  |
|        - | 11271 | `	ph7_class_instance *pThis;` |
|        - | 11272 | `	SyString sAttr;` |
|        - | 11273 | `	ph7_value *pAttr;` |
|      501 | 11274 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 11275 | `		return 0;` |
|        - | 11276 | `	}` |
|      501 | 11277 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      501 | 11278 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 11279 | `		return 0;` |
|        - | 11280 | `	}` |
|      501 | 11281 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      501 | 11282 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      501 | 11283 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 11284 | `		return 0;` |
|        - | 11285 | `	}` |
|      501 | 11286 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      253 | 11287 |  |
|        - | 11288 | `/*` |
|        - | 11289 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 11290 | ` */` |
|       44 | 11291 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11292 |  |
|        - | 11293 | `	ph7_generator *pGen;` |
|        - | 11294 | `	sxi32 rc;` |
|       49 | 11295 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 | 11296 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 | 11297 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 | 11298 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       49 | 11299 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 | 11300 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 | 11301 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       22 | 11302 | `	}` |
|       49 | 11303 | `	return PH7_OK;` |
|       27 | 11304 |  |
|        - | 11305 | `/*` |
|        - | 11306 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 11307 | ` */` |
|      142 | 11308 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 11309 |  |
|        - | 11310 | `	ph7_generator *pGen;` |
|      146 | 11311 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      146 | 11312 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      146 | 11313 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|      146 | 11314 | `	return PH7_OK;` |
|       75 | 11315 |  |
|        - | 11316 | `/*` |
|        - | 11317 | ` * Generator::current() — return the last yielded value.` |
|        - | 11318 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11319 | ` */` |
|      124 | 11320 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11321 |  |
|        - | 11322 | `	ph7_generator *pGen;` |
|        - | 11323 | `	sxi32 rc;` |
|      129 | 11324 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      129 | 11325 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      129 | 11326 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      129 | 11327 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11328 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11329 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11330 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11331 | `	}` |
|      129 | 11332 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      129 | 11333 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       67 | 11334 | `	}else{` |
|      ! 0 | 11335 | `		ph7_result_null(pCtx);` |
|        - | 11336 | `	}` |
|      129 | 11337 | `	return PH7_OK;` |
|       67 | 11338 |  |
|        - | 11339 | `/*` |
|        - | 11340 | ` * Generator::key() — return the last yielded key.` |
|        - | 11341 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 11342 | ` */` |
|       68 | 11343 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 11344 |  |
|        - | 11345 | `	ph7_generator *pGen;` |
|        - | 11346 | `	sxi32 rc;` |
|       71 | 11347 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       71 | 11348 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       71 | 11349 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       71 | 11350 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11351 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11352 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11353 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11354 | `	}` |
|       71 | 11355 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       71 | 11356 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|       37 | 11357 | `	}else{` |
|      ! 0 | 11358 | `		ph7_result_null(pCtx);` |
|        - | 11359 | `	}` |
|       71 | 11360 | `	return PH7_OK;` |
|       37 | 11361 |  |
|        - | 11362 | `/*` |
|        - | 11363 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11364 | ` */` |
|      112 | 11365 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 11366 |  |
|        - | 11367 | `	ph7_generator *pGen;` |
|        - | 11368 | `	sxi32 rc;` |
|      117 | 11369 | `	if( nArg < 1 ) return PH7_OK;` |
|      117 | 11370 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      117 | 11371 | `	if( pGen == 0 ) return PH7_OK;` |
|      117 | 11372 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11373 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      117 | 11374 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      117 | 11375 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       61 | 11376 | `	}else{` |
|      ! 0 | 11377 | `		return PH7_OK;` |
|        - | 11378 | `	}` |
|      117 | 11379 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      117 | 11380 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      117 | 11381 | `	return PH7_OK;` |
|       61 | 11382 |  |
|        - | 11383 | `/*` |
|        - | 11384 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11385 | ` */` |
|        4 | 11386 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11387 |  |
|        - | 11388 | `	ph7_generator *pGen;` |
|        - | 11389 | `	ph7_value *pSendVal;` |
|        - | 11390 | `	sxi32 rc;` |
|        5 | 11391 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11392 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11393 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11394 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11395 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11396 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11397 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11398 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11399 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11400 | `	}else{` |
|      ! 0 | 11401 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11402 | `		return PH7_OK;` |
|        - | 11403 | `	}` |
|        5 | 11404 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11405 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11406 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11407 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11408 | `	}else{` |
|        3 | 11409 | `		ph7_result_null(pCtx);` |
|        - | 11410 | `	}` |
|        5 | 11411 | `	return PH7_OK;` |
|        3 | 11412 |  |
|        - | 11413 | `/*` |
|        - | 11414 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11415 | ` *` |
|        - | 11416 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11417 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11418 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11419 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11420 | ` * the exception to the caller.` |
|        - | 11421 | ` */` |
|      ! 0 | 11422 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11423 |  |
|        - | 11424 | `	ph7_generator *pGen;` |
|        - | 11425 | `	const char *zMsg;` |
|        - | 11426 | `	int nLen;` |
|      ! 0 | 11427 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11428 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11429 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11430 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11431 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11432 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11433 | `			"Cannot throw into a closed generator");` |
|        - | 11434 | `	}` |
|        - | 11435 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11436 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11437 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11438 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11439 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11440 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11441 | `	nLen = 0;` |
|      ! 0 | 11442 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11443 | `		/* Try to get the exception's message */` |
|        - | 11444 | `		SyString sAttr;` |
|        - | 11445 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11446 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11447 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11448 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11449 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11450 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11451 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11452 | `		}` |
|      ! 0 | 11453 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11454 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11455 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11456 | `	}` |
|      ! 0 | 11457 | `	(void)nLen;` |
|      ! 0 | 11458 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11459 |  |
|        - | 11460 | `/*` |
|        - | 11461 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11462 | ` */` |
|        2 | 11463 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11464 |  |
|        - | 11465 | `	ph7_generator *pGen;` |
|        3 | 11466 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11467 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11468 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11469 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11470 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11471 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11472 | `	}` |
|        3 | 11473 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11474 | `	return PH7_OK;` |
|        2 | 11475 |  |
|        - | 11476 | `/*` |
|        - | 11477 | ` * Generator::__destruct() — clean up.` |
|        - | 11478 | ` */` |
|      ! 0 | 11479 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11480 |  |
|        - | 11481 | `	ph7_generator *pGen;` |
|      ! 0 | 11482 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11483 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11484 | `	if( pGen ){` |
|      ! 0 | 11485 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11486 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11487 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11488 | `			SyString sAttrName;` |
|        - | 11489 | `			ph7_value *pAttr;` |
|      ! 0 | 11490 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11491 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11492 | `			if( pAttr ){` |
|      ! 0 | 11493 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11494 | `			}` |
|      ! 0 | 11495 | `		}` |
|      ! 0 | 11496 | `	}` |
|      ! 0 | 11497 | `	return PH7_OK;` |
|      ! 0 | 11498 |  |
|        - | 11499 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11500 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11501 | `/*` |
|        - | 11502 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11503 | ` * the desired message.` |
|        - | 11504 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11505 | ` * in 'api.c' for additional information.` |
|        - | 11506 | ` */` |
|      370 | 11507 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11508 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11509 | `	SyString *pString /* Message to output */` |
|        - | 11510 | `	)` |
|        3 | 11511 |  |
|      373 | 11512 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      373 | 11513 | `	sxi32 rc = SXRET_OK;` |
|        - | 11514 | `	/* Call the output consumer */` |
|      373 | 11515 | `	if( pString->nByte > 0 ){` |
|      373 | 11516 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      373 | 11517 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11518 | `	}` |
|      373 | 11519 | `	return rc;` |
|        3 | 11520 |  |
|        - | 11521 | `/*` |
|        - | 11522 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11523 | ` * callback to consume the formatted message.` |
|        - | 11524 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11525 | ` * in 'api.c' for additional information.` |
|        - | 11526 | ` */` |
|        2 | 11527 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11528 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11529 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11530 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11531 | `	)` |
|        1 | 11532 |  |
|        3 | 11533 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11534 | `	sxi32 rc = SXRET_OK;` |
|        - | 11535 | `	SyBlob sWorker;` |
|        - | 11536 | `	/* Format the message and call the output consumer */` |
|        3 | 11537 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11538 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11539 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11540 | `		/* Consume the formatted message */` |
|        3 | 11541 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11542 | `	}` |
|        3 | 11543 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11544 | `	/* Release the working buffer */` |
|        3 | 11545 | `	SyBlobRelease(&sWorker);` |
|        3 | 11546 | `	return rc;` |
|        1 | 11547 |  |
|        - | 11548 | `/*` |
|        - | 11549 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11550 | ` * This function never fail and always return a pointer` |
|        - | 11551 | ` * to a null terminated string.` |
|        - | 11552 | ` */` |
|       12 | 11553 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11554 |  |
|       13 | 11555 | `	const char *zOp = "Unknown     ";` |
|       13 | 11556 | `	switch(nOp){` |
|        3 | 11557 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11558 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11559 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11560 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11561 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11562 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11563 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11564 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11565 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11566 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11567 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11568 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11569 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11570 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11571 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11572 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11573 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11574 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11575 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11576 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11577 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11578 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11579 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11580 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11581 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11582 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11583 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11584 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11585 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11586 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11587 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11588 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11589 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11590 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11591 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11592 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11593 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11594 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11595 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11596 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11597 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11598 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11599 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11600 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11601 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11602 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11603 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11604 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11605 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11606 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11607 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11608 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11609 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11610 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11611 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11612 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11613 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11614 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11615 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11616 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11617 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11618 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11619 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11620 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11621 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11622 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11623 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11624 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11625 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11626 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11627 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11628 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11629 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11630 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11631 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11632 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11633 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11634 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11635 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11636 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11637 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11638 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11639 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11640 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11641 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11642 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11643 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11644 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11645 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11646 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11647 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11648 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11649 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11650 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11651 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11652 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11653 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11654 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11655 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11656 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11657 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11658 | `	default:` |
|      ! 0 | 11659 | `		break;` |
|        - | 11660 | `	}` |
|       13 | 11661 | `	return zOp;` |
|        1 | 11662 |  |
|        - | 11663 | `/*` |
|        - | 11664 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11665 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11666 | ` * is responsible of consuming the generated dump.` |
|        - | 11667 | ` */` |
|        2 | 11668 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11669 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11670 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11671 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11672 | `	)` |
|        1 | 11673 |  |
|        - | 11674 | `	sxi32 rc;` |
|        3 | 11675 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11676 | `	return rc;` |
|        1 | 11677 |  |
|        - | 11678 | `/*` |
|        - | 11679 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11680 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11681 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11682 | ` * in 'compile.c' for additional information.` |
|        - | 11683 | ` */` |
|       14 | 11684 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11685 |  |
|       15 | 11686 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11687 | `	/* Evaluate and expand constant value */` |
|       15 | 11688 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|       15 | 11689 |  |
|        - | 11690 | `/*` |
|        - | 11691 | ` * Section:` |
|        - | 11692 | ` *  Function handling functions.` |
|        - | 11693 | ` * Status:` |
|        - | 11694 | ` *    Stable.` |
|        - | 11695 | ` */` |
|        - | 11696 | `/*` |
|        - | 11697 | ` * int func_num_args(void)` |
|        - | 11698 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11699 | ` * Parameters` |
|        - | 11700 | ` *   None.` |
|        - | 11701 | ` * Return` |
|        - | 11702 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11703 | ` *  or -1 if called from the globe scope.` |
|        - | 11704 | ` */` |
|      986 | 11705 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 11706 |  |
|        - | 11707 | `	VmFrame *pFrame;` |
|        - | 11708 | `	ph7_vm *pVm;` |
|        - | 11709 | `	/* Point to the target VM */` |
|      991 | 11710 | `	pVm = pCtx->pVm;` |
|        - | 11711 | `	/* Current frame */` |
|      991 | 11712 | `	pFrame = pVm->pFrame;` |
|      991 | 11713 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      991 | 11714 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11715 | `		SXUNUSED(nArg);` |
|      ! 0 | 11716 | `		SXUNUSED(apArg);` |
|        - | 11717 | `		/* Global frame,return -1 */` |
|      ! 0 | 11718 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11719 | `		return SXRET_OK;` |
|        - | 11720 | `	}` |
|        - | 11721 | `	/* Total number of arguments passed to the enclosing function */` |
|      991 | 11722 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      991 | 11723 | `	ph7_result_int(pCtx,nArg);` |
|      991 | 11724 | `	return SXRET_OK;` |
|      498 | 11725 |  |
|        - | 11726 | `/*` |
|        - | 11727 | ` * value func_get_arg(int $arg_num)` |
|        - | 11728 | ` *   Return an item from the argument list.` |
|        - | 11729 | ` * Parameters` |
|        - | 11730 | ` *  Argument number(index start from zero).` |
|        - | 11731 | ` * Return` |
|        - | 11732 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11733 | ` */` |
|       22 | 11734 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11735 |  |
|       24 | 11736 | `	ph7_value *pObj = 0;` |
|       24 | 11737 | `	VmSlot *pSlot = 0;` |
|        - | 11738 | `	VmFrame *pFrame;` |
|        - | 11739 | `	ph7_vm *pVm;` |
|        - | 11740 | `	/* Point to the target VM */` |
|       24 | 11741 | `	pVm = pCtx->pVm;` |
|        - | 11742 | `	/* Current frame */` |
|       24 | 11743 | `	pFrame = pVm->pFrame;` |
|       24 | 11744 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11745 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11746 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11747 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11748 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11749 | `		return SXRET_OK;` |
|        - | 11750 | `	}` |
|        - | 11751 | `	/* Extract the desired index */` |
|       21 | 11752 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11753 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11754 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11755 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11756 | `		return SXRET_OK;` |
|        - | 11757 | `	}` |
|        - | 11758 | `	/* Extract the desired argument */` |
|       21 | 11759 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11760 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11761 | `			/* Return the desired argument */` |
|       21 | 11762 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11763 | `		}else{` |
|        - | 11764 | `			/* No such argument,return false */` |
|      ! 0 | 11765 | `			ph7_result_bool(pCtx,0);` |
|        - | 11766 | `		}` |
|       11 | 11767 | `	}else{` |
|        - | 11768 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11769 | `		ph7_result_bool(pCtx,0);` |
|        - | 11770 | `	}` |
|       21 | 11771 | `	return SXRET_OK;` |
|       13 | 11772 |  |
|        - | 11773 | `/*` |
|        - | 11774 | ` * array func_get_args_byref(void)` |
|        - | 11775 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11776 | ` * Parameters` |
|        - | 11777 | ` *  None.` |
|        - | 11778 | ` * Return` |
|        - | 11779 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11780 | ` *  member of the current user-defined function's argument list.` |
|        - | 11781 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11782 | ` * NOTE:` |
|        - | 11783 | ` *  Arguments are returned to the array by reference.` |
|        - | 11784 | ` */` |
|        2 | 11785 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11786 |  |
|        - | 11787 | `	ph7_value *pArray;` |
|        - | 11788 | `	VmFrame *pFrame;` |
|        - | 11789 | `	VmSlot *aSlot;` |
|        - | 11790 | `	sxu32 n;` |
|        - | 11791 | `	/* Point to the current frame */` |
|        3 | 11792 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11793 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11794 | `	if( pFrame->pParent == 0 ){` |
|        - | 11795 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11796 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11797 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11798 | `		return SXRET_OK;` |
|        - | 11799 | `	}` |
|        - | 11800 | `	/* Create a new array */` |
|        3 | 11801 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11802 | `	if( pArray == 0 ){` |
|      ! 0 | 11803 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11804 | `		SXUNUSED(apArg);` |
|      ! 0 | 11805 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11806 | `		return SXRET_OK;` |
|        - | 11807 | `	}` |
|        - | 11808 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11809 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11810 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11811 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11812 | `	}` |
|        - | 11813 | `	/* Return the freshly created array */` |
|        3 | 11814 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11815 | `	return SXRET_OK;` |
|        2 | 11816 |  |
|        - | 11817 | `/*` |
|        - | 11818 | ` * array func_get_args(void)` |
|        - | 11819 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11820 | ` * Parameters` |
|        - | 11821 | ` *  None.` |
|        - | 11822 | ` * Return` |
|        - | 11823 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11824 | ` *  member of the current user-defined function's argument list.` |
|        - | 11825 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11826 | ` */` |
|       88 | 11827 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 11828 |  |
|       93 | 11829 | `	ph7_value *pObj = 0;` |
|        - | 11830 | `	ph7_value *pArray;` |
|        - | 11831 | `	VmFrame *pFrame;` |
|        - | 11832 | `	VmSlot *aSlot;` |
|        - | 11833 | `	sxu32 n;` |
|        - | 11834 | `	/* Point to the current frame */` |
|       93 | 11835 | `	pFrame = pCtx->pVm->pFrame;` |
|       93 | 11836 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       93 | 11837 | `	if( pFrame->pParent == 0 ){` |
|        - | 11838 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11839 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11840 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11841 | `		return SXRET_OK;` |
|        - | 11842 | `	}` |
|        - | 11843 | `	/* Create a new array */` |
|       93 | 11844 | `	pArray = ph7_context_new_array(pCtx);` |
|       93 | 11845 | `	if( pArray == 0 ){` |
|      ! 0 | 11846 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11847 | `		SXUNUSED(apArg);` |
|      ! 0 | 11848 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11849 | `		return SXRET_OK;` |
|        - | 11850 | `	}` |
|        - | 11851 | `	/* Start filling the array with the given arguments */` |
|       93 | 11852 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      225 | 11853 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11854 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11855 | `		if( pObj ){` |
|      134 | 11856 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11857 | `		}` |
|       68 | 11858 | `	}` |
|        - | 11859 | `	/* Return the freshly created array */` |
|       93 | 11860 | `	ph7_result_value(pCtx,pArray);` |
|       93 | 11861 | `	return SXRET_OK;` |
|       49 | 11862 |  |
|        - | 11863 | `/*` |
|        - | 11864 | ` * bool function_exists(string $name)` |
|        - | 11865 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11866 | ` * Parameters` |
|        - | 11867 | ` *  The name of the desired function.` |
|        - | 11868 | ` * Return` |
|        - | 11869 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11870 | ` */` |
|     1748 | 11871 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 11872 |  |
|        - | 11873 | `	const char *zName;` |
|        - | 11874 | `	ph7_vm *pVm;` |
|        - | 11875 | `	int nLen;` |
|        - | 11876 | `	int res;` |
|     1753 | 11877 | `	if( nArg < 1 ){` |
|        - | 11878 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11879 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11880 | `		return SXRET_OK;` |
|        - | 11881 | `	}` |
|        - | 11882 | `	/* Point to the target VM */` |
|     1753 | 11883 | `	pVm = pCtx->pVm;` |
|        - | 11884 | `	/* Extract the function name */` |
|     1753 | 11885 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11886 | `	/* Assume the function is not defined */` |
|     1753 | 11887 | `	res = 0;` |
|        - | 11888 | `	/* Perform the lookup */` |
|     2625 | 11889 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1744 | 11890 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11891 | `			/* Function is defined */` |
|      271 | 11892 | `			res = 1;` |
|      133 | 11893 | `	}` |
|     1753 | 11894 | `	ph7_result_bool(pCtx,res);` |
|     1753 | 11895 | `	return SXRET_OK;` |
|      879 | 11896 |  |
|        - | 11897 | `/*` |
|        - | 11898 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11899 | ` * [i.e: Whether it is callable or not].` |
|        - | 11900 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11901 | ` */` |
|    24250 | 11902 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        5 | 11903 |  |
|    24255 | 11904 | `	int res = 0;` |
|    24255 | 11905 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11906 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11907 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11908 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11909 | `		 * standard PHP behavior. */` |
|       21 | 11910 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       21 | 11911 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       19 | 11912 | `			res = 1;` |
|       11 | 11913 | `		}` |
|        9 | 11914 | `		(void)CallInvoke;` |
|    24246 | 11915 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       30 | 11916 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       30 | 11917 | `		if( pMap->nEntry == 2 ){` |
|        - | 11918 | `			ph7_class *pClass;` |
|        - | 11919 | `			ph7_value *pV;` |
|        - | 11920 | `			/* Extract the target class */` |
|       13 | 11921 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       13 | 11922 | `			if( pV ){` |
|       13 | 11923 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       13 | 11924 | `				if( pClass ){` |
|        - | 11925 | `					ph7_class_method *pMethod;` |
|        - | 11926 | `					/* Extract the target method */` |
|       10 | 11927 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11928 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11929 | `						/* Perform the lookup */` |
|       10 | 11930 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11931 | `						if( pMethod ){` |
|        - | 11932 | `							/* Method is callable */` |
|        5 | 11933 | `							res = 1;` |
|        2 | 11934 | `						}` |
|        4 | 11935 | `					}` |
|        4 | 11936 | `				}` |
|        5 | 11937 | `			}` |
|        9 | 11938 | `		}` |
|    24224 | 11939 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11940 | `		const char *zName;` |
|        - | 11941 | `		int nLen;` |
|        - | 11942 | `		/* Extract the name */` |
|     5955 | 11943 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11944 | `		/* Perform the lookup */` |
|     5970 | 11945 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11946 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11947 | `				/* Function is callable */` |
|     5937 | 11948 | `				res = 1;` |
|     2966 | 11949 | `		}` |
|     2975 | 11950 | `	}` |
|    24255 | 11951 | `	return res;` |
|        5 | 11952 |  |
|        - | 11953 | `/*` |
|        - | 11954 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11955 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11956 | ` * Parameters` |
|        - | 11957 | ` * $name` |
|        - | 11958 | ` *    The callback function to check` |
|        - | 11959 | ` * $syntax_only` |
|        - | 11960 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11961 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11962 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11963 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11964 | ` *    a string.` |
|        - | 11965 | ` * Return` |
|        - | 11966 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11967 | ` */` |
|       20 | 11968 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11969 |  |
|        - | 11970 | `	ph7_vm *pVm;` |
|        - | 11971 | `	int res;` |
|       21 | 11972 | `	if( nArg < 1 ){` |
|        - | 11973 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11974 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11975 | `		return SXRET_OK;` |
|        - | 11976 | `	}` |
|        - | 11977 | `	/* Point to the target VM */` |
|       21 | 11978 | `	pVm = pCtx->pVm;` |
|        - | 11979 | `	/* Perform the requested operation */` |
|       21 | 11980 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11981 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11982 | `	return SXRET_OK;` |
|       11 | 11983 |  |
|        - | 11984 | `/*` |
|        - | 11985 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11986 | ` * defined below.` |
|        - | 11987 | ` */` |
|     1312 | 11988 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11989 |  |
|     1313 | 11990 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11991 | `	ph7_value sName;` |
|        - | 11992 | `	sxi32 rc;` |
|        - | 11993 | `	/* Prepare the function name for insertion */` |
|     1313 | 11994 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1313 | 11995 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11996 | `	/* Perform the insertion */` |
|     1313 | 11997 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1313 | 11998 | `	PH7_MemObjRelease(&sName);` |
|     1313 | 11999 | `	return rc;` |
|        1 | 12000 |  |
|        - | 12001 | `/*` |
|        - | 12002 | ` * array get_defined_functions(void)` |
|        - | 12003 | ` *  Returns an array of all defined functions.` |
|        - | 12004 | ` * Parameter` |
|        - | 12005 | ` *  None.` |
|        - | 12006 | ` * Return` |
|        - | 12007 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 12008 | ` *  both built-in (internal) and user-defined.` |
|        - | 12009 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 12010 | ` *  defined ones using $arr["user"].` |
|        - | 12011 | ` * Note:` |
|        - | 12012 | ` *  NULL is returned on failure.` |
|        - | 12013 | ` */` |
|        2 | 12014 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12015 |  |
|        - | 12016 | `	ph7_value *pArray,*pEntry;` |
|        - | 12017 | `	/* NOTE:` |
|        - | 12018 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 12019 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 12020 | `	 */` |
|        3 | 12021 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12022 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12023 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12024 | `		SXUNUSED(apArg);` |
|        - | 12025 | `		/* Return NULL */` |
|      ! 0 | 12026 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12027 | `		return SXRET_OK;` |
|        - | 12028 | `	}` |
|        3 | 12029 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 12030 | `	if( pEntry == 0 ){` |
|        - | 12031 | `		/* Return NULL */` |
|      ! 0 | 12032 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12033 | `		return SXRET_OK;` |
|        - | 12034 | `	}` |
|        - | 12035 | `	/* Fill with the appropriate information */` |
|        3 | 12036 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 12037 | `	/* Create the 'internal' index */` |
|        3 | 12038 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 12039 | `	/* Create the user-func array */` |
|        3 | 12040 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 12041 | `	if( pEntry == 0 ){` |
|        - | 12042 | `		/* Return NULL */` |
|      ! 0 | 12043 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12044 | `		return SXRET_OK;` |
|        - | 12045 | `	}` |
|        - | 12046 | `	/* Fill with the appropriate information */` |
|        3 | 12047 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 12048 | `	/* Create the 'user' index */` |
|        3 | 12049 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 12050 | `	/* Return the multi-dimensional array */` |
|        3 | 12051 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12052 | `	return SXRET_OK;` |
|        2 | 12053 |  |
|        - | 12054 | `/*` |
|        - | 12055 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 12056 | ` *  Register a function for execution on shutdown.` |
|        - | 12057 | ` * Note` |
|        - | 12058 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 12059 | ` *  be called in the same order as they were registered.` |
|        - | 12060 | ` * Parameters` |
|        - | 12061 | ` *  $callback` |
|        - | 12062 | ` *   The shutdown callback to register.` |
|        - | 12063 | ` * $param` |
|        - | 12064 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 12065 | ` * Return` |
|        - | 12066 | ` *  Nothing.` |
|        - | 12067 | ` */` |
|       12 | 12068 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 12069 |  |
|        - | 12070 | `	VmShutdownCB sEntry;` |
|        - | 12071 | `	int i,j;` |
|       17 | 12072 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12073 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 12074 | `		return PH7_OK;` |
|        - | 12075 | `	}` |
|        - | 12076 | `	/* Zero the Entry */` |
|       17 | 12077 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 12078 | `	/* Initialize fields */` |
|       17 | 12079 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 12080 | `	/* Save the callback name for later invocation name */` |
|       17 | 12081 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      137 | 12082 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      125 | 12083 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       65 | 12084 | `	}` |
|        - | 12085 | `	/* Copy arguments */` |
|       17 | 12086 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 12087 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 12088 | `			/* Limit reached */` |
|      ! 0 | 12089 | `			break;` |
|        - | 12090 | `		}` |
|      ! 0 | 12091 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 12092 | `	}` |
|       17 | 12093 | `	sEntry.nArg = j;` |
|        - | 12094 | `	/* Install the callback */` |
|       17 | 12095 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       17 | 12096 | `	return PH7_OK;` |
|       11 | 12097 |  |
|        - | 12098 | `/*` |
|        - | 12099 | ` * Section:` |
|        - | 12100 | ` *  Class handling functions.` |
|        - | 12101 | ` * Status:` |
|        - | 12102 | ` *    Stable.` |
|        - | 12103 | ` */` |
|        - | 12104 | `/*` |
|        - | 12105 | ` * Extract the top active class. NULL is returned` |
|        - | 12106 | ` * if the class stack is empty.` |
|        - | 12107 | ` */` |
|     1054 | 12108 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        5 | 12109 |  |
|     1059 | 12110 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 12111 | `	ph7_class **apClass;` |
|     1059 | 12112 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 12113 | `		/* Empty stack,return NULL */` |
|       15 | 12114 | `		return 0;` |
|        - | 12115 | `	}` |
|        - | 12116 | `	/* Peek the last entry */` |
|     1045 | 12117 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|     1045 | 12118 | `	return apClass[pSet->nUsed - 1];` |
|      532 | 12119 |  |
|        - | 12120 | `/*` |
|        - | 12121 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 12122 | ` *   Get the class that declared the currently executing method.` |
|        - | 12123 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 12124 | ` *` |
|        - | 12125 | ` * Parameters` |
|        - | 12126 | ` *   pVm: Target VM` |
|        - | 12127 | ` *` |
|        - | 12128 | ` * Return` |
|        - | 12129 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 12130 | ` *   - Not executing within a class method` |
|        - | 12131 | ` *` |
|        - | 12132 | ` * Note` |
|        - | 12133 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 12134 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 12135 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 12136 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 12137 | ` *   declaring class.` |
|        - | 12138 | ` */` |
|       98 | 12139 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        5 | 12140 |  |
|      103 | 12141 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12142 | `	ph7_vm_func *pVmFunc;` |
|        - | 12143 |  |
|        - | 12144 | `	/* Skip exception frames to find the actual method frame */` |
|      103 | 12145 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 12146 |  |
|        - | 12147 | `	/* Check if we're in a method context */` |
|      103 | 12148 | `	if( pFrame->pParent ){` |
|       99 | 12149 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       99 | 12150 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 12151 | `			/* Return the declaring class */` |
|       99 | 12152 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 12153 | `		}` |
|      ! 0 | 12154 | `	}` |
|        - | 12155 |  |
|        5 | 12156 | `	return 0;` |
|       54 | 12157 |  |
|        - | 12158 |  |
|        - | 12159 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 12160 | `/*` |
|        - | 12161 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 12162 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 12163 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 12164 | ` * return value indicates failure.` |
|        - | 12165 | ` */` |
|        - | 12166 | `/*` |
|        - | 12167 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 12168 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 12169 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 12170 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 12171 | ` */` |
|     2880 | 12172 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 12173 | `	ph7_vm *pVm,` |
|        - | 12174 | `	ph7_class_instance *pThis,` |
|        - | 12175 | `	ph7_class_method *pMethod,` |
|        - | 12176 | `	ph7_value *pResult,` |
|        - | 12177 | `	int nArg,` |
|        - | 12178 | `	ph7_value **apArg,` |
|        - | 12179 | `	VmCallArgMap *pMap` |
|        - | 12180 | `	)` |
|        5 | 12181 |  |
|        - | 12182 | `	ph7_value *aStack;` |
|        - | 12183 | `	VmInstr aInstr[2];` |
|        - | 12184 | `	int iCursor;` |
|        - | 12185 | `	int i;` |
|        - | 12186 | `	sxi32 rc;` |
|     2885 | 12187 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2885 | 12188 | `	if( aStack == 0 ){` |
|      ! 0 | 12189 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12190 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 12191 | `		return SXERR_MEM;` |
|        - | 12192 | `	}` |
|     4493 | 12193 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1613 | 12194 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1613 | 12195 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      809 | 12196 | `	}` |
|     2885 | 12197 | `	iCursor = nArg + 1;` |
|     2885 | 12198 | `	if( pThis ){` |
|     2879 | 12199 | `		pThis->iRef++;` |
|     2879 | 12200 | `		aStack[i].x.pOther = pThis;` |
|     2879 | 12201 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1437 | 12202 | `	}` |
|     2885 | 12203 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2885 | 12204 | `	i++;` |
|     2885 | 12205 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2885 | 12206 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2885 | 12207 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2885 | 12208 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2885 | 12209 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2885 | 12210 | `	aInstr[0].iP1 = nArg;` |
|     2885 | 12211 | `	aInstr[0].iP2 = 0;` |
|     2885 | 12212 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2885 | 12213 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2885 | 12214 | `	aInstr[1].iP1 = 1;` |
|     2885 | 12215 | `	aInstr[1].iP2 = 0;` |
|     2885 | 12216 | `	aInstr[1].p3  = 0;` |
|     2885 | 12217 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0,FALSE);` |
|     2885 | 12218 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12219 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 12220 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2885 | 12221 | `	return rc;` |
|     1445 | 12222 |  |
|     2268 | 12223 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 12224 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 12225 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 12226 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 12227 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 12228 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 12229 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 12230 | `	)` |
|        5 | 12231 |  |
|     2273 | 12232 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        5 | 12233 |  |
|        - | 12234 | `/*` |
|        - | 12235 | ` * Helper for PH7_VmIteratorWalk: call a zero-arg Iterator method by name,` |
|        - | 12236 | ` * returning its result. Returns the exec status so a method that throws` |
|        - | 12237 | ` * (PH7_EXCEPTION) or aborts (PH7_ABORT) is propagated — unlike the foreach` |
|        - | 12238 | ` * opcode, which discards it.` |
|        - | 12239 | ` */` |
|      324 | 12240 | `static sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult)` |
|        1 | 12241 |  |
|      325 | 12242 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,zName,nLen);` |
|      325 | 12243 | `	if( pMethod == 0 ){` |
|      ! 0 | 12244 | `		return SXRET_OK; /* missing method: treat as no-op (mirrors foreach leniency) */` |
|        - | 12245 | `	}` |
|      325 | 12246 | `	return PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,0,0);` |
|      163 | 12247 |  |
|        - | 12248 | `/*` |
|        - | 12249 | ` * Walk a Traversable (Iterator / IteratorAggregate / Generator), invoking xStep` |
|        - | 12250 | ` * for each (key,value) pair. This is the reusable form of the Iterator protocol` |
|        - | 12251 | ` * that the foreach opcode drives inline; it is consumed by iterator_to_array /` |
|        - | 12252 | ` * iterator_count / iterator_apply and by Traversable spread.` |
|        - | 12253 | ` *` |
|        - | 12254 | ` * Returns:` |
|        - | 12255 | ` *   SXRET_OK            walk completed (or xStep stopped early via SXERR_EOF)` |
|        - | 12256 | ` *   SXERR_NOTIMPLEMENTED pObj is not a Traversable (caller raises a TypeError)` |
|        - | 12257 | ` *   PH7_EXCEPTION       an iterator method or the step threw` |
|        - | 12258 | ` *   PH7_ABORT           an iterator method or the step requested a VM halt` |
|        - | 12259 | ` *` |
|        - | 12260 | ` * pKey/pValue handed to xStep are owned by the walk (released after the step` |
|        - | 12261 | ` * returns); xStep must copy what it needs.` |
|        - | 12262 | ` */` |
|       28 | 12263 | `PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData)` |
|        1 | 12264 |  |
|        - | 12265 | `	ph7_class_instance *pThis;        /* the live Iterator (after aggregate resolution) */` |
|       29 | 12266 | `	ph7_class_instance *pAggregate = 0;` |
|        - | 12267 | `	ph7_class *pIteratorClass;` |
|       29 | 12268 | `	sxi32 rc = SXRET_OK;` |
|       29 | 12269 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->x.pOther == 0 ){` |
|      ! 0 | 12270 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12271 | `	}` |
|       29 | 12272 | `	pThis = (ph7_class_instance *)pObj->x.pOther;` |
|       29 | 12273 | `	pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       29 | 12274 | `	if( pIteratorClass == 0 ){` |
|      ! 0 | 12275 | `		return SXERR_NOTIMPLEMENTED;` |
|        - | 12276 | `	}` |
|       29 | 12277 | `	if( PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|       27 | 12278 | `		pThis->iRef++; /* keep the iterator alive across the walk */` |
|       14 | 12279 | `	}else{` |
|        - | 12280 | `		/* Maybe an IteratorAggregate: resolve its inner Iterator via getIterator() */` |
|        3 | 12281 | `		ph7_class *pAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|        - | 12282 | `		ph7_value sInner;` |
|        3 | 12283 | `		int bOk = 0;` |
|        3 | 12284 | `		if( pAggClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pAggClass) ){` |
|      ! 0 | 12285 | `			return SXERR_NOTIMPLEMENTED; /* not Traversable at all */` |
|        - | 12286 | `		}` |
|        3 | 12287 | `		PH7_MemObjInit(&(*pVm),&sInner);` |
|        3 | 12288 | `		rc = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sInner);` |
|        3 | 12289 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      ! 0 | 12290 | `			PH7_MemObjRelease(&sInner);` |
|      ! 0 | 12291 | `			return rc;` |
|        - | 12292 | `		}` |
|        3 | 12293 | `		if( (sInner.iFlags & MEMOBJ_OBJ) && sInner.x.pOther ){` |
|        3 | 12294 | `			ph7_class_instance *pIter = (ph7_class_instance *)sInner.x.pOther;` |
|        3 | 12295 | `			if( PH7_VmInstanceOf(pIter->pClass,pIteratorClass) ){` |
|        3 | 12296 | `				pAggregate = pThis; pAggregate->iRef++; /* keep the aggregate alive */` |
|        3 | 12297 | `				pThis = pIter; pThis->iRef++;           /* survive release of sInner */` |
|        3 | 12298 | `				bOk = 1;` |
|        1 | 12299 | `			}` |
|        1 | 12300 | `		}` |
|        3 | 12301 | `		PH7_MemObjRelease(&sInner);` |
|        3 | 12302 | `		if( !bOk ){` |
|        - | 12303 | `			/* getIterator() returned a non-Iterator: surface as not-a-Traversable */` |
|      ! 0 | 12304 | `			return SXERR_NOTIMPLEMENTED;` |
|        - | 12305 | `		}` |
|        - | 12306 | `	}` |
|        - | 12307 | `	/* Drive rewind / valid / current / key / step / next */` |
|       29 | 12308 | `	rc = VmIterCallMethod(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|       29 | 12309 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       78 | 12310 | `	for(;;){` |
|        - | 12311 | `		ph7_value sValid,sValue,sKey;` |
|        - | 12312 | `		int isValid;` |
|       93 | 12313 | `		PH7_MemObjInit(&(*pVm),&sValid);` |
|       93 | 12314 | `		rc = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);` |
|       96 | 12315 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValid); goto done; }` |
|       93 | 12316 | `		PH7_MemObjToBool(&sValid);` |
|       93 | 12317 | `		isValid = (sValid.x.iVal != 0);` |
|       93 | 12318 | `		PH7_MemObjRelease(&sValid);` |
|       93 | 12319 | `		if( !isValid ){ rc = SXRET_OK; break; }` |
|       71 | 12320 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|       71 | 12321 | `		rc = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sValue);` |
|       71 | 12322 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); goto done; }` |
|       69 | 12323 | `		PH7_MemObjInit(&(*pVm),&sKey);` |
|       69 | 12324 | `		rc = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);` |
|       69 | 12325 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sValue); PH7_MemObjRelease(&sKey); goto done; }` |
|       69 | 12326 | `		rc = xStep(&(*pVm),&sKey,&sValue,pUserData);` |
|       69 | 12327 | `		PH7_MemObjRelease(&sValue);` |
|       69 | 12328 | `		PH7_MemObjRelease(&sKey);` |
|       69 | 12329 | `		if( rc != SXRET_OK ){` |
|        5 | 12330 | `			if( rc == SXERR_EOF ){ rc = SXRET_OK; } /* early stop is success */` |
|        5 | 12331 | `			goto done;` |
|        - | 12332 | `		}` |
|       65 | 12333 | `		rc = VmIterCallMethod(pVm,pThis,"next",sizeof("next")-1,0);` |
|       65 | 12334 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ goto done; }` |
|       12 | 12335 | `	}` |
|       14 | 12336 | `done:` |
|       29 | 12337 | `	PH7_ClassInstanceUnref(pThis);` |
|       29 | 12338 | `	if( pAggregate ){ PH7_ClassInstanceUnref(pAggregate); }` |
|       29 | 12339 | `	return rc;` |
|       15 | 12340 |  |
|        - | 12341 | `/*` |
|        - | 12342 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 12343 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 12344 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 12345 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 12346 | ` *` |
|        - | 12347 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 12348 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 12349 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 12350 | ` *` |
|        - | 12351 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 12352 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 12353 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 12354 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 12355 | ` *` |
|        - | 12356 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 12357 | ` */` |
|      174 | 12358 | `static sxi32 VmCallObjectInvoke(` |
|        - | 12359 | `	ph7_vm *pVm,` |
|        - | 12360 | `	ph7_class_instance *pThis,` |
|        - | 12361 | `	int nArg,` |
|        - | 12362 | `	ph7_value **apArg,` |
|        - | 12363 | `	ph7_value *pResult,` |
|        - | 12364 | `	VmCallArgMap *pMap` |
|        - | 12365 | `	)` |
|        4 | 12366 |  |
|        - | 12367 | `	ph7_class_method *pMethod;` |
|      178 | 12368 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      178 | 12369 | `	if( pMethod == 0 ){` |
|       13 | 12370 | `		if( pResult ){` |
|       13 | 12371 | `			PH7_MemObjRelease(pResult);` |
|        6 | 12372 | `		}` |
|       13 | 12373 | `		return SXERR_INVALID;` |
|        - | 12374 | `	}` |
|      166 | 12375 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       91 | 12376 |  |
|        - | 12377 | `/*` |
|        - | 12378 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 12379 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 12380 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 12381 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 12382 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 12383 | ` * lookup or 'goto Exception').` |
|        - | 12384 | ` *` |
|        - | 12385 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 12386 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 12387 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 12388 | ` * reported.` |
|        - | 12389 | ` */` |
|       12 | 12390 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 12391 |  |
|        - | 12392 | `	ph7_class *pErrorClass;` |
|       13 | 12393 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 12394 | `	ph7_class_method *pCons;` |
|        - | 12395 | `	VmFrame *pThrowFrame;` |
|        - | 12396 | `	char zMsg[256];` |
|        - | 12397 | `	int nMsg;` |
|        - | 12398 | `	sxi32 rc;` |
|       25 | 12399 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 12400 | `		"Object of type %.*s is not callable",` |
|       12 | 12401 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 12402 | `		pThis->pClass->sName.zString);` |
|       13 | 12403 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 12404 | `	if( pErrorClass ){` |
|       13 | 12405 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 12406 | `	}` |
|       13 | 12407 | `	if( pErrInst == 0 ){` |
|        - | 12408 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 12409 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 12410 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 12411 | `		 * visible to the user. */` |
|      ! 0 | 12412 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 12413 | `		return SXERR_ABORT;` |
|        - | 12414 | `	}` |
|       13 | 12415 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 12416 | `	if( pCons ){` |
|        - | 12417 | `		ph7_value sArg;` |
|        - | 12418 | `		ph7_value *apMsg[1];` |
|        - | 12419 | `		SyString sMsgStr;` |
|       13 | 12420 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 12421 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 12422 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 12423 | `		apMsg[0] = &sArg;` |
|       13 | 12424 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 12425 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 12426 | `	}` |
|        - | 12427 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 12428 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 12429 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 12430 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 12431 | `	if( pThrowFrame ){` |
|       13 | 12432 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 12433 | `	}` |
|       13 | 12434 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 12435 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 12436 | `	return rc;` |
|        7 | 12437 |  |
|        - | 12438 | `/*` |
|        - | 12439 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 12440 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 12441 | ` * in the apArg[] array.` |
|        - | 12442 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12443 | ` * return value indicates failure.` |
|        - | 12444 | ` */` |
|     1238 | 12445 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 12446 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12447 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12448 | `	int nArg,          /* Total number of given arguments */` |
|        - | 12449 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 12450 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 12451 | `	)` |
|        5 | 12452 |  |
|        - | 12453 | `	ph7_value *aStack;` |
|        - | 12454 | `	VmInstr aInstr[2];` |
|        - | 12455 | `	int i;` |
|     1243 | 12456 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12457 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12458 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12459 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      141 | 12460 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12461 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12462 | `			nArg,apArg,pResult,0);` |
|        - | 12463 | `	}` |
|     1151 | 12464 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12465 | `		/* Don't bother processing,it's invalid anyway */` |
|      514 | 12466 | `		if( pResult ){` |
|        - | 12467 | `			/* Assume a null return value */` |
|      ! 0 | 12468 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12469 | `		}` |
|      514 | 12470 | `		return SXERR_INVALID;` |
|        - | 12471 | `	}` |
|      641 | 12472 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12473 | `		/* Class method */` |
|       15 | 12474 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12475 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12476 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12477 | `		ph7_class *pClass = 0;` |
|        - | 12478 | `		ph7_value *pValue;` |
|        - | 12479 | `		sxi32 rc;` |
|       15 | 12480 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12481 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12482 | `			if( pResult ){` |
|        - | 12483 | `				/* Assume a null return value */` |
|      ! 0 | 12484 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12485 | `			}` |
|      ! 0 | 12486 | `			return SXRET_OK;` |
|        - | 12487 | `		}` |
|        - | 12488 | `		/* Extract the class name or an instance of it */` |
|       15 | 12489 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12490 | `		if( pValue ){` |
|       15 | 12491 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12492 | `		}` |
|       15 | 12493 | `		if( pClass == 0 ){` |
|        - | 12494 | `			/* No such class,return NULL */` |
|      ! 0 | 12495 | `			if( pResult ){` |
|      ! 0 | 12496 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12497 | `			}` |
|      ! 0 | 12498 | `			return SXRET_OK;` |
|        - | 12499 | `		}` |
|       15 | 12500 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12501 | `			/* Point to the class instance */` |
|        9 | 12502 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12503 | `		}` |
|        - | 12504 | `		/* Try to extract the method */` |
|       15 | 12505 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12506 | `		if( pValue ){` |
|       15 | 12507 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12508 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12509 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12510 | `			}` |
|        7 | 12511 | `		}` |
|       15 | 12512 | `		if( pMethod == 0 ){` |
|        - | 12513 | `			/* No such method,return NULL */` |
|      ! 0 | 12514 | `			if( pResult ){` |
|      ! 0 | 12515 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12516 | `			}` |
|      ! 0 | 12517 | `			return SXRET_OK;` |
|        - | 12518 | `		}` |
|        - | 12519 | `		/* Call the class method */` |
|       15 | 12520 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12521 | `		return rc;` |
|        - | 12522 | `	}` |
|        - | 12523 | `	/* Create a new operand stack */` |
|      627 | 12524 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      627 | 12525 | `	if( aStack == 0 ){` |
|      ! 0 | 12526 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12527 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12528 | `		if( pResult ){` |
|        - | 12529 | `			/* Assume a null return value */` |
|      ! 0 | 12530 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12531 | `		}` |
|      ! 0 | 12532 | `		return SXERR_MEM;` |
|        - | 12533 | `	}` |
|        - | 12534 | `	/* Fill the operand stack with the given arguments */` |
|     1937 | 12535 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1315 | 12536 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12537 | `		/*` |
|        - | 12538 | `		 * Symisc eXtension:` |
|        - | 12539 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12540 | `		 */` |
|     1315 | 12541 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      660 | 12542 | `	}` |
|        - | 12543 | `	/* Push the function name */` |
|      627 | 12544 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      627 | 12545 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12546 | `	/* Emit the CALL istruction */` |
|      627 | 12547 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      627 | 12548 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      627 | 12549 | `	aInstr[0].iP2 = 0;` |
|      627 | 12550 | `	aInstr[0].p3  = 0;` |
|        - | 12551 | `	/* Emit the DONE instruction */` |
|      627 | 12552 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      627 | 12553 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      627 | 12554 | `	aInstr[1].iP2 = 0;` |
|      627 | 12555 | `	aInstr[1].p3  = 0;` |
|        - | 12556 | `	/* Execute the function body (if available) */` |
|        - | 12557 | `	{` |
|        - | 12558 | `		sxi32 rcExec;` |
|      627 | 12559 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0,FALSE);` |
|        - | 12560 | `		/* Clean up the mess left behind */` |
|      627 | 12561 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12562 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      627 | 12563 | `		return rcExec;` |
|        - | 12564 | `	}` |
|      624 | 12565 |  |
|        - | 12566 | `/*` |
|        - | 12567 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12568 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12569 | ` * parameter.` |
|        - | 12570 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12571 | ` * return value indicates failure.` |
|        - | 12572 | ` */` |
|      240 | 12573 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12574 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12575 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12576 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12577 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12578 | `	)` |
|        1 | 12579 |  |
|        - | 12580 | `	ph7_value *pArg;` |
|        - | 12581 | `	SySet aArg;` |
|        - | 12582 | `	va_list ap;` |
|        - | 12583 | `	sxi32 rc;` |
|      241 | 12584 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12585 | `	/* Copy arguments one after one */` |
|      241 | 12586 | `	va_start(ap,pResult);` |
|      399 | 12587 | `	for(;;){` |
|      799 | 12588 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12589 | `		if( pArg == 0 ){` |
|      241 | 12590 | `			break;` |
|        - | 12591 | `		}` |
|      559 | 12592 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12593 | `	}` |
|        - | 12594 | `	/* Call the core routine */` |
|      241 | 12595 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12596 | `	/* Cleanup */` |
|      241 | 12597 | `	SySetRelease(&aArg);` |
|      241 | 12598 | `	return rc;` |
|        1 | 12599 |  |
|        - | 12600 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12601 | `/*` |
|        - | 12602 | ` * bool defined(string $name)` |
|        - | 12603 | ` *  Checks whether a given named constant exists.` |
|        - | 12604 | ` * Parameter:` |
|        - | 12605 | ` *  Name of the desired constant.` |
|        - | 12606 | ` * Return` |
|        - | 12607 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12608 | ` */` |
|       26 | 12609 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12610 |  |
|        - | 12611 | `	const char *zName;` |
|       28 | 12612 | `	int nLen = 0;` |
|       28 | 12613 | `	int res = 0;` |
|       28 | 12614 | `	if( nArg < 1 ){` |
|        - | 12615 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12616 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12617 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12618 | `		return SXRET_OK;` |
|        - | 12619 | `	}` |
|        - | 12620 | `	/* Extract constant name */` |
|       28 | 12621 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12622 | `	/* Perform the lookup */` |
|       28 | 12623 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12624 | `		/* Already defined */` |
|       26 | 12625 | `		res = 1;` |
|       12 | 12626 | `	}` |
|       28 | 12627 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12628 | `	return SXRET_OK;` |
|       15 | 12629 |  |
|        - | 12630 | `/*` |
|        - | 12631 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12632 | ` * below.` |
|        - | 12633 | ` */` |
|       16 | 12634 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 | 12635 |  |
|       19 | 12636 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12637 | `	/* Expand constant value */` |
|       19 | 12638 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       19 | 12639 |  |
|        - | 12640 | `/*` |
|        - | 12641 | ` * bool define(string $constant_name,expression value)` |
|        - | 12642 | ` *  Defines a named constant at runtime.` |
|        - | 12643 | ` * Parameter:` |
|        - | 12644 | ` *  $constant_name` |
|        - | 12645 | ` *   The name of the constant` |
|        - | 12646 | ` *  $value` |
|        - | 12647 | ` *   Constant value` |
|        - | 12648 | ` * Return:` |
|        - | 12649 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12650 | ` */` |
|       14 | 12651 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 12652 |  |
|        - | 12653 | `	const char *zName;  /* Constant name */` |
|        - | 12654 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       17 | 12655 | `	int nLen = 0;       /* Name length */` |
|        - | 12656 | `	sxi32 rc;` |
|       17 | 12657 | `	if( nArg < 2 ){` |
|        - | 12658 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12659 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12660 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12661 | `		return SXRET_OK;` |
|        - | 12662 | `	}` |
|       17 | 12663 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12664 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12665 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12666 | `		return SXRET_OK;` |
|        - | 12667 | `	}` |
|        - | 12668 | `	/* Extract constant name */` |
|       17 | 12669 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       17 | 12670 | `	if( nLen < 1 ){` |
|      ! 0 | 12671 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12672 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12673 | `		return SXRET_OK;` |
|        - | 12674 | `	}` |
|        - | 12675 | `	/* Duplicate constant value */` |
|       17 | 12676 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       17 | 12677 | `	if( pValue == 0 ){` |
|      ! 0 | 12678 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12679 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12680 | `		return SXRET_OK;` |
|        - | 12681 | `	}` |
|        - | 12682 | `	/* Initialize the memory object */` |
|       17 | 12683 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12684 | `	/* Register the constant */` |
|       17 | 12685 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       17 | 12686 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12687 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12688 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12689 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12690 | `		return SXRET_OK;` |
|        - | 12691 | `	}` |
|        - | 12692 | `	/* Duplicate constant value */` |
|       17 | 12693 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       17 | 12694 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12695 | `		/* Lower case the constant name */` |
|      ! 0 | 12696 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12697 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12698 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12699 | `				/* UTF-8 stream */` |
|      ! 0 | 12700 | `				zCur++;` |
|      ! 0 | 12701 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12702 | `					zCur++;` |
|      ! 0 | 12703 | `				}` |
|      ! 0 | 12704 | `				continue;` |
|        - | 12705 | `			}` |
|      ! 0 | 12706 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12707 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12708 | `				zCur[0] = (char)c;` |
|      ! 0 | 12709 | `			}` |
|      ! 0 | 12710 | `			zCur++;` |
|      ! 0 | 12711 | `		}` |
|        - | 12712 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12713 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12714 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12715 | `		{` |
|      ! 0 | 12716 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12717 | `			if( pAlias ){` |
|      ! 0 | 12718 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12719 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12720 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12721 | `			}` |
|        - | 12722 | `		}` |
|      ! 0 | 12723 | `	}` |
|        - | 12724 | `	/* All done,return TRUE */` |
|       17 | 12725 | `	ph7_result_bool(pCtx,1);` |
|       17 | 12726 | `	return SXRET_OK;` |
|       10 | 12727 |  |
|        - | 12728 | `/*` |
|        - | 12729 | ` * value constant(string $name)` |
|        - | 12730 | ` *  Returns the value of a constant` |
|        - | 12731 | ` * Parameter` |
|        - | 12732 | ` *  $name` |
|        - | 12733 | ` *    Name of the constant.` |
|        - | 12734 | ` * Return` |
|        - | 12735 | ` *  Constant value or NULL if not defined.` |
|        - | 12736 | ` */` |
|        8 | 12737 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 12738 |  |
|        - | 12739 | `	SyHashEntry *pEntry;` |
|        - | 12740 | `	ph7_constant *pCons;` |
|        - | 12741 | `	const char *zName; /* Constant name */` |
|        - | 12742 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12743 | `	int nLen;` |
|       11 | 12744 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12745 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12746 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12747 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12748 | `		return SXRET_OK;` |
|        - | 12749 | `	}` |
|        - | 12750 | `	/* Extract the constant name */` |
|       11 | 12751 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12752 | `	/* Perform the query */` |
|       11 | 12753 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       11 | 12754 | `	if( pEntry == 0 ){` |
|        3 | 12755 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12756 | `		ph7_result_null(pCtx);` |
|        3 | 12757 | `		return SXRET_OK;` |
|        - | 12758 | `	}` |
|        9 | 12759 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12760 | `	/* Point to the structure that describe the constant */` |
|        9 | 12761 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12762 | `	/* Extract constant value by calling it's associated callback */` |
|        9 | 12763 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12764 | `	/* Return that value */` |
|        9 | 12765 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12766 | `	/* Cleanup */` |
|        9 | 12767 | `	PH7_MemObjRelease(&sVal);` |
|        9 | 12768 | `	return SXRET_OK;` |
|        7 | 12769 |  |
|        - | 12770 | `/*` |
|        - | 12771 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12772 | ` * defined below.` |
|        - | 12773 | ` */` |
|      466 | 12774 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12775 |  |
|      467 | 12776 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12777 | `	ph7_value sName;` |
|        - | 12778 | `	sxi32 rc;` |
|        - | 12779 | `	/* Prepare the constant name for insertion */` |
|      467 | 12780 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12781 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12782 | `	/* Perform the insertion */` |
|      467 | 12783 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12784 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12785 | `	return rc;` |
|        1 | 12786 |  |
|        - | 12787 | `/*` |
|        - | 12788 | ` * array get_defined_constants(void)` |
|        - | 12789 | ` *  Returns an associative array with the names of all defined` |
|        - | 12790 | ` *  constants.` |
|        - | 12791 | ` * Parameters` |
|        - | 12792 | ` *  NONE.` |
|        - | 12793 | ` * Returns` |
|        - | 12794 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12795 | ` */` |
|        2 | 12796 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12797 |  |
|        - | 12798 | `	ph7_value *pArray;` |
|        - | 12799 | `	/* Create the array first*/` |
|        3 | 12800 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12801 | `	if( pArray == 0 ){` |
|      ! 0 | 12802 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12803 | `		SXUNUSED(apArg);` |
|        - | 12804 | `		/* Return NULL */` |
|      ! 0 | 12805 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12806 | `		return SXRET_OK;` |
|        - | 12807 | `	}` |
|        - | 12808 | `	/* Fill the array with the defined constants */` |
|        3 | 12809 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12810 | `	/* Return the created array */` |
|        3 | 12811 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12812 | `	return SXRET_OK;` |
|        2 | 12813 |  |
|        - | 12814 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12815 | `/*` |
|        - | 12816 | ` * Section:` |
|        - | 12817 | ` *  Random numbers/string generators.` |
|        - | 12818 | ` * Status:` |
|        - | 12819 | ` *    Stable.` |
|        - | 12820 | ` */` |
|        - | 12821 | `/*` |
|        - | 12822 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12823 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12824 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12825 | ` */` |
|     2912 | 12826 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 | 12827 |  |
|        - | 12828 | `	sxu32 iNum;` |
|     2917 | 12829 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2917 | 12830 | `	return iNum;` |
|        5 | 12831 |  |
|        - | 12832 | `/*` |
|        - | 12833 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12834 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12835 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12836 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12837 | ` */` |
|   237114 | 12838 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 | 12839 |  |
|        - | 12840 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12841 | `	int i;` |
|        - | 12842 | `	/* Generate a binary string first */` |
|   237119 | 12843 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12844 | `	/* Turn the binary string into english based alphabet */` |
|  2608427 | 12845 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2371313 | 12846 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1185659 | 12847 | `	 }` |
|   237119 | 12848 |  |
|        - | 12849 | `/*` |
|        - | 12850 | ` * int rand()` |
|        - | 12851 | ` * int mt_rand()` |
|        - | 12852 | ` * int rand(int $min,int $max)` |
|        - | 12853 | ` * int mt_rand(int $min,int $max)` |
|        - | 12854 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12855 | ` * Parameter` |
|        - | 12856 | ` *  $min` |
|        - | 12857 | ` *    The lowest value to return (default: 0)` |
|        - | 12858 | ` *  $max` |
|        - | 12859 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12860 | ` * Return` |
|        - | 12861 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12862 | ` * Note:` |
|        - | 12863 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12864 | ` *  by te SQLite3 library.` |
|        - | 12865 | ` */` |
|       20 | 12866 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12867 |  |
|        - | 12868 | `	sxu32 iNum;` |
|        - | 12869 | `	/* Generate the random number */` |
|       21 | 12870 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12871 | `	if( nArg > 1 ){` |
|        - | 12872 | `		sxu32 iMin,iMax;` |
|        3 | 12873 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12874 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12875 | `		if( iMin < iMax ){` |
|        3 | 12876 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12877 | `			if( iDiv > 0 ){` |
|        3 | 12878 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12879 | `			}` |
|        1 | 12880 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12881 | `			iNum %= iMax;` |
|      ! 0 | 12882 | `		}` |
|        1 | 12883 | `	}` |
|        - | 12884 | `	/* Return the number */` |
|       21 | 12885 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12886 | `	return SXRET_OK;` |
|        1 | 12887 |  |
|        - | 12888 | `/*` |
|        - | 12889 | ` * int getrandmax(void)` |
|        - | 12890 | ` * int mt_getrandmax(void)` |
|        - | 12891 | ` * int rc4_getrandmax(void)` |
|        - | 12892 | ` *   Show largest possible random value` |
|        - | 12893 | ` * Return` |
|        - | 12894 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12895 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12896 | ` * Note:` |
|        - | 12897 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12898 | ` *  by te SQLite3 library.` |
|        - | 12899 | ` */` |
|        4 | 12900 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12901 |  |
|        2 | 12902 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12903 | `	SXUNUSED(apArg);` |
|        5 | 12904 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12905 | `	return SXRET_OK;` |
|        1 | 12906 |  |
|        - | 12907 | `/*` |
|        - | 12908 | ` * string rand_str()` |
|        - | 12909 | ` * string rand_str(int $len)` |
|        - | 12910 | ` *  Generate a random string (English alphabet).` |
|        - | 12911 | ` * Parameter` |
|        - | 12912 | ` *  $len` |
|        - | 12913 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12914 | ` * Return` |
|        - | 12915 | ` *   A pseudo random string.` |
|        - | 12916 | ` * Note:` |
|        - | 12917 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12918 | ` *  by te SQLite3 library.` |
|        - | 12919 | ` *  This function is a symisc extension.` |
|        - | 12920 | ` */` |
|      120 | 12921 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12922 |  |
|        - | 12923 | `	char zString[1024];` |
|      122 | 12924 | `	int iLen = 0x10;` |
|      122 | 12925 | `	if( nArg > 0 ){` |
|        - | 12926 | `		/* Get the desired length */` |
|      122 | 12927 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12928 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12929 | `			/* Default length */` |
|        3 | 12930 | `			iLen = 0x10;` |
|        1 | 12931 | `		}` |
|       60 | 12932 | `	}` |
|        - | 12933 | `	/* Generate the random string */` |
|      122 | 12934 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12935 | `	/* Return the generated string */` |
|      122 | 12936 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12937 | `	return SXRET_OK;` |
|        2 | 12938 |  |
|        - | 12939 | `/*` |
|        - | 12940 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12941 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12942 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12943 | ` */` |
|      488 | 12944 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12945 |  |
|      488 | 12946 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12947 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12948 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12949 | `			"TypeError",` |
|        - | 12950 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12951 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12952 | `			ph7_type_name(pArg)` |
|        - | 12953 | `			);` |
|        - | 12954 | `	}` |
|      483 | 12955 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12956 | `		int len;` |
|        9 | 12957 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12958 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12959 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12960 | `				"TypeError",` |
|        - | 12961 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12962 | `				zFunc,iArgPos,zParamName` |
|        - | 12963 | `				);` |
|        - | 12964 | `		}` |
|        2 | 12965 | `	}` |
|      479 | 12966 | `	return SXRET_OK;` |
|      245 | 12967 |  |
|        - | 12968 | `/*` |
|        - | 12969 | ` * int random_int(int $min, int $max)` |
|        - | 12970 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12971 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12972 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12973 | ` *  power-of-two mask covering the range.` |
|        - | 12974 | ` */` |
|      242 | 12975 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12976 |  |
|        - | 12977 | `	sxi64 iMin,iMax;` |
|        - | 12978 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12979 | `	unsigned int nAttempt;` |
|        - | 12980 | `	int rc;` |
|      243 | 12981 | `	if( nArg != 2 ){` |
|       10 | 12982 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12983 | `			"ArgumentCountError",` |
|        - | 12984 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12985 | `			nArg` |
|        - | 12986 | `			);` |
|        - | 12987 | `	}` |
|      237 | 12988 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12989 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12990 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12991 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12992 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12993 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12994 | `	if( iMin > iMax ){` |
|        3 | 12995 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12996 | `			"ValueError",` |
|        - | 12997 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12998 | `			);` |
|        - | 12999 | `	}` |
|      229 | 13000 | `	if( iMin == iMax ){` |
|        5 | 13001 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 13002 | `		return SXRET_OK;` |
|        - | 13003 | `	}` |
|      225 | 13004 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 13005 | `	uMask = uRange;` |
|      225 | 13006 | `	uMask \|= uMask >> 1;` |
|      225 | 13007 | `	uMask \|= uMask >> 2;` |
|      225 | 13008 | `	uMask \|= uMask >> 4;` |
|      225 | 13009 | `	uMask \|= uMask >> 8;` |
|      225 | 13010 | `	uMask \|= uMask >> 16;` |
|      225 | 13011 | `	uMask \|= uMask >> 32;` |
|      225 | 13012 | `	uResult = 0;` |
|      345 | 13013 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 13014 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 13015 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 13016 | `		 * and the low-half mask would always read 0). */` |
|        - | 13017 | `		sxu64 uDraw;` |
|      345 | 13018 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 13019 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 13020 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 13021 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13022 | `				"Exception",` |
|        - | 13023 | `				"Cannot gather sufficient random data"` |
|        - | 13024 | `				);` |
|        - | 13025 | `		}` |
|      345 | 13026 | `		uDraw &= uMask;` |
|      345 | 13027 | `		if( uDraw <= uRange ){` |
|      225 | 13028 | `			uResult = uDraw;` |
|      225 | 13029 | `			break;` |
|        - | 13030 | `		}` |
|       69 | 13031 | `	}` |
|      225 | 13032 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 13033 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13034 | `			"Exception",` |
|        - | 13035 | `			"Cannot gather sufficient random data"` |
|        - | 13036 | `			);` |
|        - | 13037 | `	}` |
|      225 | 13038 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 13039 | `	return SXRET_OK;` |
|      122 | 13040 |  |
|        - | 13041 | `/*` |
|        - | 13042 | ` * string random_bytes(int $length)` |
|        - | 13043 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 13044 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 13045 | ` */` |
|       24 | 13046 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13047 |  |
|        - | 13048 | `	sxi64 iLen;` |
|        - | 13049 | `	unsigned char zStack[256];` |
|        - | 13050 | `	void *pBuf;` |
|        - | 13051 | `	int rc;` |
|       25 | 13052 | `	int bHeap = 0;` |
|       25 | 13053 | `	if( nArg != 1 ){` |
|        7 | 13054 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13055 | `			"ArgumentCountError",` |
|        - | 13056 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 13057 | `			nArg` |
|        - | 13058 | `			);` |
|        - | 13059 | `	}` |
|       21 | 13060 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 13061 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 13062 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 13063 | `	if( iLen < 1 ){` |
|        5 | 13064 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13065 | `			"ValueError",` |
|        - | 13066 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 13067 | `			);` |
|        - | 13068 | `	}` |
|        - | 13069 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 13070 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 13071 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 13072 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 13073 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13074 | `			"ValueError",` |
|        - | 13075 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 13076 | `			);` |
|        - | 13077 | `	}` |
|       13 | 13078 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 13079 | `		pBuf = zStack;` |
|        7 | 13080 | `	}else{` |
|      ! 0 | 13081 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 13082 | `		if( pBuf == 0 ){` |
|      ! 0 | 13083 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13084 | `				"Exception",` |
|        - | 13085 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 13086 | `				iLen` |
|        - | 13087 | `				);` |
|        - | 13088 | `		}` |
|      ! 0 | 13089 | `		bHeap = 1;` |
|        - | 13090 | `	}` |
|       13 | 13091 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 13092 | `		if( bHeap ){` |
|      ! 0 | 13093 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13094 | `		}` |
|      ! 0 | 13095 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13096 | `			"Exception",` |
|        - | 13097 | `			"Cannot gather sufficient random data"` |
|        - | 13098 | `			);` |
|        - | 13099 | `	}` |
|       13 | 13100 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 13101 | `	if( bHeap ){` |
|      ! 0 | 13102 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 13103 | `	}` |
|       13 | 13104 | `	return SXRET_OK;` |
|       13 | 13105 |  |
|        - | 13106 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13107 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13108 | `/* Unique ID private data */` |
|        - | 13109 | `struct unique_id_data` |
|        - | 13110 |  |
|        - | 13111 | `	ph7_context *pCtx; /* Call context */` |
|        - | 13112 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 13113 | `};` |
|        - | 13114 | `/*` |
|        - | 13115 | ` * Binary to hex consumer callback.` |
|        - | 13116 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 13117 | ` * defined below.` |
|        - | 13118 | ` */` |
|      192 | 13119 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 13120 |  |
|      193 | 13121 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 13122 | `	sxu32 nBuflen;` |
|        - | 13123 | `	/* Extract result buffer length */` |
|      193 | 13124 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 13125 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 13126 | `			/*` |
|        - | 13127 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 13128 | `			 * string will be 13 characters long` |
|        - | 13129 | `			 */` |
|       25 | 13130 | `		return SXERR_ABORT;` |
|        - | 13131 | `	}` |
|      169 | 13132 | `	if( nBuflen > 22 ){` |
|      ! 0 | 13133 | `		return SXERR_ABORT;` |
|        - | 13134 | `	}` |
|        - | 13135 | `	/* Safely Consume the hex stream */` |
|      169 | 13136 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 13137 | `	return SXRET_OK;` |
|       97 | 13138 |  |
|        - | 13139 | `/*` |
|        - | 13140 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 13141 | ` *  Generate a unique ID` |
|        - | 13142 | ` * Parameter` |
|        - | 13143 | ` * $prefix` |
|        - | 13144 | ` *  Append this prefix to the generated unique ID.` |
|        - | 13145 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 13146 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 13147 | ` * $more_entropy` |
|        - | 13148 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 13149 | ` *  that the result will be unique.` |
|        - | 13150 | ` * Return` |
|        - | 13151 | ` *  Returns the unique identifier, as a string.` |
|        - | 13152 | ` */` |
|       24 | 13153 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13154 |  |
|        - | 13155 | `	struct unique_id_data sUniq;` |
|        - | 13156 | `	unsigned char zDigest[20];` |
|       25 | 13157 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13158 | `	const char *zPrefix;` |
|        - | 13159 | `	SHA1Context sCtx;` |
|        - | 13160 | `	char zRandom[7];` |
|        - | 13161 | `	int nPrefix;` |
|        - | 13162 | `	int entropy;` |
|        - | 13163 | `	/* Generate a random string first */` |
|       25 | 13164 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 13165 | `	/* Initialize fields */` |
|       25 | 13166 | `	zPrefix = 0;` |
|       25 | 13167 | `	nPrefix = 0;` |
|       25 | 13168 | `	entropy = 0;` |
|       25 | 13169 | `	if( nArg > 0 ){` |
|        - | 13170 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 13171 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 13172 | `		if( nArg > 1 ){` |
|      ! 0 | 13173 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13174 | `		}` |
|      ! 0 | 13175 | `	}` |
|       25 | 13176 | `	SHA1Init(&sCtx);` |
|        - | 13177 | `	/* Generate the random ID */` |
|       25 | 13178 | `	if( nPrefix > 0 ){` |
|      ! 0 | 13179 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 13180 | `	}` |
|        - | 13181 | `	/* Append the random ID */` |
|       25 | 13182 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 13183 | `	/* Append the random string */` |
|       25 | 13184 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 13185 | `	/* Increment the number */` |
|       25 | 13186 | `	pVm->unique_id++;` |
|       25 | 13187 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 13188 | `	/* Hexify the digest */` |
|       25 | 13189 | `	sUniq.pCtx = pCtx;` |
|       25 | 13190 | `	sUniq.entropy = entropy;` |
|       25 | 13191 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 13192 | `	/* All done */` |
|       25 | 13193 | `	return PH7_OK;` |
|        1 | 13194 |  |
|        - | 13195 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13196 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13197 | `/*` |
|        - | 13198 | ` * Section:` |
|        - | 13199 | ` *  Language construct implementation as foreign functions.` |
|        - | 13200 | ` * Status:` |
|        - | 13201 | ` *    Stable.` |
|        - | 13202 | ` */` |
|        - | 13203 | `/*` |
|        - | 13204 | ` * void echo($string...)` |
|        - | 13205 | ` *  Output one or more messages.` |
|        - | 13206 | ` * Parameters` |
|        - | 13207 | ` *  $string` |
|        - | 13208 | ` *   Message to output.` |
|        - | 13209 | ` * Return` |
|        - | 13210 | ` *  NULL.` |
|        - | 13211 | ` */` |
|      ! 0 | 13212 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13213 |  |
|        - | 13214 | `	const char *zData;` |
|      ! 0 | 13215 | `	int nDataLen = 0;` |
|        - | 13216 | `	ph7_vm *pVm;` |
|        - | 13217 | `	int i,rc;` |
|        - | 13218 | `	/* Point to the target VM */` |
|      ! 0 | 13219 | `	pVm = pCtx->pVm;` |
|        - | 13220 | `	/* Output */` |
|      ! 0 | 13221 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 13222 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 13223 | `		if( nDataLen > 0 ){` |
|      ! 0 | 13224 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 13225 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 13226 | `			if( rc == SXERR_ABORT ){` |
|        - | 13227 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13228 | `				return PH7_ABORT;` |
|        - | 13229 | `			}` |
|      ! 0 | 13230 | `		}` |
|      ! 0 | 13231 | `	}` |
|      ! 0 | 13232 | `	return SXRET_OK;` |
|      ! 0 | 13233 |  |
|        - | 13234 | `/*` |
|        - | 13235 | ` * int print($string...)` |
|        - | 13236 | ` *  Output one or more messages.` |
|        - | 13237 | ` * Parameters` |
|        - | 13238 | ` *  $string` |
|        - | 13239 | ` *   Message to output.` |
|        - | 13240 | ` * Return` |
|        - | 13241 | ` *  1 always.` |
|        - | 13242 | ` */` |
|        2 | 13243 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13244 |  |
|        - | 13245 | `	const char *zData;` |
|        3 | 13246 | `	int nDataLen = 0;` |
|        - | 13247 | `	ph7_vm *pVm;` |
|        - | 13248 | `	int i,rc;` |
|        - | 13249 | `	/* Point to the target VM */` |
|        3 | 13250 | `	pVm = pCtx->pVm;` |
|        - | 13251 | `	/* Output */` |
|        5 | 13252 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 13253 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 13254 | `		if( nDataLen > 0 ){` |
|        3 | 13255 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 13256 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 13257 | `			if( rc == SXERR_ABORT ){` |
|        - | 13258 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 13259 | `				return PH7_ABORT;` |
|        - | 13260 | `			}` |
|        1 | 13261 | `		}` |
|        2 | 13262 | `	}` |
|        - | 13263 | `	/* Return 1 */` |
|        3 | 13264 | `	ph7_result_int(pCtx,1);` |
|        3 | 13265 | `	return SXRET_OK;` |
|        2 | 13266 |  |
|        - | 13267 | `/*` |
|        - | 13268 | ` * void exit(string $msg)` |
|        - | 13269 | ` * void exit(int $status)` |
|        - | 13270 | ` * void die(string $ms)` |
|        - | 13271 | ` * void die(int $status)` |
|        - | 13272 | ` *   Output a message and terminate program execution.` |
|        - | 13273 | ` * Parameter` |
|        - | 13274 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 13275 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 13276 | ` *  and not printed` |
|        - | 13277 | ` * Return` |
|        - | 13278 | ` *  NULL` |
|        - | 13279 | ` */` |
|      ! 0 | 13280 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 13281 |  |
|      ! 0 | 13282 | `	if( nArg > 0 ){` |
|      ! 0 | 13283 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 13284 | `			const char *zData;` |
|      ! 0 | 13285 | `			int iLen = 0;` |
|        - | 13286 | `			/* Print exit message */` |
|      ! 0 | 13287 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 13288 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 13289 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 13290 | `			sxi32 iExitStatus;` |
|        - | 13291 | `			/* Record exit status code */` |
|      ! 0 | 13292 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 13293 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 13294 | `		}` |
|      ! 0 | 13295 | `	}` |
|        - | 13296 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 13297 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 13298 | `	 */` |
|      ! 0 | 13299 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 13300 | `	return PH7_ABORT;` |
|      ! 0 | 13301 |  |
|        - | 13302 | `/*` |
|        - | 13303 | ` * bool isset($var,...)` |
|        - | 13304 | ` *  Finds out whether a variable is set.` |
|        - | 13305 | ` * Parameters` |
|        - | 13306 | ` *  One or more variable to check.` |
|        - | 13307 | ` * Return` |
|        - | 13308 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 13309 | ` */` |
|    94068 | 13310 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13311 |  |
|        - | 13312 | `	ph7_value *pObj;` |
|    94073 | 13313 | `	int res = 0;` |
|        - | 13314 | `	int i;` |
|    94073 | 13315 | `	if( nArg < 1 ){` |
|        - | 13316 | `		/* Missing arguments,return false */` |
|      ! 0 | 13317 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 13318 | `		return SXRET_OK;` |
|        - | 13319 | `	}` |
|        - | 13320 | `	/* Iterate over available arguments */` |
|   122919 | 13321 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    94083 | 13322 | `		pObj = apArg[i];` |
|    94083 | 13323 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 13324 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 13325 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 13326 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    64187 | 13327 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 13328 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 13329 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 13330 | `			}` |
|    32091 | 13331 | `		}` |
|    94083 | 13332 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    94083 | 13333 | `		if( !res ){` |
|        - | 13334 | `			/* Variable not set,return FALSE */` |
|    65237 | 13335 | `			ph7_result_bool(pCtx,0);` |
|    65237 | 13336 | `			return SXRET_OK;` |
|        - | 13337 | `		}` |
|    14428 | 13338 | `	}` |
|        - | 13339 | `	/* All given variable are set,return TRUE */` |
|    28841 | 13340 | `	ph7_result_bool(pCtx,1);` |
|    28841 | 13341 | `	return SXRET_OK;` |
|    47039 | 13342 |  |
|        - | 13343 | `/*` |
|        - | 13344 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 13345 | ` * frame,the reference table and discard it's contents.` |
|        - | 13346 | ` * This function never fail and always return SXRET_OK.` |
|        - | 13347 | ` */` |
|  3173934 | 13348 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 | 13349 |  |
|        - | 13350 | `	ph7_value *pObj;` |
|        - | 13351 | `	VmRefObj *pRef;` |
|  3173939 | 13352 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3173939 | 13353 | `	if( pObj ){` |
|        - | 13354 | `		/* Release the object */` |
|  3173939 | 13355 | `		PH7_MemObjRelease(pObj);` |
|  1586967 | 13356 | `	}` |
|        - | 13357 | `	/* Remove old reference links */` |
|  3173939 | 13358 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3173939 | 13359 | `	if( pRef ){` |
|  3173933 | 13360 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 13361 | `		/* Unlink from the reference table */` |
|  3173933 | 13362 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3173933 | 13363 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 13364 | `			VmSlot sFree;` |
|        - | 13365 | `			/* Restore to the free list */` |
|  3173925 | 13366 | `			sFree.nIdx = nObjIdx;` |
|  3173925 | 13367 | `			sFree.pUserData = 0;` |
|  3173925 | 13368 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1586960 | 13369 | `		}` |
|  1586964 | 13370 | `	}` |
|  3173939 | 13371 | `	return SXRET_OK;` |
|        5 | 13372 |  |
|        - | 13373 | `/*` |
|        - | 13374 | ` * void unset($var,...)` |
|        - | 13375 | ` *   Unset one or more given variable.` |
|        - | 13376 | ` * Parameters` |
|        - | 13377 | ` *  One or more variable to unset.` |
|        - | 13378 | ` * Return` |
|        - | 13379 | ` *  Nothing.` |
|        - | 13380 | ` */` |
|     7588 | 13381 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13382 |  |
|        - | 13383 | `	ph7_value *pObj;` |
|        - | 13384 | `	ph7_vm *pVm;` |
|        - | 13385 | `	int i;` |
|        - | 13386 | `	/* Point to the target VM */` |
|     7593 | 13387 | `	pVm = pCtx->pVm;` |
|        - | 13388 | `	/* Iterate and unset */` |
|    15181 | 13389 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7593 | 13390 | `		pObj = apArg[i];` |
|     7593 | 13391 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      841 | 13392 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 13393 | `				/* Throw an error */` |
|      ! 0 | 13394 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 13395 | `			}` |
|      423 | 13396 | `		}else{` |
|     6757 | 13397 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 13398 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6757 | 13399 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6751 | 13400 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3373 | 13401 | `			}` |
|        - | 13402 | `		}` |
|     3799 | 13403 | `	}` |
|     7593 | 13404 | `	return SXRET_OK;` |
|        5 | 13405 |  |
|        - | 13406 | `/*` |
|        - | 13407 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 13408 | ` */` |
|      120 | 13409 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 13410 |  |
|      121 | 13411 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      121 | 13412 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13413 | `	ph7_value *pObj;` |
|        - | 13414 | `	sxu32 nIdx;` |
|        - | 13415 | `	/* Extract the memory object */` |
|      121 | 13416 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      121 | 13417 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      121 | 13418 | `	if( pObj ){` |
|      121 | 13419 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      119 | 13420 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 13421 | `				SyString sName;` |
|        - | 13422 | `				ph7_value sKey;` |
|        - | 13423 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 13424 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 13425 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      119 | 13426 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      119 | 13427 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      119 | 13428 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      119 | 13429 | `				PH7_MemObjRelease(&sKey);` |
|       59 | 13430 | `			}` |
|       59 | 13431 | `		}` |
|       60 | 13432 | `	}` |
|      121 | 13433 | `	return SXRET_OK;` |
|        1 | 13434 |  |
|        - | 13435 | `/*` |
|        - | 13436 | ` * array get_defined_vars(void)` |
|        - | 13437 | ` *  Returns an array of all defined variables.` |
|        - | 13438 | ` * Parameter` |
|        - | 13439 | ` *  None` |
|        - | 13440 | ` * Return` |
|        - | 13441 | ` *  An array with all the variables defined in the current scope.` |
|        - | 13442 | ` */` |
|        2 | 13443 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13444 |  |
|        3 | 13445 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13446 | `	ph7_value *pArray;` |
|        - | 13447 | `	/* Create a new array */` |
|        3 | 13448 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13449 | ` 	if( pArray == 0 ){` |
|      ! 0 | 13450 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13451 | `		SXUNUSED(apArg);` |
|        - | 13452 | `		/* Return NULL */` |
|      ! 0 | 13453 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13454 | `		return SXRET_OK;` |
|        - | 13455 | `	}` |
|        - | 13456 | `	/* Superglobals first */` |
|        3 | 13457 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13458 | `	/* Then variable defined in the current frame */` |
|        3 | 13459 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13460 | `	/* Finally,return the created array */` |
|        3 | 13461 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13462 | `	return SXRET_OK;` |
|        2 | 13463 |  |
|        - | 13464 | `/*` |
|        - | 13465 | ` * bool gettype($var)` |
|        - | 13466 | ` *  Get the type of a variable` |
|        - | 13467 | ` * Parameters` |
|        - | 13468 | ` *   $var` |
|        - | 13469 | ` *    The variable being type checked.` |
|        - | 13470 | ` * Return` |
|        - | 13471 | ` *   String representation of the given variable type.` |
|        - | 13472 | ` */` |
|       32 | 13473 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 13474 |  |
|       35 | 13475 | `	const char *zType = "Empty";` |
|       35 | 13476 | `	if( nArg > 0 ){` |
|       35 | 13477 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 13478 | `	}` |
|        - | 13479 | `	/* Return the variable type */` |
|       35 | 13480 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       35 | 13481 | `	return SXRET_OK;` |
|        3 | 13482 |  |
|        - | 13483 | `/*` |
|        - | 13484 | ` * string get_resource_type(resource $handle)` |
|        - | 13485 | ` *  This function gets the type of the given resource.` |
|        - | 13486 | ` * Parameters` |
|        - | 13487 | ` *  $handle` |
|        - | 13488 | ` *  The evaluated resource handle.` |
|        - | 13489 | ` * Return` |
|        - | 13490 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13491 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13492 | ` *  the return value will be the string Unknown.` |
|        - | 13493 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13494 | ` *  is not a resource.` |
|        - | 13495 | ` */` |
|        2 | 13496 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13497 |  |
|        3 | 13498 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13499 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13500 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13501 | `		return PH7_OK;` |
|        - | 13502 | `	}` |
|        3 | 13503 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13504 | `	return SXRET_OK;` |
|        2 | 13505 |  |
|        - | 13506 | `/*` |
|        - | 13507 | ` * void var_dump(expression,....)` |
|        - | 13508 | ` *   var_dump � Dumps information about a variable` |
|        - | 13509 | ` * Parameters` |
|        - | 13510 | ` *   One or more expression to dump.` |
|        - | 13511 | ` * Returns` |
|        - | 13512 | ` *  Nothing.` |
|        - | 13513 | ` */` |
|      218 | 13514 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 13515 |  |
|        - | 13516 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13517 | `	int i;` |
|      221 | 13518 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13519 | `	/* Dump one or more expressions */` |
|      445 | 13520 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      227 | 13521 | `		ph7_value *pObj = apArg[i];` |
|        - | 13522 | `		/* Reset the working buffer */` |
|      227 | 13523 | `		SyBlobReset(&sDump);` |
|        - | 13524 | `		/* Dump the given expression */` |
|      227 | 13525 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13526 | `		/* Output */` |
|      227 | 13527 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      227 | 13528 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13529 | `		}` |
|      115 | 13530 | `	}` |
|        - | 13531 | `	/* Release the working buffer */` |
|      221 | 13532 | `	SyBlobRelease(&sDump);` |
|      221 | 13533 | `	return SXRET_OK;` |
|        3 | 13534 |  |
|        - | 13535 | `/*` |
|        - | 13536 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13537 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13538 | ` * Parameters` |
|        - | 13539 | ` *   expression: Expression to dump` |
|        - | 13540 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13541 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13542 | ` *            print_r() will return the information rather than print it.` |
|        - | 13543 | ` * Return` |
|        - | 13544 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13545 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13546 | ` */` |
|       16 | 13547 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13548 |  |
|       17 | 13549 | `	int ret_string = 0;` |
|        - | 13550 | `	SyBlob sDump;` |
|       17 | 13551 | `	if( nArg < 1 ){` |
|        - | 13552 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13553 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13554 | `		return SXRET_OK;` |
|        - | 13555 | `	}` |
|       17 | 13556 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13557 | `	if ( nArg > 1 ){` |
|        - | 13558 | `		/* Where to redirect output */` |
|       11 | 13559 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13560 | `	}` |
|        - | 13561 | `	/* Generate dump */` |
|       17 | 13562 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13563 | `	if( !ret_string ){` |
|        - | 13564 | `		/* Output dump */` |
|        7 | 13565 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13566 | `		/* Return true */` |
|        7 | 13567 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13568 | `	}else{` |
|        - | 13569 | `		/* Generated dump as return value */` |
|       11 | 13570 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13571 | `	}` |
|        - | 13572 | `	/* Release the working buffer */` |
|       17 | 13573 | `	SyBlobRelease(&sDump);` |
|       17 | 13574 | `	return SXRET_OK;` |
|        9 | 13575 |  |
|        - | 13576 | `/*` |
|        - | 13577 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13578 | ` * Same job as print_r. (see coment above)` |
|        - | 13579 | ` */` |
|        2 | 13580 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13581 |  |
|        3 | 13582 | `	int ret_string = 0;` |
|        - | 13583 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13584 | `	if( nArg < 1 ){` |
|        - | 13585 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13586 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13587 | `		return SXRET_OK;` |
|        - | 13588 | `	}` |
|        3 | 13589 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13590 | `	if ( nArg > 1 ){` |
|        - | 13591 | `		/* Where to redirect output */` |
|        3 | 13592 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13593 | `	}` |
|        - | 13594 | `	/* Generate dump */` |
|        3 | 13595 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13596 | `	if( !ret_string ){` |
|        - | 13597 | `		/* Output dump */` |
|      ! 0 | 13598 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13599 | `		/* Return NULL */` |
|      ! 0 | 13600 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13601 | `	}else{` |
|        - | 13602 | `		/* Generated dump as return value */` |
|        3 | 13603 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13604 | `	}` |
|        - | 13605 | `	/* Release the working buffer */` |
|        3 | 13606 | `	SyBlobRelease(&sDump);` |
|        3 | 13607 | `	return SXRET_OK;` |
|        2 | 13608 |  |
|        - | 13609 | `/*` |
|        - | 13610 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13611 | ` *  Set/get the various assert flags.` |
|        - | 13612 | ` * Parameter` |
|        - | 13613 | ` * $what` |
|        - | 13614 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13615 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13616 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13617 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13618 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13619 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13620 | ` * $value` |
|        - | 13621 | ` *   An optional new value for the option.` |
|        - | 13622 | ` * Return` |
|        - | 13623 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13624 | ` */` |
|       28 | 13625 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13626 |  |
|       33 | 13627 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13628 | `	int iOption;` |
|        - | 13629 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       33 | 13630 | `	if( nArg < 1 ){` |
|        3 | 13631 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13632 | `			"ArgumentCountError",` |
|        - | 13633 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13634 | `			);` |
|        - | 13635 | `	}` |
|        - | 13636 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13637 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       31 | 13638 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13639 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13640 | `			"TypeError",` |
|        - | 13641 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13642 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13643 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13644 | `			);` |
|        - | 13645 | `	}` |
|       31 | 13646 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13647 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13648 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13649 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       31 | 13650 | `	switch( iOption ){` |
|        5 | 13651 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13652 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13653 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13654 | `		if( nArg > 1 ){` |
|        5 | 13655 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13656 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13657 | `			}else{` |
|        3 | 13658 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13659 | `			}` |
|        2 | 13660 | `		}` |
|       12 | 13661 | `		break;` |
|        1 | 13662 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13663 | `		/* Return old callback or null */` |
|        3 | 13664 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13665 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13666 | `		}else{` |
|        3 | 13667 | `			ph7_result_null(pCtx);` |
|        - | 13668 | `		}` |
|        3 | 13669 | `		if( nArg > 1 ){` |
|      ! 0 | 13670 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13671 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13672 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13673 | `			}else{` |
|      ! 0 | 13674 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13675 | `			}` |
|      ! 0 | 13676 | `		}` |
|        3 | 13677 | `		break;` |
|        5 | 13678 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13679 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13680 | `		if( nArg > 1 ){` |
|        5 | 13681 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13682 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13683 | `			}else{` |
|        3 | 13684 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13685 | `			}` |
|        2 | 13686 | `		}` |
|       11 | 13687 | `		break;` |
|      ! 0 | 13688 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13689 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13690 | `		break;` |
|        1 | 13691 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13692 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13693 | `		break;` |
|      ! 0 | 13694 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13695 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13696 | `		break;` |
|        1 | 13697 | `	default:` |
|        - | 13698 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13699 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13700 | `			"ValueError",` |
|        - | 13701 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13702 | `			);` |
|        - | 13703 | `	}` |
|       29 | 13704 | `	return PH7_OK;` |
|       19 | 13705 |  |
|        - | 13706 | `/*` |
|        - | 13707 | ` * bool assert(mixed $assertion)` |
|        - | 13708 | ` *  Checks if assertion is FALSE.` |
|        - | 13709 | ` * Parameter` |
|        - | 13710 | ` *  $assertion` |
|        - | 13711 | ` *    The assertion to test.` |
|        - | 13712 | ` * Return` |
|        - | 13713 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13714 | ` */` |
|       24 | 13715 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13716 |  |
|       29 | 13717 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13718 | `	int iFlags,iResult;` |
|        - | 13719 | `	const char *zDesc;` |
|        - | 13720 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       29 | 13721 | `	if( nArg < 1 ){` |
|        3 | 13722 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13723 | `			"ArgumentCountError",` |
|        - | 13724 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13725 | `			);` |
|        - | 13726 | `	}` |
|       27 | 13727 | `	iFlags = pVm->iAssertFlags;` |
|       27 | 13728 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13729 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13730 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13731 | `		return PH7_OK;` |
|        - | 13732 | `	}` |
|        - | 13733 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       27 | 13734 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       27 | 13735 | `	if( !iResult ){` |
|        - | 13736 | `		/* Assertion failed */` |
|        - | 13737 | `		/* Extract optional description */` |
|       16 | 13738 | `		zDesc = 0;` |
|       16 | 13739 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13740 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13741 | `		}` |
|       16 | 13742 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13743 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13744 | `			ph7_value sFile,sLine;` |
|        - | 13745 | `			ph7_value *apCbArg[3];` |
|        - | 13746 | `			SyString *pFile;` |
|        - | 13747 | `			/* Extract the processed script */` |
|      ! 0 | 13748 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13749 | `			if( pFile == 0 ){` |
|      ! 0 | 13750 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13751 | `			}` |
|        - | 13752 | `			/* Invoke the callback */` |
|      ! 0 | 13753 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13754 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13755 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13756 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13757 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13758 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13759 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13760 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13761 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13762 | `		}` |
|       16 | 13763 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13764 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13765 | `			return PH7_ABORT;` |
|        - | 13766 | `		}` |
|        - | 13767 | `		/* PHP 8: throw AssertionError by default */` |
|       16 | 13768 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13769 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13770 | `				"AssertionError",` |
|        - | 13771 | `				"%s",` |
|        1 | 13772 | `				zDesc` |
|        - | 13773 | `				);` |
|      ! 0 | 13774 | `		}else{` |
|       13 | 13775 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13776 | `				"AssertionError",` |
|        - | 13777 | `				"assert(false)"` |
|        - | 13778 | `				);` |
|        - | 13779 | `		}` |
|        - | 13780 | `	}` |
|        - | 13781 | `	/* Assertion passed */` |
|       11 | 13782 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13783 | `	return PH7_OK;` |
|       17 | 13784 |  |
|        - | 13785 | `/*` |
|        - | 13786 | ` * Section:` |
|        - | 13787 | ` *  Error reporting functions.` |
|        - | 13788 | ` * Status:` |
|        - | 13789 | ` *    Stable.` |
|        - | 13790 | ` */` |
|        - | 13791 | `/*` |
|        - | 13792 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13793 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13794 | ` * Parameters` |
|        - | 13795 | ` *  $error_msg` |
|        - | 13796 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13797 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13798 | ` * $error_type` |
|        - | 13799 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13800 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13801 | ` * Return` |
|        - | 13802 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13803 | ` */` |
|       12 | 13804 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13805 |  |
|       17 | 13806 | `	int nErr = PH7_CTX_NOTICE;` |
|       17 | 13807 | `	int rc = PH7_OK;` |
|       17 | 13808 | `	if( nArg > 0 ){` |
|        - | 13809 | `		const char *zErr;` |
|        - | 13810 | `		int nLen;` |
|        - | 13811 | `		/* Extract the error message */` |
|       14 | 13812 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 13813 | `		if( nArg > 1 ){` |
|        - | 13814 | `			/* Extract the error type */` |
|       14 | 13815 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       14 | 13816 | `			switch( nErr ){` |
|        1 | 13817 | `			case 1:   /* E_ERROR */` |
|        - | 13818 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13819 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13820 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13821 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13822 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13823 | `				break;` |
|        1 | 13824 | `			case 2:   /* E_WARNING */` |
|        - | 13825 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13826 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13827 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13828 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13829 | `				break;` |
|        3 | 13830 | `			default:` |
|        9 | 13831 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13832 | `				break;` |
|        - | 13833 | `			}` |
|        5 | 13834 | `		}` |
|        - | 13835 | `		/* Report error */` |
|       14 | 13836 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       14 | 13837 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13838 | `			return rc;` |
|        - | 13839 | `		}` |
|        - | 13840 | `		/* Return true */` |
|       14 | 13841 | `		ph7_result_bool(pCtx,1);` |
|        9 | 13842 | `	}else{` |
|        - | 13843 | `		/* Missing arguments,return FALSE */` |
|        3 | 13844 | `		ph7_result_bool(pCtx,0);` |
|        - | 13845 | `	}` |
|       17 | 13846 | `	return rc;` |
|       11 | 13847 |  |
|        - | 13848 | `/*` |
|        - | 13849 | ` * int error_reporting([int $level])` |
|        - | 13850 | ` *  Sets which PHP errors are reported.` |
|        - | 13851 | ` * Parameters` |
|        - | 13852 | ` *  $level` |
|        - | 13853 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13854 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13855 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13856 | ` *   levels will not always behave as expected.` |
|        - | 13857 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13858 | ` *   in the predefined constants.` |
|        - | 13859 | ` * Return` |
|        - | 13860 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13861 | ` *   parameter is given.` |
|        - | 13862 | ` */` |
|       32 | 13863 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 13864 |  |
|       37 | 13865 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13866 | `	int nOld;` |
|        - | 13867 | `	/* Extract the old reporting level */` |
|       37 | 13868 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       37 | 13869 | `	if( nArg > 0 ){` |
|        - | 13870 | `		int nNew;` |
|        - | 13871 | `		/* Extract the desired error reporting level */` |
|       31 | 13872 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       31 | 13873 | `		if( !nNew ){` |
|        - | 13874 | `			/* Do not report errors at all */` |
|        5 | 13875 | `			pVm->bErrReport = 0;` |
|        3 | 13876 | `		}else{` |
|        - | 13877 | `			/* Report all errors */` |
|       27 | 13878 | `			pVm->bErrReport = 1;` |
|        - | 13879 | `		}` |
|       13 | 13880 | `	}` |
|        - | 13881 | `	/* Return the old level */` |
|       37 | 13882 | `	ph7_result_int(pCtx,nOld);` |
|       37 | 13883 | `	return PH7_OK;` |
|        5 | 13884 |  |
|        - | 13885 | `/*` |
|        - | 13886 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13887 | ` *  Send an error message somewhere.` |
|        - | 13888 | ` * Parameter` |
|        - | 13889 | ` *  $message` |
|        - | 13890 | ` *   The error message that should be logged.` |
|        - | 13891 | ` *  $message_type` |
|        - | 13892 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13893 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13894 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13895 | ` *       This is the default option.` |
|        - | 13896 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13897 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13898 | ` *    2  No longer an option.` |
|        - | 13899 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13900 | ` *       to the end of the message string.` |
|        - | 13901 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13902 | ` *  $destination` |
|        - | 13903 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13904 | ` *  $extra_headers` |
|        - | 13905 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13906 | ` * Return` |
|        - | 13907 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13908 | ` * NOTE:` |
|        - | 13909 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13910 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13911 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13912 | ` *  Otherwise this function is no-op.` |
|        - | 13913 | ` */` |
|        4 | 13914 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13915 |  |
|        - | 13916 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13917 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13918 | `	int iType = 0;` |
|        5 | 13919 | `	if( nArg < 1 ){` |
|        - | 13920 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13921 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13922 | `		return PH7_OK;` |
|        - | 13923 | `	}` |
|        5 | 13924 | `	if( pVm->xErrLog  ){` |
|        - | 13925 | `		/* Invoke the user callback */` |
|      ! 0 | 13926 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13927 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13928 | `		if( nArg > 1 ){` |
|      ! 0 | 13929 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13930 | `			if( nArg > 2 ){` |
|      ! 0 | 13931 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13932 | `				if( nArg > 3 ){` |
|      ! 0 | 13933 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13934 | `				}` |
|      ! 0 | 13935 | `			}` |
|      ! 0 | 13936 | `		}` |
|      ! 0 | 13937 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13938 | `	}` |
|        - | 13939 | `	/* Retun TRUE */` |
|        5 | 13940 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13941 | `	return PH7_OK;` |
|        3 | 13942 |  |
|        - | 13943 | `/*` |
|        - | 13944 | ` * bool restore_exception_handler(void)` |
|        - | 13945 | ` *  Restores the previously defined exception handler function.` |
|        - | 13946 | ` * Parameter` |
|        - | 13947 | ` *  None` |
|        - | 13948 | ` * Return` |
|        - | 13949 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13950 | ` */` |
|        4 | 13951 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13952 |  |
|        5 | 13953 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13954 | `	ph7_value *pOld,*pNew;` |
|        - | 13955 | `	/* Point to the old and the new handler */` |
|        5 | 13956 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13957 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13958 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13959 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13960 | `		SXUNUSED(apArg);` |
|        - | 13961 | `		/* No installed handler,return FALSE */` |
|        5 | 13962 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13963 | `		return PH7_OK;` |
|        - | 13964 | `	}` |
|        - | 13965 | `	/* Copy the old handler */` |
|      ! 0 | 13966 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13967 | `	PH7_MemObjRelease(pOld);` |
|        - | 13968 | `	/* Return TRUE */` |
|      ! 0 | 13969 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13970 | `	return PH7_OK;` |
|        3 | 13971 |  |
|        - | 13972 | `/*` |
|        - | 13973 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13974 | ` *  Sets a user-defined exception handler function.` |
|        - | 13975 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13976 | ` * NOTE` |
|        - | 13977 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13978 | ` *  the satndard PHP engine.` |
|        - | 13979 | ` * Parameters` |
|        - | 13980 | ` *  $exception_handler` |
|        - | 13981 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13982 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13983 | ` *   that was thrown.` |
|        - | 13984 | ` *  Note:` |
|        - | 13985 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13986 | ` * Return` |
|        - | 13987 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13988 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13989 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13990 | ` */` |
|        4 | 13991 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13992 |  |
|        6 | 13993 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13994 | `	ph7_value *pOld,*pNew;` |
|        - | 13995 | `	/* Point to the old and the new handler */` |
|        6 | 13996 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13997 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13998 | `	/* Return the old handler */` |
|        6 | 13999 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 14000 | `	if( nArg > 0 ){` |
|        6 | 14001 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14002 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 14003 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 14004 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14005 | `		}else{` |
|        6 | 14006 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14007 | `			/* Install the new handler */` |
|        6 | 14008 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14009 | `		}` |
|        2 | 14010 | `	}` |
|        6 | 14011 | `	return PH7_OK;` |
|        2 | 14012 |  |
|        - | 14013 | `/*` |
|        - | 14014 | ` * bool restore_error_handler(void)` |
|        - | 14015 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 14016 | ` * Parameters:` |
|        - | 14017 | ` *  None.` |
|        - | 14018 | ` * Return` |
|        - | 14019 | ` *  Always TRUE.` |
|        - | 14020 | ` */` |
|        6 | 14021 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14022 |  |
|        8 | 14023 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14024 | `	ph7_value *pOld,*pNew;` |
|        - | 14025 | `	/* Point to the old and the new handler */` |
|        8 | 14026 | `	pOld = &pVm->aErrCB[0];` |
|        8 | 14027 | `	pNew = &pVm->aErrCB[1];` |
|        8 | 14028 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 14029 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 14030 | `		SXUNUSED(apArg);` |
|        - | 14031 | `		/* No installed callback,return FALSE */` |
|        8 | 14032 | `		ph7_result_bool(pCtx,0);` |
|        8 | 14033 | `		return PH7_OK;` |
|        - | 14034 | `	}` |
|        - | 14035 | `	/* Copy the old callback */` |
|      ! 0 | 14036 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 14037 | `	PH7_MemObjRelease(pOld);` |
|        - | 14038 | `	/* Return TRUE */` |
|      ! 0 | 14039 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 14040 | `	return PH7_OK;` |
|        5 | 14041 |  |
|        - | 14042 | `/*` |
|        - | 14043 | ` * value set_error_handler(callable $error_handler)` |
|        - | 14044 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 14045 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 14046 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 14047 | ` *  Sets a user-defined error handler function.` |
|        - | 14048 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 14049 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 14050 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 14051 | ` *  conditions (using trigger_error()).` |
|        - | 14052 | ` * Parameters` |
|        - | 14053 | ` *  $error_handler` |
|        - | 14054 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 14055 | ` *   describing the error.` |
|        - | 14056 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 14057 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 14058 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 14059 | ` *   The function can be shown as:` |
|        - | 14060 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 14061 | ` *     errno` |
|        - | 14062 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 14063 | ` *   errstr` |
|        - | 14064 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 14065 | ` *   errfile` |
|        - | 14066 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 14067 | ` *     was raised in, as a string.` |
|        - | 14068 | ` *  Note:` |
|        - | 14069 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 14070 | ` * Return` |
|        - | 14071 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 14072 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 14073 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 14074 | ` */` |
|    11004 | 14075 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 14076 |  |
|    11007 | 14077 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14078 | `	ph7_value *pOld,*pNew;` |
|        - | 14079 | `	/* Point to the old and the new handler */` |
|    11007 | 14080 | `	pOld = &pVm->aErrCB[0];` |
|    11007 | 14081 | `	pNew = &pVm->aErrCB[1];` |
|        - | 14082 | `	/* Return the old handler */` |
|    11007 | 14083 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    11007 | 14084 | `	if( nArg > 0 ){` |
|    11007 | 14085 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 14086 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5497 | 14087 | `			PH7_MemObjRelease(pNew);` |
|     5497 | 14088 | `			ph7_result_bool(pCtx,1);` |
|     2749 | 14089 | `		}else{` |
|     5511 | 14090 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 14091 | `			/* Install the new handler */` |
|     5511 | 14092 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 14093 | `		}` |
|     5502 | 14094 | `	}` |
|    11007 | 14095 | `	return PH7_OK;` |
|        3 | 14096 |  |
|        - | 14097 | `/*` |
|        - | 14098 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 14099 | ` *  Generates a backtrace.` |
|        - | 14100 | ` * Paramaeter` |
|        - | 14101 | ` *  $options` |
|        - | 14102 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 14103 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 14104 | ` *   all the function/method arguments, to save memory.` |
|        - | 14105 | ` * $limit` |
|        - | 14106 | ` *   (Not Used)` |
|        - | 14107 | ` * Return` |
|        - | 14108 | ` *  An array.The possible returned elements are as follows:` |
|        - | 14109 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 14110 | ` *          Name        Type      Description` |
|        - | 14111 | ` *          ------      ------     -----------` |
|        - | 14112 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 14113 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 14114 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 14115 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 14116 | ` *          object      object    The current object.` |
|        - | 14117 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 14118 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 14119 | ` */` |
|      996 | 14120 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 14121 |  |
|     1001 | 14122 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14123 | `	ph7_value *pArray;` |
|        - | 14124 | `	ph7_class *pClass;` |
|        - | 14125 | `	ph7_value *pValue;` |
|        - | 14126 | `	SyString *pFile;` |
|        - | 14127 | `	/* Create a new array */` |
|     1001 | 14128 | `	pArray = ph7_context_new_array(pCtx);` |
|     1001 | 14129 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     1001 | 14130 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14131 | `		/* Out of memory,return NULL */` |
|      ! 0 | 14132 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 14133 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14134 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14135 | `		SXUNUSED(apArg);` |
|      ! 0 | 14136 | `		return PH7_OK;` |
|        - | 14137 | `	}` |
|        - | 14138 | `	/* Dump running function name and it's arguments  */` |
|     1001 | 14139 | `	if( pVm->pFrame->pParent ){` |
|     1001 | 14140 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 14141 | `		ph7_vm_func *pFunc;` |
|        - | 14142 | `		ph7_value *pArg;` |
|     1001 | 14143 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     1001 | 14144 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     1001 | 14145 | `		if( pFrame->pParent && pFunc ){` |
|     1001 | 14146 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|     1001 | 14147 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|     1001 | 14148 | `			ph7_value_reset_string_cursor(pValue);` |
|      498 | 14149 | `		}` |
|        - | 14150 | `		/* Function arguments */` |
|     1001 | 14151 | `		pArg = ph7_context_new_array(pCtx);` |
|     1001 | 14152 | `		if( pArg  ){` |
|        - | 14153 | `			ph7_value *pObj;` |
|        - | 14154 | `			VmSlot *aSlot;` |
|        - | 14155 | `			sxu32 n;` |
|        - | 14156 | `			/* Start filling the array with the given arguments */` |
|     1001 | 14157 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3993 | 14158 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2997 | 14159 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2997 | 14160 | `				if( pObj ){` |
|     2997 | 14161 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1496 | 14162 | `				}` |
|     1501 | 14163 | `			}` |
|        - | 14164 | `			/* Save the array */` |
|     1001 | 14165 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      498 | 14166 | `		}` |
|      498 | 14167 | `	}` |
|     1001 | 14168 | `	ph7_value_int(pValue,1);` |
|        - | 14169 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 14170 | `	 * line numbers at run-time. )` |
|        - | 14171 | `	 */` |
|     1001 | 14172 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 14173 | `	/* Current processed script */` |
|     1001 | 14174 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1001 | 14175 | `	if( pFile ){` |
|     1001 | 14176 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|     1001 | 14177 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|     1001 | 14178 | `		ph7_value_reset_string_cursor(pValue);` |
|      498 | 14179 | `	}` |
|        - | 14180 | `	/* Top class */` |
|     1001 | 14181 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     1001 | 14182 | `	if( pClass ){` |
|      997 | 14183 | `		ph7_value_reset_string_cursor(pValue);` |
|      997 | 14184 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      997 | 14185 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      496 | 14186 | `	}` |
|        - | 14187 | `	/* Return the freshly created array */` |
|     1001 | 14188 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14189 | `	/*` |
|        - | 14190 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 14191 | `	 * as soon we return from this function.` |
|        - | 14192 | `	 */` |
|     1001 | 14193 | `	return PH7_OK;` |
|      503 | 14194 |  |
|        - | 14195 | `/*` |
|        - | 14196 | ` * Generate a small backtrace.` |
|        - | 14197 | ` * Store the generated dump in the given BLOB` |
|        - | 14198 | ` */` |
|        4 | 14199 | `static int VmMiniBacktrace(` |
|        - | 14200 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14201 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 14202 | `	)` |
|        1 | 14203 |  |
|        5 | 14204 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14205 | `	ph7_vm_func *pFunc;` |
|        - | 14206 | `	ph7_class *pClass;` |
|        - | 14207 | `	SyString *pFile;` |
|        - | 14208 | `	/* Called function */` |
|        5 | 14209 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 14210 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 14211 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14212 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 14213 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 14214 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 14215 | `	}else{` |
|      ! 0 | 14216 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 14217 | `	}` |
|        5 | 14218 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 14219 | `	/* Current processed script */` |
|        5 | 14220 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 14221 | `	if( pFile ){` |
|        5 | 14222 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 14223 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 14224 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 14225 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 14226 | `	}` |
|        - | 14227 | `	/* Top class */` |
|        5 | 14228 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 14229 | `	if( pClass ){` |
|      ! 0 | 14230 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 14231 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 14232 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 14233 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 14234 | `	}` |
|        5 | 14235 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 14236 | `	/* All done */` |
|        5 | 14237 | `	return SXRET_OK;` |
|        1 | 14238 |  |
|        - | 14239 | `/*` |
|        - | 14240 | ` * void debug_print_backtrace()` |
|        - | 14241 | ` *  Prints a backtrace` |
|        - | 14242 | ` * Parameters` |
|        - | 14243 | ` * None` |
|        - | 14244 | ` * Return` |
|        - | 14245 | ` * NULL` |
|        - | 14246 | ` */` |
|        2 | 14247 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14248 |  |
|        3 | 14249 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14250 | `	SyBlob sDump;` |
|        3 | 14251 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14252 | `	/* Generate the backtrace */` |
|        3 | 14253 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14254 | `	/* Output backtrace */` |
|        3 | 14255 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 14256 | `	/* All done,cleanup */` |
|        3 | 14257 | `	SyBlobRelease(&sDump);` |
|        1 | 14258 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14259 | `	SXUNUSED(apArg);` |
|        3 | 14260 | `	return PH7_OK;` |
|        1 | 14261 |  |
|        - | 14262 | `/*` |
|        - | 14263 | ` * string debug_string_backtrace()` |
|        - | 14264 | ` *  Generate a backtrace` |
|        - | 14265 | ` * Parameters` |
|        - | 14266 | ` * None` |
|        - | 14267 | ` * Return` |
|        - | 14268 | ` *  A mini backtrace().` |
|        - | 14269 | ` * Note that this is a symisc extension.` |
|        - | 14270 | ` */` |
|        2 | 14271 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14272 |  |
|        3 | 14273 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14274 | `	SyBlob sDump;` |
|        3 | 14275 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 14276 | `	/* Generate the backtrace */` |
|        3 | 14277 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 14278 | `	/* Return the backtrace */` |
|        3 | 14279 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 14280 | `	/* All done,cleanup */` |
|        3 | 14281 | `	SyBlobRelease(&sDump);` |
|        1 | 14282 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14283 | `	SXUNUSED(apArg);` |
|        3 | 14284 | `	return PH7_OK;` |
|        1 | 14285 |  |
|        - | 14286 | `/*` |
|        - | 14287 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 14288 | ` * exception is triggered.` |
|        - | 14289 | ` */` |
|      512 | 14290 | `static sxi32 VmUncaughtException(` |
|        - | 14291 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 14292 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14293 | `	)` |
|        4 | 14294 |  |
|        - | 14295 | `	ph7_value *apArg[2],sArg;` |
|      516 | 14296 | `	int nArg = 1;` |
|        - | 14297 | `	sxi32 rc;` |
|      516 | 14298 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 14299 | `		/* Nesting limit reached */` |
|      ! 0 | 14300 | `		return SXRET_OK;` |
|        - | 14301 | `	}` |
|        - | 14302 | `	/* Call any exception handler if available */` |
|      516 | 14303 | `	PH7_MemObjInit(pVm,&sArg);` |
|      516 | 14304 | `	if( pThis ){` |
|        - | 14305 | `		/* Load the exception instance */` |
|      516 | 14306 | `		sArg.x.pOther = pThis;` |
|      516 | 14307 | `		pThis->iRef++;` |
|      516 | 14308 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      260 | 14309 | `	}else{` |
|      ! 0 | 14310 | `		nArg = 0;` |
|        - | 14311 | `	}` |
|      516 | 14312 | `	apArg[0] = &sArg;` |
|        - | 14313 | `	/* Call the exception handler if available */` |
|      516 | 14314 | `	pVm->nExceptDepth++;` |
|      516 | 14315 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      516 | 14316 | `	pVm->nExceptDepth--;` |
|      516 | 14317 | `	if( rc != SXRET_OK ){` |
|        - | 14318 | `		SyBlob sMsgBuf;` |
|      514 | 14319 | `		const char *zClass = "Exception";` |
|      514 | 14320 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 14321 | `		const char *zMsg;` |
|        - | 14322 | `		sxu32 nMsg;` |
|        - | 14323 | `		const char *zFuncName;` |
|        - | 14324 | `		int nFuncLen;` |
|      514 | 14325 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      514 | 14326 | `		if( pThis ){` |
|        - | 14327 | `			ph7_class_method *pGetMessage;` |
|        - | 14328 | `			ph7_value sMsg;` |
|        - | 14329 | `			const char *zTmp;` |
|        - | 14330 | `			int nTmp;` |
|      514 | 14331 | `			zClass = pThis->pClass->sName.zString;` |
|      514 | 14332 | `			nClass = pThis->pClass->sName.nByte;` |
|      514 | 14333 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      514 | 14334 | `			if( pGetMessage ){` |
|      514 | 14335 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      514 | 14336 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      514 | 14337 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      514 | 14338 | `					if( zTmp && nTmp > 0 ){` |
|      514 | 14339 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 14340 | `					}` |
|      255 | 14341 | `				}` |
|      514 | 14342 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 14343 | `			}` |
|      255 | 14344 | `		}` |
|      514 | 14345 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      514 | 14346 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      514 | 14347 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      514 | 14348 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      514 | 14349 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 14350 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      514 | 14351 | `		rc = SXERR_ABORT;` |
|      255 | 14352 | `	}` |
|      516 | 14353 | `	PH7_MemObjRelease(&sArg);` |
|      516 | 14354 | `	return rc;` |
|      260 | 14355 |  |
|        - | 14356 | `/*` |
|        - | 14357 | ` * Throw a user exception.` |
|        - | 14358 | ` *` |
|        - | 14359 | ` * Exception dispatch follows this sequence:` |
|        - | 14360 | ` *` |
|        - | 14361 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 14362 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 14363 | ` *` |
|        - | 14364 | ` * 2. If NO catch matches:` |
|        - | 14365 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 14366 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 14367 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 14368 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 14369 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 14370 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 14371 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 14372 | ` *` |
|        - | 14373 | ` * 3. If a catch DOES match:` |
|        - | 14374 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 14375 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 14376 | ` *       inside the catch body from immediately propagating past our` |
|        - | 14377 | ` *       finally block.` |
|        - | 14378 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 14379 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 14380 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 14381 | ` *       in pPendingException (step 2c).` |
|        - | 14382 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 14383 | ` *    d. Run finally (if present).` |
|        - | 14384 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 14385 | ` *       that handlers are restored and finally has run.` |
|        - | 14386 | ` */` |
|      926 | 14387 | `static sxi32 VmThrowException(` |
|        - | 14388 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 14389 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 14390 | `	)` |
|        5 | 14391 |  |
|        - | 14392 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 14393 | `	ph7_exception **apException;` |
|        - | 14394 | `	ph7_exception *pException;` |
|        - | 14395 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 14396 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 14397 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      931 | 14398 | `	VmCoalesceDisarm(pVm);` |
|        - | 14399 | `	/* A fresh throw supersedes any pending catch/finally return (PHP: an` |
|        - | 14400 | ``	 * exception thrown in a catch/finally discards an earlier `return`). */`` |
|      931 | 14401 | `	if( pVm->bReturnRequested ){` |
|      ! 0 | 14402 | `		pVm->bReturnRequested = 0;` |
|      ! 0 | 14403 | `		PH7_MemObjRelease(&pVm->sCatchReturn);` |
|      ! 0 | 14404 | `	}` |
|        - | 14405 | `	/* Point to the stack of loaded exceptions */` |
|      931 | 14406 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      931 | 14407 | `	pException = 0;` |
|      931 | 14408 | `	pCatch = 0;` |
|      931 | 14409 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14410 | `		ph7_exception_block *aCatch;` |
|        - | 14411 | `		ph7_class *pClass;` |
|        - | 14412 | `		SyString *aNames;` |
|        - | 14413 | `		sxu32 nNames;` |
|        - | 14414 | `		int matched;` |
|        - | 14415 | `		sxu32 j,k;` |
|        - | 14416 | `		/* Locate the appropriate block to execute */` |
|      409 | 14417 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      409 | 14418 | `		(void)SySetPop(&pVm->aException);` |
|      409 | 14419 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      417 | 14420 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 14421 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      415 | 14422 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      415 | 14423 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      415 | 14424 | `			matched = 0;` |
|      441 | 14425 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 14426 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 14427 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 14428 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      433 | 14429 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      433 | 14430 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 14431 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 14432 | `					continue;` |
|        - | 14433 | `				}` |
|      433 | 14434 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      407 | 14435 | `					matched = 1;` |
|      407 | 14436 | `					break;` |
|        - | 14437 | `				}` |
|       14 | 14438 | `			}` |
|      415 | 14439 | `			if( matched ){` |
|        - | 14440 | `				/* Catch block found,break immediately */` |
|      407 | 14441 | `				pCatch = &aCatch[j];` |
|      407 | 14442 | `				break;` |
|        - | 14443 | `			}` |
|        5 | 14444 | `		}` |
|      202 | 14445 | `	}` |
|        - | 14446 | `	/* Execute the cached block if available */` |
|      931 | 14447 | `	if( pCatch == 0 ){` |
|        - | 14448 | `		sxi32 rc;` |
|        - | 14449 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      529 | 14450 | `		if( pException && pException->iHasFinally ){` |
|        3 | 14451 | `			pException->iFinallyDone = 1;` |
|        3 | 14452 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|        3 | 14453 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14454 | `				return SXERR_ABORT;` |
|        - | 14455 | `			}` |
|        1 | 14456 | `		}` |
|        - | 14457 | `		/* Check if there is an outer exception handler on the stack */` |
|      529 | 14458 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14459 | `			/* Re-throw to the outer handler */` |
|        3 | 14460 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14461 | `		}` |
|        - | 14462 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14463 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14464 | `		 * exception instead of reporting it uncaught.` |
|        - | 14465 | `		 */` |
|      527 | 14466 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14467 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14468 | `			 * by looking for a catch frame on the stack.` |
|        - | 14469 | `			 */` |
|      527 | 14470 | `			VmFrame *pF = pVm->pFrame;` |
|      527 | 14471 | `			int inCatch = 0;` |
|     1055 | 14472 | `			while( pF ){` |
|      543 | 14473 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|       11 | 14474 | `					inCatch = 1;` |
|       11 | 14475 | `					break;` |
|        - | 14476 | `				}` |
|      532 | 14477 | `				pF = pF->pParent;` |
|        4 | 14478 | `			}` |
|      527 | 14479 | `			if( inCatch ){` |
|        - | 14480 | `				/* Defer — will be re-thrown after finally runs */` |
|       11 | 14481 | `				pThis->iRef++;` |
|       11 | 14482 | `				pVm->pPendingException = pThis;` |
|       11 | 14483 | `				return SXRET_OK;` |
|        - | 14484 | `			}` |
|      256 | 14485 | `		}` |
|        - | 14486 | `		/* Truly uncaught */` |
|      516 | 14487 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      516 | 14488 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14489 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14490 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14491 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14492 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14493 | `			}` |
|      ! 0 | 14494 | `		}` |
|      516 | 14495 | `		return rc;` |
|      ! 0 | 14496 | `	}else{` |
|      407 | 14497 | `		VmFrame *pFrame = pVm->pFrame;` |
|      407 | 14498 | `		ph7_exception **apSaved = 0;` |
|        - | 14499 | `		sxu32 nSavedCount;` |
|        - | 14500 | `		sxi32 rc;` |
|      407 | 14501 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      407 | 14502 | `		if( pException->pFrame == pFrame ){` |
|      291 | 14503 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      143 | 14504 | `		}` |
|        - | 14505 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14506 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14507 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14508 | `		 */` |
|      407 | 14509 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      407 | 14510 | `		if( nSavedCount > 0 ){` |
|       22 | 14511 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        7 | 14512 | `				nSavedCount * sizeof(ph7_exception *));` |
|       15 | 14513 | `			if( apSaved ){` |
|       22 | 14514 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        7 | 14515 | `					nSavedCount * sizeof(ph7_exception *));` |
|       15 | 14516 | `				SySetReset(&pVm->aException);` |
|        7 | 14517 | `			}` |
|        7 | 14518 | `		}` |
|        - | 14519 | `		/* Create the catch frame (made transparent below) */` |
|      407 | 14520 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      407 | 14521 | `		if( rc == SXRET_OK ){` |
|        - | 14522 | `			ph7_value *pObj;` |
|        - | 14523 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 14524 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 14525 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 14526 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 14527 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 14528 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 14529 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 14530 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|      407 | 14531 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|      407 | 14532 | `			pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      407 | 14533 | `			if( pObj ){` |
|        - | 14534 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 14535 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 14536 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 14537 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 14538 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 14539 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|      407 | 14540 | `				pThis->iRef++;` |
|      407 | 14541 | `				PH7_MemObjRelease(pObj);` |
|      407 | 14542 | `				pObj->x.pOther = pThis;` |
|      407 | 14543 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      201 | 14544 | `			}` |
|        - | 14545 | `			/* Execute the catch block */` |
|      407 | 14546 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0,TRUE);` |
|        - | 14547 | `			/* Leave the frame */` |
|      407 | 14548 | `			VmLeaveFrame(&(*pVm));` |
|      201 | 14549 | `		}` |
|        - | 14550 | `		/* Restore the outer exception handlers */` |
|      407 | 14551 | `		if( apSaved ){` |
|        - | 14552 | `			sxu32 k;` |
|        - | 14553 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14554 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14555 | `			 * Restore the original outer entries.` |
|        - | 14556 | `			 */` |
|       15 | 14557 | `			SySetReset(&pVm->aException);` |
|       29 | 14558 | `			for(k = 0; k < nSavedCount; k++){` |
|       15 | 14559 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        8 | 14560 | `			}` |
|       15 | 14561 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        7 | 14562 | `		}` |
|        - | 14563 | `		/* Execute the finally block after catch */` |
|      407 | 14564 | `		if( pException->iHasFinally ){` |
|       25 | 14565 | `			pException->iFinallyDone = 1;` |
|        - | 14566 | `			{` |
|       25 | 14567 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0,TRUE);` |
|       25 | 14568 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14569 | `					return SXERR_ABORT;` |
|        - | 14570 | `				}` |
|        - | 14571 | `			}` |
|       11 | 14572 | `		}` |
|      407 | 14573 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14574 | `			return SXERR_ABORT;` |
|        - | 14575 | `		}` |
|        - | 14576 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14577 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14578 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 14579 | ``		 * unless the finally itself issued a `return`, which swallows the`` |
|        - | 14580 | `		 * in-flight exception (PHP semantics).` |
|        - | 14581 | `		 */` |
|      407 | 14582 | `		if( pVm->pPendingException ){` |
|       11 | 14583 | `			if( !pVm->bReturnRequested ){` |
|        9 | 14584 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14585 | `				pVm->pPendingException = 0;` |
|        9 | 14586 | `				return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14587 | `			}` |
|        - | 14588 | `			/* Swallowed by finally's return: drop the deferred exception. */` |
|        3 | 14589 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        3 | 14590 | `			pVm->pPendingException = 0;` |
|        1 | 14591 | `		}` |
|        - | 14592 | `	}` |
|        - | 14593 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14594 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14595 | `	 */` |
|      399 | 14596 | `	return SXRET_OK;` |
|      468 | 14597 |  |
|        - | 14598 | `/*` |
|        - | 14599 | ` * Section:` |
|        - | 14600 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14601 | ` * Status:` |
|        - | 14602 | ` *    Stable.` |
|        - | 14603 | ` */` |
|        - | 14604 | `/*` |
|        - | 14605 | ` * string ph7version(void)` |
|        - | 14606 | ` *  Returns the running version of the PH7 version.` |
|        - | 14607 | ` * Parameters` |
|        - | 14608 | ` *  None` |
|        - | 14609 | ` * Return` |
|        - | 14610 | ` * Current PH7 version.` |
|        - | 14611 | ` */` |
|        2 | 14612 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14613 |  |
|        1 | 14614 | `	SXUNUSED(nArg);` |
|        1 | 14615 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14616 | `	/* Current engine version */` |
|        3 | 14617 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14618 | `	return PH7_OK;` |
|        1 | 14619 |  |
|        - | 14620 | `/*` |
|        - | 14621 | ` * string phpversion([ string $extension ])` |
|        - | 14622 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14623 | ` * Parameters` |
|        - | 14624 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14625 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14626 | ` * Return` |
|        - | 14627 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14628 | ` */` |
|        4 | 14629 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14630 |  |
|        2 | 14631 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14632 | `	if( nArg > 0 ){` |
|      ! 0 | 14633 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14634 | `		return PH7_OK;` |
|        - | 14635 | `	}` |
|        5 | 14636 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14637 | `	return PH7_OK;` |
|        3 | 14638 |  |
|        - | 14639 | `/*` |
|        - | 14640 | ` * string php_sapi_name(void)` |
|        - | 14641 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14642 | ` * Parameters` |
|        - | 14643 | ` *  None` |
|        - | 14644 | ` * Return` |
|        - | 14645 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14646 | ` */` |
|        2 | 14647 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14648 |  |
|        3 | 14649 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14650 | `	SXUNUSED(nArg);` |
|        1 | 14651 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14652 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14653 | `	return PH7_OK;` |
|        1 | 14654 |  |
|        - | 14655 | `/*` |
|        - | 14656 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14657 | ` */` |
|        - | 14658 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14659 | ` "<html><head>"\` |
|        - | 14660 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14661 | ` "<style type=\"text/css\">"\` |
|        - | 14662 | ` "div {"\` |
|        - | 14663 | `     "border: 1px solid #cccccc;"\` |
|        - | 14664 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14665 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14666 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14667 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14668 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14669 | `     "-o-border-radius: 10px;"\` |
|        - | 14670 | `     "border-radius: 10px;"\` |
|        - | 14671 | `     "padding-left: 2em;"\` |
|        - | 14672 | `     "background-color: white;"\` |
|        - | 14673 | `     "margin-left: auto;"\` |
|        - | 14674 | `     "font-family: verdana;"\` |
|        - | 14675 | `     "padding-right: 2em;"\` |
|        - | 14676 | `     "margin-right: auto;"\` |
|        - | 14677 | `     "}"\` |
|        - | 14678 | `     "body {"\` |
|        - | 14679 | `     "padding: 0.2em;"\` |
|        - | 14680 | `     "font-style: normal;"\` |
|        - | 14681 | `     "font-size: medium;"\` |
|        - | 14682 | `     "background-color: #f2f2f2;"\` |
|        - | 14683 | `     "}"\` |
|        - | 14684 | `     "hr {"\` |
|        - | 14685 | `     "border-style: solid none none;"\` |
|        - | 14686 | `     "border-width: 1px medium medium;"\` |
|        - | 14687 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14688 | `     "height: 1px;"\` |
|        - | 14689 | `     "}"\` |
|        - | 14690 | `     "a {"\` |
|        - | 14691 | `     "color: #3366cc;"\` |
|        - | 14692 | `     "text-decoration: none;"\` |
|        - | 14693 | `     "}"\` |
|        - | 14694 | `     "a:hover {"\` |
|        - | 14695 | `     "color: #999999;"\` |
|        - | 14696 | `     "}"\` |
|        - | 14697 | `     "a:active {"\` |
|        - | 14698 | `     "color: #663399;"\` |
|        - | 14699 | `     "}"\` |
|        - | 14700 | `     "h1 {"\` |
|        - | 14701 | `     "margin: 0;"\` |
|        - | 14702 | `     "padding: 0;"\` |
|        - | 14703 | `     "font-family: Verdana;"\` |
|        - | 14704 | `     "font-weight: bold;"\` |
|        - | 14705 | `     "font-style: normal;"\` |
|        - | 14706 | `     "font-size: medium;"\` |
|        - | 14707 | `     "text-transform: capitalize;"\` |
|        - | 14708 | `     "color: #0a328c;"\` |
|        - | 14709 | `     "}"\` |
|        - | 14710 | `     "p {"\` |
|        - | 14711 | `     "margin: 0 auto;"\` |
|        - | 14712 | `     "font-size: medium;"\` |
|        - | 14713 | `     "font-style: normal;"\` |
|        - | 14714 | `     "font-family: verdana;"\` |
|        - | 14715 | `     "}"\` |
|        - | 14716 | `"</style></head><body>"\` |
|        - | 14717 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14718 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14719 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14720 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14721 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14722 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14723 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14724 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14725 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14726 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14727 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14728 |  |
|        - | 14729 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14730 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14731 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14732 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14733 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14734 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14735 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14736 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14737 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14738 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14739 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14740 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14741 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14742 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14743 |  |
|        - | 14744 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14745 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14746 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14747 | `"&nbsp;*<br>"\` |
|        - | 14748 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14749 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14750 | `"&nbsp;* are met:<br>"\` |
|        - | 14751 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14752 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14753 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14754 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14755 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14756 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14757 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14758 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14759 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14760 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14761 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14762 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14763 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14764 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14765 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14766 | `"&nbsp;*<br>"\` |
|        - | 14767 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14768 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14769 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14770 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14771 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14772 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14773 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14774 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14775 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14776 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14777 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14778 | `"&nbsp;*/<br>"\` |
|        - | 14779 | `"</span></small></small></p>"\` |
|        - | 14780 | `"</div></body></html>"` |
|        - | 14781 | `/*` |
|        - | 14782 | ` * bool ph7credits(void)` |
|        - | 14783 | ` * bool ph7info(void)` |
|        - | 14784 | ` * bool ph7copyright(void)` |
|        - | 14785 | ` *  Prints out the credits for PH7 engine` |
|        - | 14786 | ` * Parameters` |
|        - | 14787 | ` *  None` |
|        - | 14788 | ` * Return` |
|        - | 14789 | ` *  Always TRUE` |
|        - | 14790 | ` */` |
|        2 | 14791 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14792 |  |
|        3 | 14793 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14794 | `	/* Expand the HTML page above*/` |
|        3 | 14795 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14796 | `	ph7_context_output_format(` |
|        1 | 14797 | `		pCtx,` |
|        - | 14798 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14799 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14800 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14801 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14802 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14803 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14804 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14805 | `#ifdef __WINNT__` |
|        - | 14806 | `		"Windows NT"` |
|        - | 14807 | `#elif defined(__UNIXES__)` |
|        - | 14808 | `		"UNIX-Like"` |
|        - | 14809 | `#else` |
|        - | 14810 | `		"Other OS"` |
|        - | 14811 | `#endif` |
|        - | 14812 | `		);` |
|        3 | 14813 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14814 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14815 | `	SXUNUSED(apArg);` |
|        - | 14816 | `	/* Return TRUE */` |
|        - | 14817 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14818 | `	return PH7_OK;` |
|        1 | 14819 |  |
|        - | 14820 | `/*` |
|        - | 14821 | ` * Section:` |
|        - | 14822 | ` *    URL related routines.` |
|        - | 14823 | ` * Status:` |
|        - | 14824 | ` *    Stable.` |
|        - | 14825 | ` */` |
|        - | 14826 | `/*` |
|        - | 14827 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14828 | ` *  Parse a URL and return its fields.` |
|        - | 14829 | ` * Parameters` |
|        - | 14830 | ` *  $url` |
|        - | 14831 | ` *   The URL to parse.` |
|        - | 14832 | ` * $component` |
|        - | 14833 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14834 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14835 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14836 | ` *  in which case the return value will be an integer).` |
|        - | 14837 | ` * Return` |
|        - | 14838 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14839 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14840 | ` *  this array are:` |
|        - | 14841 | ` *   scheme - e.g. http` |
|        - | 14842 | ` *   host` |
|        - | 14843 | ` *   port` |
|        - | 14844 | ` *   user` |
|        - | 14845 | ` *   pass` |
|        - | 14846 | ` *   path` |
|        - | 14847 | ` *   query - after the question mark ?` |
|        - | 14848 | ` *   fragment - after the hashmark #` |
|        - | 14849 | ` * Note:` |
|        - | 14850 | ` *  FALSE is returned on failure.` |
|        - | 14851 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14852 | ` *  with the standard PHP engine.` |
|        - | 14853 | ` */` |
|       28 | 14854 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14855 |  |
|        - | 14856 | `	const char *zStr; /* Input string */` |
|        - | 14857 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14858 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14859 | `	int nLen;` |
|        - | 14860 | `	sxi32 rc;` |
|       29 | 14861 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14862 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14863 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14864 | `		return PH7_OK;` |
|        - | 14865 | `	}` |
|        - | 14866 | `	/* Extract the given URI */` |
|       29 | 14867 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14868 | `	if( nLen < 1 ){` |
|        - | 14869 | `		/* Nothing to process,return FALSE */` |
|        3 | 14870 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14871 | `		return PH7_OK;` |
|        - | 14872 | `	}` |
|        - | 14873 | `	/* Get a parse */` |
|       27 | 14874 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14875 | `	if( rc != SXRET_OK ){` |
|        - | 14876 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14877 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14878 | `		return PH7_OK;` |
|        - | 14879 | `	}` |
|       27 | 14880 | `	if( nArg > 1 ){` |
|      ! 0 | 14881 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14882 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14883 | `		switch(nComponent){` |
|      ! 0 | 14884 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14885 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14886 | `			if( pComp->nByte < 1 ){` |
|        - | 14887 | `				/* No available value,return NULL */` |
|      ! 0 | 14888 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14889 | `			}else{` |
|      ! 0 | 14890 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14891 | `			}` |
|      ! 0 | 14892 | `			break;` |
|      ! 0 | 14893 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14894 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14895 | `			if( pComp->nByte < 1 ){` |
|        - | 14896 | `				/* No available value,return NULL */` |
|      ! 0 | 14897 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14898 | `			}else{` |
|      ! 0 | 14899 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14900 | `			}` |
|      ! 0 | 14901 | `			break;` |
|      ! 0 | 14902 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14903 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14904 | `			if( pComp->nByte < 1 ){` |
|        - | 14905 | `				/* No available value,return NULL */` |
|      ! 0 | 14906 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14907 | `			}else{` |
|      ! 0 | 14908 | `				int iPort = 0;` |
|        - | 14909 | `				/* Cast the value to integer */` |
|      ! 0 | 14910 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14911 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14912 | `			}` |
|      ! 0 | 14913 | `			break;` |
|      ! 0 | 14914 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14915 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14916 | `			if( pComp->nByte < 1 ){` |
|        - | 14917 | `				/* No available value,return NULL */` |
|      ! 0 | 14918 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14919 | `			}else{` |
|      ! 0 | 14920 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14921 | `			}` |
|      ! 0 | 14922 | `			break;` |
|      ! 0 | 14923 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14924 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14925 | `			if( pComp->nByte < 1 ){` |
|        - | 14926 | `				/* No available value,return NULL */` |
|      ! 0 | 14927 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14928 | `			}else{` |
|      ! 0 | 14929 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14930 | `			}` |
|      ! 0 | 14931 | `			break;` |
|      ! 0 | 14932 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14933 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14934 | `			if( pComp->nByte < 1 ){` |
|        - | 14935 | `				/* No available value,return NULL */` |
|      ! 0 | 14936 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14937 | `			}else{` |
|      ! 0 | 14938 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14939 | `			}` |
|      ! 0 | 14940 | `			break;` |
|      ! 0 | 14941 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14942 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14943 | `			if( pComp->nByte < 1 ){` |
|        - | 14944 | `				/* No available value,return NULL */` |
|      ! 0 | 14945 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14946 | `			}else{` |
|      ! 0 | 14947 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14948 | `			}` |
|      ! 0 | 14949 | `			break;` |
|      ! 0 | 14950 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14951 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14952 | `			if( pComp->nByte < 1 ){` |
|        - | 14953 | `				/* No available value,return NULL */` |
|      ! 0 | 14954 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14955 | `			}else{` |
|      ! 0 | 14956 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14957 | `			}` |
|      ! 0 | 14958 | `			break;` |
|      ! 0 | 14959 | `		default:` |
|        - | 14960 | `			/* No such entry,return NULL */` |
|      ! 0 | 14961 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14962 | `			break;` |
|        - | 14963 | `		}` |
|      ! 0 | 14964 | `	}else{` |
|        - | 14965 | `		ph7_value *pArray,*pValue;` |
|        - | 14966 | `		/* Return an associative array */` |
|       27 | 14967 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14968 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14969 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14970 | `			/* Out of memory */` |
|      ! 0 | 14971 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14972 | `			/* Return false */` |
|      ! 0 | 14973 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14974 | `			return PH7_OK;` |
|        - | 14975 | `		}` |
|        - | 14976 | `		/* Fill the array */` |
|       27 | 14977 | `		pComp = &sURI.sScheme;` |
|       27 | 14978 | `		if( pComp->nByte > 0 ){` |
|       19 | 14979 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14980 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14981 | `		}` |
|        - | 14982 | `		/* Reset the string cursor */` |
|       27 | 14983 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14984 | `		pComp = &sURI.sHost;` |
|       27 | 14985 | `		if( pComp->nByte > 0 ){` |
|       25 | 14986 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14987 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14988 | `		}` |
|        - | 14989 | `		/* Reset the string cursor */` |
|       27 | 14990 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14991 | `		pComp = &sURI.sPort;` |
|       27 | 14992 | `		if( pComp->nByte > 0 ){` |
|       11 | 14993 | `			int iPort = 0;/* cc warning */` |
|        - | 14994 | `			/* Convert to integer */` |
|       11 | 14995 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14996 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14997 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14998 | `		}` |
|        - | 14999 | `		/* Reset the string cursor */` |
|       27 | 15000 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15001 | `		pComp = &sURI.sUser;` |
|       27 | 15002 | `		if( pComp->nByte > 0 ){` |
|        7 | 15003 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 15004 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 15005 | `		}` |
|        - | 15006 | `		/* Reset the string cursor */` |
|       27 | 15007 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15008 | `		pComp = &sURI.sPass;` |
|       27 | 15009 | `		if( pComp->nByte > 0 ){` |
|        7 | 15010 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 15011 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 15012 | `		}` |
|        - | 15013 | `		/* Reset the string cursor */` |
|       27 | 15014 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15015 | `		pComp = &sURI.sPath;` |
|       27 | 15016 | `		if( pComp->nByte > 0 ){` |
|       17 | 15017 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 15018 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 15019 | `		}` |
|        - | 15020 | `		/* Reset the string cursor */` |
|       27 | 15021 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15022 | `		pComp = &sURI.sQuery;` |
|       27 | 15023 | `		if( pComp->nByte > 0 ){` |
|        5 | 15024 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 15025 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 15026 | `		}` |
|        - | 15027 | `		/* Reset the string cursor */` |
|       27 | 15028 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 15029 | `		pComp = &sURI.sFragment;` |
|       27 | 15030 | `		if( pComp->nByte > 0 ){` |
|        5 | 15031 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 15032 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 15033 | `		}` |
|        - | 15034 | `		/* Return the created array */` |
|       27 | 15035 | `		ph7_result_value(pCtx,pArray);` |
|        - | 15036 | `		/* NOTE:` |
|        - | 15037 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 15038 | `		 * automatically as soon we return from this function.` |
|        - | 15039 | `		 */` |
|        - | 15040 | `	}` |
|        - | 15041 | `	/* All done */` |
|       27 | 15042 | `	return PH7_OK;` |
|       15 | 15043 |  |
|        - | 15044 | `/*` |
|        - | 15045 | ` * Section:` |
|        - | 15046 | ` *   Array related routines.` |
|        - | 15047 | ` * Status:` |
|        - | 15048 | ` *    Stable.` |
|        - | 15049 | ` * Note 2012-5-21 01:04:15:` |
|        - | 15050 | ` *  Array related functions that need access to the underlying` |
|        - | 15051 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 15052 | ` */` |
|        - | 15053 | `/*` |
|        - | 15054 | ` * The [compact()] function store it's state information in an instance` |
|        - | 15055 | ` * of the following structure.` |
|        - | 15056 | ` */` |
|        - | 15057 | `struct compact_data` |
|        - | 15058 |  |
|        - | 15059 | `	ph7_value *pArray;  /* Target array */` |
|        - | 15060 | `	int nRecCount;      /* Recursion count */` |
|        - | 15061 | `};` |
|        - | 15062 | `/*` |
|        - | 15063 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 15064 | ` */` |
|      ! 0 | 15065 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 15066 |  |
|      ! 0 | 15067 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 15068 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 15069 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 15070 | `	/* Act according to the hashmap value */` |
|      ! 0 | 15071 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 15072 | `		SyString sVar;` |
|      ! 0 | 15073 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 15074 | `		if( sVar.nByte > 0 ){` |
|        - | 15075 | `			/* Query the current frame */` |
|      ! 0 | 15076 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 15077 | `			/* ^` |
|        - | 15078 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 15079 | `			 */` |
|      ! 0 | 15080 | `			if( pKey ){` |
|        - | 15081 | `				/* Perform the insertion */` |
|      ! 0 | 15082 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 15083 | `			}` |
|      ! 0 | 15084 | `		}` |
|      ! 0 | 15085 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 15086 | `		int rc;` |
|        - | 15087 | `		/* Recursively traverse this array */` |
|      ! 0 | 15088 | `		pData->nRecCount++;` |
|      ! 0 | 15089 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 15090 | `		pData->nRecCount--;` |
|      ! 0 | 15091 | `		return rc;` |
|        - | 15092 | `	}` |
|      ! 0 | 15093 | `	return SXRET_OK;` |
|      ! 0 | 15094 |  |
|        - | 15095 | `/*` |
|        - | 15096 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 15097 | ` *  Create array containing variables and their values.` |
|        - | 15098 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 15099 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 15100 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 15101 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 15102 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 15103 | ` * Parameters` |
|        - | 15104 | ` *  $varname` |
|        - | 15105 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 15106 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 15107 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 15108 | ` *   it recursively.` |
|        - | 15109 | ` * Return` |
|        - | 15110 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 15111 | ` */` |
|        2 | 15112 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15113 |  |
|        - | 15114 | `	ph7_value *pArray,*pObj;` |
|        3 | 15115 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15116 | `	const char *zName;` |
|        - | 15117 | `	SyString sVar;` |
|        - | 15118 | `	int i,nLen;` |
|        3 | 15119 | `	if( nArg < 1 ){` |
|        - | 15120 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 15121 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15122 | `		return PH7_OK;` |
|        - | 15123 | `	}` |
|        - | 15124 | `	/* Create the array */` |
|        3 | 15125 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 15126 | `	if( pArray == 0 ){` |
|        - | 15127 | `		/* Out of memory */` |
|      ! 0 | 15128 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 15129 | `		/* Return NULL */` |
|      ! 0 | 15130 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15131 | `		return PH7_OK;` |
|        - | 15132 | `	}` |
|        - | 15133 | `	/* Perform the requested operation */` |
|        7 | 15134 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 15135 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 15136 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 15137 | `				struct compact_data sData;` |
|      ! 0 | 15138 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 15139 | `				/* Recursively walk the array */` |
|      ! 0 | 15140 | `				sData.nRecCount = 0;` |
|      ! 0 | 15141 | `				sData.pArray = pArray;` |
|      ! 0 | 15142 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 15143 | `			}` |
|      ! 0 | 15144 | `		}else{` |
|        - | 15145 | `			/* Extract variable name */` |
|        5 | 15146 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 15147 | `			if( nLen > 0 ){` |
|        5 | 15148 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 15149 | `				/* Check if the variable is available in the current frame */` |
|        5 | 15150 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 15151 | `				if( pObj ){` |
|        5 | 15152 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 15153 | `				}` |
|        2 | 15154 | `			}` |
|        - | 15155 | `		}` |
|        3 | 15156 | `	}` |
|        - | 15157 | `	/* Return the array */` |
|        3 | 15158 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 15159 | `	return PH7_OK;` |
|        2 | 15160 |  |
|        - | 15161 | `/*` |
|        - | 15162 | ` * The [extract()] function store it's state information in an instance` |
|        - | 15163 | ` * of the following structure.` |
|        - | 15164 | ` */` |
|        - | 15165 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 15166 | `struct extract_aux_data` |
|        - | 15167 |  |
|        - | 15168 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 15169 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 15170 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 15171 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 15172 | `	int iFlags;           /* Control flags */` |
|        - | 15173 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 15174 | `};` |
|        - | 15175 | `/* Forward declaration */` |
|        - | 15176 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 15177 | `/*` |
|        - | 15178 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 15179 | ` *   Import variables into the current symbol table from an array.` |
|        - | 15180 | ` * Parameters` |
|        - | 15181 | ` * $var_array` |
|        - | 15182 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 15183 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 15184 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 15185 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 15186 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 15187 | ` * $extract_type` |
|        - | 15188 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 15189 | ` *  It can be one of the following values:` |
|        - | 15190 | ` *   EXTR_OVERWRITE` |
|        - | 15191 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 15192 | ` *   EXTR_SKIP` |
|        - | 15193 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 15194 | ` *   EXTR_PREFIX_SAME` |
|        - | 15195 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 15196 | ` *   EXTR_PREFIX_ALL` |
|        - | 15197 | ` *       Prefix all variable names with prefix.` |
|        - | 15198 | ` *   EXTR_PREFIX_INVALID` |
|        - | 15199 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 15200 | ` *   EXTR_IF_EXISTS` |
|        - | 15201 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 15202 | ` *       otherwise do nothing.` |
|        - | 15203 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 15204 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 15205 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 15206 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 15207 | ` *      the current symbol table.` |
|        - | 15208 | ` * $prefix` |
|        - | 15209 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 15210 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 15211 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 15212 | ` *  underscore character.` |
|        - | 15213 | ` * Return` |
|        - | 15214 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 15215 | ` */` |
|        4 | 15216 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15217 |  |
|        - | 15218 | `	extract_aux_data sAux;` |
|        - | 15219 | `	ph7_hashmap *pMap;` |
|        5 | 15220 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 15221 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 15222 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15223 | `		return PH7_OK;` |
|        - | 15224 | `	}` |
|        - | 15225 | `	/* Point to the target hashmap */` |
|        5 | 15226 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 15227 | `	if( pMap->nEntry < 1 ){` |
|        - | 15228 | `		/* Empty map,return  0 */` |
|      ! 0 | 15229 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 15230 | `		return PH7_OK;` |
|        - | 15231 | `	}` |
|        - | 15232 | `	/* Prepare the aux data */` |
|        5 | 15233 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 15234 | `	if( nArg > 1 ){` |
|        3 | 15235 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 15236 | `		if( nArg > 2 ){` |
|      ! 0 | 15237 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 15238 | `		}` |
|        1 | 15239 | `	}` |
|        5 | 15240 | `	sAux.pVm = pCtx->pVm;` |
|        - | 15241 | `	/* Invoke the worker callback */` |
|        5 | 15242 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 15243 | `	/* Number of variables successfully imported */` |
|        5 | 15244 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 15245 | `	return PH7_OK;` |
|        3 | 15246 |  |
|        - | 15247 | `/*` |
|        - | 15248 | ` * Worker callback for the [extract()] function defined` |
|        - | 15249 | ` * below.` |
|        - | 15250 | ` */` |
|        8 | 15251 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15252 |  |
|        9 | 15253 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 15254 | `	int iFlags = pAux->iFlags;` |
|        9 | 15255 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15256 | `	ph7_value *pObj;` |
|        - | 15257 | `	SyString sVar;` |
|        9 | 15258 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 15259 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 15260 | `	}` |
|        - | 15261 | `	/* Perform a string cast */` |
|        9 | 15262 | `	PH7_MemObjToString(pKey);` |
|        9 | 15263 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15264 | `		/* Unavailable variable name */` |
|      ! 0 | 15265 | `		return SXRET_OK;` |
|        - | 15266 | `	}` |
|        9 | 15267 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 15268 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 15269 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15270 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15271 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15272 | `			);` |
|      ! 0 | 15273 | `	}else{` |
|       13 | 15274 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 15275 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15276 | `	}` |
|        9 | 15277 | `	sVar.zString = pAux->zWorker;` |
|        - | 15278 | `	/* Try to extract the variable */` |
|        9 | 15279 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 15280 | `	if( pObj ){` |
|        - | 15281 | `		/* Collision */` |
|        5 | 15282 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 15283 | `			return SXRET_OK;` |
|        - | 15284 | `		}` |
|        5 | 15285 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 15286 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 15287 | `				/* Already prefixed */` |
|      ! 0 | 15288 | `				return SXRET_OK;` |
|        - | 15289 | `			}` |
|      ! 0 | 15290 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 15291 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 15292 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15293 | `				);` |
|      ! 0 | 15294 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 15295 | `		}` |
|        3 | 15296 | `	}else{` |
|        - | 15297 | `		/* Create the variable */` |
|        5 | 15298 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 15299 | `	}` |
|        9 | 15300 | `	if( pObj ){` |
|        - | 15301 | `		/* Overwrite the old value */` |
|        9 | 15302 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 15303 | `		/* Increment counter */` |
|        9 | 15304 | `		pAux->iCount++;` |
|        4 | 15305 | `	}` |
|        9 | 15306 | `	return SXRET_OK;` |
|        5 | 15307 |  |
|        - | 15308 | `/*` |
|        - | 15309 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 15310 | ` * defined below.` |
|        - | 15311 | ` */` |
|        2 | 15312 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 15313 |  |
|        3 | 15314 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 15315 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 15316 | `	ph7_value *pObj;` |
|        - | 15317 | `	SyString sVar;` |
|        - | 15318 | `	/* Perform a string cast */` |
|        3 | 15319 | `	PH7_MemObjToString(pKey);` |
|        3 | 15320 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 15321 | `		/* Unavailable variable name */` |
|      ! 0 | 15322 | `		return SXRET_OK;` |
|        - | 15323 | `	}` |
|        3 | 15324 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 15325 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 15326 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 15327 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 15328 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 15329 | `			);` |
|        2 | 15330 | `	}else{` |
|      ! 0 | 15331 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 15332 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 15333 | `	}` |
|        3 | 15334 | `	sVar.zString = pAux->zWorker;` |
|        - | 15335 | `	/* Extract the variable */` |
|        3 | 15336 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 15337 | `	if( pObj ){` |
|        3 | 15338 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 15339 | `	}` |
|        3 | 15340 | `	return SXRET_OK;` |
|        2 | 15341 |  |
|        - | 15342 | `/*` |
|        - | 15343 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 15344 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 15345 | ` * Parameters` |
|        - | 15346 | ` * $types` |
|        - | 15347 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 15348 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 15349 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 15350 | ` *  POST includes the POST uploaded file information.` |
|        - | 15351 | ` *  Note:` |
|        - | 15352 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 15353 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 15354 | ` * $prefix` |
|        - | 15355 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 15356 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 15357 | ` *  variable named $pref_userid.` |
|        - | 15358 | ` * Return` |
|        - | 15359 | ` *  TRUE on success or FALSE on failure.` |
|        - | 15360 | ` */` |
|        2 | 15361 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15362 |  |
|        - | 15363 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 15364 | `	extract_aux_data sAux;` |
|        - | 15365 | `	int nLen,nPrefixLen;` |
|        - | 15366 | `	ph7_value *pSuper;` |
|        - | 15367 | `	ph7_vm *pVm;` |
|        - | 15368 | `	/* By default import only $_GET variables  */` |
|        3 | 15369 | `	zImport = "G";` |
|        3 | 15370 | `	nLen = (int)sizeof(char);` |
|        3 | 15371 | `	zPrefix = 0;` |
|        3 | 15372 | `	nPrefixLen = 0;` |
|        3 | 15373 | `	if( nArg > 0 ){` |
|        3 | 15374 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 15375 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 15376 | `		}` |
|        3 | 15377 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 15378 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 15379 | `		}` |
|        1 | 15380 | `	}` |
|        - | 15381 | `	/* Point to the underlying VM */` |
|        3 | 15382 | `	pVm = pCtx->pVm;` |
|        - | 15383 | `	/* Initialize the aux data */` |
|        3 | 15384 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 15385 | `	sAux.zPrefix = zPrefix;` |
|        3 | 15386 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 15387 | `	sAux.pVm = pVm;` |
|        - | 15388 | `	/* Extract */` |
|        3 | 15389 | `	zEnd = &zImport[nLen];` |
|        5 | 15390 | `	while( zImport < zEnd ){` |
|        3 | 15391 | `		int c = zImport[0];` |
|        3 | 15392 | `		pSuper = 0;` |
|        3 | 15393 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 15394 | `			/* Import $_GET variables */` |
|        3 | 15395 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 15396 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 15397 | `			/* Import $_POST variables */` |
|      ! 0 | 15398 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 15399 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 15400 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 15401 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 15402 | `		}` |
|        3 | 15403 | `		if( pSuper ){` |
|        - | 15404 | `			/* Iterate throw array entries */` |
|        3 | 15405 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 15406 | `		}` |
|        - | 15407 | `		/* Advance the cursor */` |
|        3 | 15408 | `		zImport++;` |
|        1 | 15409 | `	}` |
|        - | 15410 | `	/* All done,return TRUE*/` |
|        3 | 15411 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15412 | `	return PH7_OK;` |
|        1 | 15413 |  |
|        - | 15414 | `/*` |
|        - | 15415 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 15416 | ` * Refer to the eval() language construct implementation for more` |
|        - | 15417 | ` * information.` |
|        - | 15418 | ` */` |
|    12914 | 15419 | `static sxi32 VmEvalChunk(` |
|        - | 15420 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 15421 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 15422 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 15423 | `	int iFlags,         /* Compile flag */` |
|        - | 15424 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 15425 | `	)` |
|        5 | 15426 |  |
|        - | 15427 | `	SySet *pByteCode,aByteCode;` |
|        - | 15428 | `	SyBlob sSavedNs;` |
|    12919 | 15429 | `	ProcConsumer xErr = 0;` |
|    12919 | 15430 | `	void *pErrData = 0;` |
|        - | 15431 | `	/* Initialize bytecode container */` |
|    12919 | 15432 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12919 | 15433 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 15434 | `	/* Reset the code generator */` |
|    12919 | 15435 | `	if( bTrueReturn ){` |
|        - | 15436 | `		/* Included file,log compile-time errors */` |
|     9714 | 15437 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9714 | 15438 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4855 | 15439 | `	}` |
|    12919 | 15440 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 15441 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 15442 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 15443 | `	 * the caller's namespace is restored. */` |
|    12919 | 15444 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12919 | 15445 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12919 | 15446 | `	if( bTrueReturn ){` |
|        - | 15447 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9714 | 15448 | `		SyBlobReset(&pVm->sNamespace);` |
|     4855 | 15449 | `	}` |
|        - | 15450 | `	/* Swap bytecode container */` |
|    12919 | 15451 | `	pByteCode = pVm->pByteContainer;` |
|    12919 | 15452 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 15453 | `	/* Compile the chunk */` |
|    12919 | 15454 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19375 | 15455 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 15456 | `		/* Compilation error,return false */` |
|        3 | 15457 | `		if( pCtx ){` |
|        3 | 15458 | `			ph7_result_bool(pCtx,0);` |
|        1 | 15459 | `		}` |
|        2 | 15460 | `	}else{` |
|        - | 15461 | `		/* Mount any newly defined classes */` |
|        - | 15462 | `		SyHashEntry *pEntry;` |
|        - | 15463 | `		ph7_class *pClass;` |
|        - | 15464 | `		ph7_value sResult; /* Return value */` |
|        - | 15465 | `		sxi32 rc;` |
|    12917 | 15466 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|  1010673 | 15467 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   991305 | 15468 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 15469 | `			/* Only mount classes that haven't been mounted yet */` |
|   991305 | 15470 | `			if( !pClass->bMounted ){` |
|   248725 | 15471 | `				rc = VmMountUserClass(pVm,pClass);` |
|   248725 | 15472 | `				if( rc != SXRET_OK ){` |
|        - | 15473 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 15474 | `					if( pCtx ){` |
|      ! 0 | 15475 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 15476 | `					}` |
|      ! 0 | 15477 | `					goto Cleanup;` |
|        - | 15478 | `				}` |
|   124360 | 15479 | `			}` |
|        5 | 15480 | `		}` |
|    12917 | 15481 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15482 | `			/* Out of memory */` |
|      ! 0 | 15483 | `			if( pCtx ){` |
|      ! 0 | 15484 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15485 | `			}` |
|      ! 0 | 15486 | `			goto Cleanup;` |
|        - | 15487 | `		}` |
|    12917 | 15488 | `		if( bTrueReturn ){` |
|        - | 15489 | `			/* Assume a boolean true return value */` |
|     9714 | 15490 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4859 | 15491 | `		}else{` |
|        - | 15492 | `			/* Assume a null return value */` |
|     3207 | 15493 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15494 | `		}` |
|        - | 15495 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here,` |
|        - | 15496 | `		 * a path the OP_CALL cap check can't see; bound it under the same limit` |
|        - | 15497 | `		 * so a recursive include/eval can't overflow the native stack. */` |
|    12917 | 15498 | `		if( VmRecursionExceeded(pVm) ){` |
|        3 | 15499 | `			PH7_MemObjRelease(&sResult);` |
|        3 | 15500 | `			VmRecursionFatal(pVm);` |
|        3 | 15501 | `			goto Cleanup;` |
|        - | 15502 | `		}` |
|    12915 | 15503 | `		pVm->nRecursionDepth++;` |
|    12915 | 15504 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    12915 | 15505 | `		pVm->nRecursionDepth--;` |
|    12915 | 15506 | `		if( pCtx ){` |
|        - | 15507 | `			/* Set the execution result */` |
|     9767 | 15508 | `			ph7_result_value(pCtx,&sResult);` |
|     4881 | 15509 | `		}` |
|    12915 | 15510 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15511 | `	}` |
|     6457 | 15512 | `Cleanup:` |
|        - | 15513 | `	/* Cleanup the mess left behind */` |
|    12919 | 15514 | `	pVm->pByteContainer = pByteCode;` |
|    12919 | 15515 | `	SySetRelease(&aByteCode);` |
|        - | 15516 | `	/* Restore caller's namespace state */` |
|    12919 | 15517 | `	SyBlobReset(&pVm->sNamespace);` |
|    12919 | 15518 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12919 | 15519 | `	SyBlobRelease(&sSavedNs);` |
|    12919 | 15520 | `	return SXRET_OK;` |
|        5 | 15521 |  |
|        - | 15522 | `/*` |
|        - | 15523 | ` * value eval(string $code)` |
|        - | 15524 | ` *   Evaluate a string as PHP code.` |
|        - | 15525 | ` * Parameter` |
|        - | 15526 | ` *  code: PHP code to evaluate.` |
|        - | 15527 | ` * Return` |
|        - | 15528 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15529 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15530 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15531 | ` */` |
|       58 | 15532 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 15533 |  |
|        - | 15534 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       62 | 15535 | `	if( nArg < 1 ){` |
|        - | 15536 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15537 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15538 | `		return SXRET_OK;` |
|        - | 15539 | `	}` |
|        - | 15540 | `	/* Chunk to evaluate */` |
|       62 | 15541 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       62 | 15542 | `	if( sChunk.nByte < 1 ){` |
|        - | 15543 | `		/* Empty string,return NULL */` |
|        3 | 15544 | `		ph7_result_null(pCtx);` |
|        3 | 15545 | `		return SXRET_OK;` |
|        - | 15546 | `	}` |
|        - | 15547 | `	/* Eval the chunk */` |
|       60 | 15548 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       60 | 15549 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15550 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       37 | 15551 | `		return PH7_ABORT;` |
|        - | 15552 | `	}` |
|       23 | 15553 | `	return SXRET_OK;` |
|       33 | 15554 |  |
|        - | 15555 | `/*` |
|        - | 15556 | ` * Check if a file path is already included.` |
|        - | 15557 | ` */` |
|    19418 | 15558 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        4 | 15559 |  |
|        - | 15560 | `	SyString *aEntries;` |
|        - | 15561 | `	sxu32 n;` |
|    19422 | 15562 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15563 | `	/* Perform a linear search */` |
| 94071198 | 15564 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 94051790 | 15565 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15566 | `			/* Already included */` |
|       11 | 15567 | `			return TRUE;` |
|        - | 15568 | `		}` |
| 47025892 | 15569 | `	}` |
|    19412 | 15570 | `	return FALSE;` |
|     9713 | 15571 |  |
|        - | 15572 | `/*` |
|        - | 15573 | ` * Push a file path in the appropriate VM container.` |
|        - | 15574 | ` */` |
|    22558 | 15575 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 | 15576 |  |
|        - | 15577 | `	SyString sPath;` |
|        - | 15578 | `	char *zDup;` |
|        - | 15579 | `#ifdef __WINNT__` |
|        - | 15580 | `	char *zCur;` |
|        - | 15581 | `#endif` |
|        - | 15582 | `	sxi32 rc;` |
|    22563 | 15583 | `	if( nLen < 0 ){` |
|     3145 | 15584 | `		nLen = SyStrlen(zPath);` |
|     1570 | 15585 | `	}` |
|        - | 15586 | `	/* Duplicate the file path first */` |
|    22563 | 15587 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22563 | 15588 | `	if( zDup == 0 ){` |
|      ! 0 | 15589 | `		return SXERR_MEM;` |
|        - | 15590 | `	}` |
|        - | 15591 | `#ifdef __WINNT__` |
|        - | 15592 | `	/* Normalize path on windows` |
|        - | 15593 | `	 * Example:` |
|        - | 15594 | `	 *    Path/To/File.php` |
|        - | 15595 | `	 * becomes` |
|        - | 15596 | `	 *   path\to\file.php` |
|        - | 15597 | `	 */` |
|        5 | 15598 | `	zCur = zDup;` |
|        5 | 15599 | `	while( zCur[0] != 0 ){` |
|        5 | 15600 | `		if( zCur[0] == '/' ){` |
|        5 | 15601 | `			zCur[0] = '\\';` |
|        5 | 15602 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15603 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15604 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15605 | `		}` |
|        5 | 15606 | `		zCur++;` |
|        5 | 15607 | `	}` |
|        - | 15608 | `#endif` |
|        - | 15609 | `	/* Install the file path */` |
|    22563 | 15610 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22563 | 15611 | `	if( !bMain ){` |
|    19422 | 15612 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15613 | `			/* Already included */` |
|       11 | 15614 | `			*pNew = 0;` |
|        6 | 15615 | `		}else{` |
|        - | 15616 | `			/* Insert in the corresponding container */` |
|    19412 | 15617 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19412 | 15618 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15619 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15620 | `				return rc;` |
|        - | 15621 | `			}` |
|    19412 | 15622 | `			*pNew = 1;` |
|        - | 15623 | `		}` |
|     9709 | 15624 | `	}` |
|    22563 | 15625 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22563 | 15626 | `	return SXRET_OK;` |
|    11284 | 15627 |  |
|        - | 15628 | `/*` |
|        - | 15629 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15630 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15631 | ` * indicates failure.` |
|        - | 15632 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15633 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15634 | ` * operations.` |
|        - | 15635 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15636 | ` * this function is a no-op.` |
|        - | 15637 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15638 | ` * constructs for more information.` |
|        - | 15639 | ` */` |
|     9724 | 15640 | `static sxi32 VmExecIncludedFile(` |
|        - | 15641 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15642 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15643 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15644 | `	 )` |
|        4 | 15645 |  |
|        - | 15646 | `	sxi32 rc;` |
|        - | 15647 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15648 | `	const ph7_io_stream *pStream;` |
|        - | 15649 | `	SyBlob sContents;` |
|        - | 15650 | `	void *pHandle;` |
|        - | 15651 | `	ph7_vm *pVm;` |
|        - | 15652 | `	int isNew;` |
|        - | 15653 | `	/* Initialize fields */` |
|     9728 | 15654 | `	pVm = pCtx->pVm;` |
|     9728 | 15655 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9728 | 15656 | `	isNew = 0;` |
|        - | 15657 | `	/* Extract the associated stream */` |
|     9728 | 15658 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15659 | `	/*` |
|        - | 15660 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15661 | `	 * in a read-only mode.` |
|        - | 15662 | `	 */` |
|     9728 | 15663 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9728 | 15664 | `	if( pHandle == 0 ){` |
|        8 | 15665 | `		return SXERR_IO;` |
|        - | 15666 | `	}` |
|     9722 | 15667 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9722 | 15668 | `	if( IncludeOnce && !isNew ){` |
|        - | 15669 | `		/* Already included */` |
|        9 | 15670 | `		rc = SXERR_EXISTS;` |
|        5 | 15671 | `	}else{` |
|        - | 15672 | `		/* Read the whole file contents */` |
|     9714 | 15673 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9714 | 15674 | `		if( rc == SXRET_OK ){` |
|        - | 15675 | `			SyString sScript;` |
|        - | 15676 | `			/* Compile and execute the script */` |
|     9714 | 15677 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9714 | 15678 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4855 | 15679 | `		}` |
|        - | 15680 | `	}` |
|        - | 15681 | `	/* Pop from the set of included file */` |
|     9722 | 15682 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15683 | `	/* Close the handle */` |
|     9722 | 15684 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15685 | `	/* Release the working buffer */` |
|     9722 | 15686 | `	SyBlobRelease(&sContents);` |
|        - | 15687 | `#else` |
|        - | 15688 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15689 | `	SXUNUSED(pPath);` |
|        - | 15690 | `	SXUNUSED(IncludeOnce);` |
|        - | 15691 | `	rc = SXERR_IO;` |
|        - | 15692 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9722 | 15693 | `	return rc;` |
|     4866 | 15694 |  |
|        - | 15695 | `/*` |
|        - | 15696 | ` * string get_include_path(void)` |
|        - | 15697 | ` *  Gets the current include_path configuration option.` |
|        - | 15698 | ` * Parameter` |
|        - | 15699 | ` *  None` |
|        - | 15700 | ` * Return` |
|        - | 15701 | ` *  Included paths as a string` |
|        - | 15702 | ` */` |
|        2 | 15703 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15704 |  |
|        3 | 15705 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15706 | `	SyString *aEntry;` |
|        - | 15707 | `	int dir_sep;` |
|        - | 15708 | `	sxu32 n;` |
|        - | 15709 | `#ifdef __WINNT__` |
|        1 | 15710 | `	dir_sep = ';';` |
|        - | 15711 | `#else` |
|        - | 15712 | `	/* Assume UNIX path separator */` |
|        2 | 15713 | `	dir_sep = ':';` |
|        - | 15714 | `#endif` |
|        1 | 15715 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15716 | `	SXUNUSED(apArg);` |
|        - | 15717 | `	/* Point to the list of import paths */` |
|        3 | 15718 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15719 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15720 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15721 | `		if( n > 0 ){` |
|        - | 15722 | `			/* Append dir seprator */` |
|      ! 0 | 15723 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15724 | `		}` |
|        - | 15725 | `		/* Append path */` |
|        3 | 15726 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15727 | `	}` |
|        3 | 15728 | `	return PH7_OK;` |
|        1 | 15729 |  |
|        - | 15730 | `/*` |
|        - | 15731 | ` * string get_get_included_files(void)` |
|        - | 15732 | ` *  Gets the current include_path configuration option.` |
|        - | 15733 | ` * Parameter` |
|        - | 15734 | ` *  None` |
|        - | 15735 | ` * Return` |
|        - | 15736 | ` *  Included paths as a string` |
|        - | 15737 | ` */` |
|        2 | 15738 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15739 |  |
|        3 | 15740 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15741 | `	ph7_value *pArray,*pWorker;` |
|        - | 15742 | `	SyString *pEntry;` |
|        - | 15743 | `	int c,d;` |
|        - | 15744 | `	/* Create an array and a working value */` |
|        3 | 15745 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15746 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15747 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15748 | `		/* Out of memory,return null */` |
|      ! 0 | 15749 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15750 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15751 | `		SXUNUSED(apArg);` |
|      ! 0 | 15752 | `		return PH7_OK;` |
|        - | 15753 | `	}` |
|        3 | 15754 | `	c = d = '/';` |
|        - | 15755 | `#ifdef __WINNT__` |
|        1 | 15756 | `	d = '\\';` |
|        - | 15757 | `#endif` |
|        - | 15758 | `	/* Iterate throw entries */` |
|        3 | 15759 | `	SySetResetCursor(pFiles);` |
|     3917 | 15760 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15761 | `		const char *zBase,*zEnd;` |
|        - | 15762 | `		int iLen;` |
|        - | 15763 | `		/* reset the string cursor */` |
|     3915 | 15764 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15765 | `		/* Extract base name */` |
|     3915 | 15766 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15767 | `		/* Ignore trailing '/' */` |
|     5872 | 15768 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15769 | `			zEnd--;` |
|      ! 0 | 15770 | `		}` |
|     3915 | 15771 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15772 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15773 | `			zEnd--;` |
|        1 | 15774 | `		}` |
|     3915 | 15775 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15776 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15777 | `		/* Copy entry name */` |
|     3915 | 15778 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15779 | `		/* Perform the insertion */` |
|     3915 | 15780 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15781 | `	}` |
|        - | 15782 | `	/* All done,return the created array */` |
|        3 | 15783 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15784 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15785 | `	 * by the engine as soon we return from this foreign` |
|        - | 15786 | `	 * function.` |
|        - | 15787 | `	 */` |
|        3 | 15788 | `	return PH7_OK;` |
|        2 | 15789 |  |
|        - | 15790 | `/*` |
|        - | 15791 | ` * include:` |
|        - | 15792 | ` * According to the PHP reference manual.` |
|        - | 15793 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15794 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15795 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15796 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15797 | ` *  and the current working directory before failing. The include()` |
|        - | 15798 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15799 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15800 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15801 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15802 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15803 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15804 | ` *  directory to find the requested file.` |
|        - | 15805 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15806 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15807 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15808 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15809 | ` */` |
|     9700 | 15810 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 15811 |  |
|        - | 15812 | `	SyString sFile;` |
|        - | 15813 | `	sxi32 rc;` |
|     9703 | 15814 | `	if( nArg < 1 ){` |
|        - | 15815 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15816 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15817 | `		return SXRET_OK;` |
|        - | 15818 | `	}` |
|        - | 15819 | `	/* File to include */` |
|     9703 | 15820 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9703 | 15821 | `	if( sFile.nByte < 1 ){` |
|        - | 15822 | `		/* Empty string,return NULL */` |
|      ! 0 | 15823 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15824 | `		return SXRET_OK;` |
|        - | 15825 | `	}` |
|        - | 15826 | `	/* Open,compile and execute the desired script */` |
|     9703 | 15827 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9703 | 15828 | `	if( rc != SXRET_OK ){` |
|        - | 15829 | `		/* Emit a warning and return false */` |
|        3 | 15830 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15831 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15832 | `	}` |
|     9703 | 15833 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15834 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 | 15835 | `		return PH7_ABORT;` |
|        - | 15836 | `	}` |
|     9698 | 15837 | `	return SXRET_OK;` |
|     4853 | 15838 |  |
|        - | 15839 | `/*` |
|        - | 15840 | ` * include_once:` |
|        - | 15841 | ` *  According to the PHP reference manual.` |
|        - | 15842 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15843 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15844 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15845 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15846 | ` *   just once.` |
|        - | 15847 | ` */` |
|       10 | 15848 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15849 |  |
|        - | 15850 | `	SyString sFile;` |
|        - | 15851 | `	sxi32 rc;` |
|       11 | 15852 | `	if( nArg < 1 ){` |
|        - | 15853 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15854 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15855 | `		return SXRET_OK;` |
|        - | 15856 | `	}` |
|        - | 15857 | `	/* File to include */` |
|       11 | 15858 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15859 | `	if( sFile.nByte < 1 ){` |
|        - | 15860 | `		/* Empty string,return NULL */` |
|      ! 0 | 15861 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15862 | `		return SXRET_OK;` |
|        - | 15863 | `	}` |
|        - | 15864 | `	/* Open,compile and execute the desired script */` |
|       11 | 15865 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15866 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15867 | `		/* File already included,return TRUE */` |
|        7 | 15868 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15869 | `		return SXRET_OK;` |
|        - | 15870 | `	}` |
|        5 | 15871 | `	if( rc != SXRET_OK ){` |
|        - | 15872 | `		/* Emit a warning and return false */` |
|      ! 0 | 15873 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15874 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15875 | ` 	}` |
|        5 | 15876 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15877 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15878 | `		return PH7_ABORT;` |
|        - | 15879 | `	}` |
|        5 | 15880 | `	return SXRET_OK;` |
|        6 | 15881 |  |
|        - | 15882 | `/*` |
|        - | 15883 | ` * require.` |
|        - | 15884 | ` *  According to the PHP reference manual.` |
|        - | 15885 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15886 | ` *   also produce a fatal level error.` |
|        - | 15887 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15888 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15889 | ` */` |
|        6 | 15890 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15891 |  |
|        - | 15892 | `	SyString sFile;` |
|        - | 15893 | `	sxi32 rc;` |
|        8 | 15894 | `	if( nArg < 1 ){` |
|        - | 15895 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15896 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15897 | `		return SXRET_OK;` |
|        - | 15898 | `	}` |
|        - | 15899 | `	/* File to include */` |
|        8 | 15900 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15901 | `	if( sFile.nByte < 1 ){` |
|        - | 15902 | `		/* Empty string,return NULL */` |
|      ! 0 | 15903 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15904 | `		return SXRET_OK;` |
|        - | 15905 | `	}` |
|        - | 15906 | `	/* Open,compile and execute the desired script */` |
|        8 | 15907 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15908 | `	if( rc != SXRET_OK ){` |
|        - | 15909 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15910 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15911 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15912 | `		return PH7_ABORT;` |
|        - | 15913 | `	}` |
|        8 | 15914 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15915 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15916 | `		return PH7_ABORT;` |
|        - | 15917 | `	}` |
|        8 | 15918 | `	return SXRET_OK;` |
|        5 | 15919 |  |
|        - | 15920 | `/*` |
|        - | 15921 | ` * require_once:` |
|        - | 15922 | ` *  According to the PHP reference manual.` |
|        - | 15923 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15924 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15925 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15926 | ` *   and how it differs from its non _once siblings.` |
|        - | 15927 | ` */` |
|        4 | 15928 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15929 |  |
|        - | 15930 | `	SyString sFile;` |
|        - | 15931 | `	sxi32 rc;` |
|        5 | 15932 | `	if( nArg < 1 ){` |
|        - | 15933 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15934 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15935 | `		return SXRET_OK;` |
|        - | 15936 | `	}` |
|        - | 15937 | `	/* File to include */` |
|        5 | 15938 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15939 | `	if( sFile.nByte < 1 ){` |
|        - | 15940 | `		/* Empty string,return NULL */` |
|      ! 0 | 15941 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15942 | `		return SXRET_OK;` |
|        - | 15943 | `	}` |
|        - | 15944 | `	/* Open,compile and execute the desired script */` |
|        5 | 15945 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15946 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15947 | `		/* File already included,return TRUE */` |
|        3 | 15948 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15949 | `		return SXRET_OK;` |
|        - | 15950 | `	}` |
|        3 | 15951 | `	if( rc != SXRET_OK ){` |
|        - | 15952 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15953 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15954 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15955 | `		return PH7_ABORT;` |
|        - | 15956 | `	}` |
|        3 | 15957 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15958 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15959 | `		return PH7_ABORT;` |
|        - | 15960 | `	}` |
|        3 | 15961 | `	return SXRET_OK;` |
|        3 | 15962 |  |
|        - | 15963 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15964 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15965 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15966 | `/*` |
|        - | 15967 | ` * Section:` |
|        - | 15968 | ` *  SPL Autoloading functions.` |
|        - | 15969 | ` * Status:` |
|        - | 15970 | ` *  Stable.` |
|        - | 15971 | ` */` |
|        - | 15972 | `/*` |
|        - | 15973 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15974 | ` *  Register given function as __autoload() implementation.` |
|        - | 15975 | ` * Parameters` |
|        - | 15976 | ` *  callback` |
|        - | 15977 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15978 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15979 | ` *  throw` |
|        - | 15980 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15981 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15982 | ` *  prepend` |
|        - | 15983 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15984 | ` *   autoload stack instead of appending it.` |
|        - | 15985 | ` * Return` |
|        - | 15986 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15987 | ` */` |
|       34 | 15988 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 15989 |  |
|        - | 15990 | `	VmAutoloadCB sEntry;` |
|       39 | 15991 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 | 15992 | `	int iPrepend = 0;` |
|        - | 15993 | `	sxu32 n;` |
|       39 | 15994 | `	if( nArg < 1 ){` |
|        - | 15995 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15996 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15997 | `		/* Check for duplicates first */` |
|        9 | 15998 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15999 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 16000 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 16001 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 16002 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 16003 | `				ph7_result_bool(pCtx,1);` |
|        5 | 16004 | `				return SXRET_OK;` |
|        - | 16005 | `			}` |
|      ! 0 | 16006 | `		}` |
|        5 | 16007 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 16008 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 16009 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 16010 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 16011 | `		ph7_result_bool(pCtx,1);` |
|        5 | 16012 | `		return SXRET_OK;` |
|        - | 16013 | `	}` |
|        - | 16014 | `	/* Validate that the callback is callable */` |
|       31 | 16015 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 16016 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 16017 | `		if( nArg >= 2 ){` |
|      ! 0 | 16018 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 16019 | `		}` |
|      ! 0 | 16020 | `		if( iThrow ){` |
|      ! 0 | 16021 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 16022 | `				"Argument is not callable");` |
|      ! 0 | 16023 | `		}` |
|      ! 0 | 16024 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16025 | `		return SXRET_OK;` |
|        - | 16026 | `	}` |
|        - | 16027 | `	/* Check for duplicates */` |
|       49 | 16028 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 16029 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 16030 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 16031 | `			/* Already registered */` |
|      ! 0 | 16032 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 16033 | `			return SXRET_OK;` |
|        - | 16034 | `		}` |
|       11 | 16035 | `	}` |
|        - | 16036 | `	/* Check prepend flag */` |
|       31 | 16037 | `	if( nArg >= 3 ){` |
|        3 | 16038 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 16039 | `	}` |
|        - | 16040 | `	/* Store the callback */` |
|       31 | 16041 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       31 | 16042 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       31 | 16043 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       32 | 16044 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 16045 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 16046 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 16047 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 16048 | `		VmAutoloadCB *aBase;` |
|        3 | 16049 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 16050 | `		/* Rotate: move last entry to front */` |
|        3 | 16051 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 16052 | `		if( aBase ){` |
|        - | 16053 | `			VmAutoloadCB sTemp;` |
|        - | 16054 | `			sxu32 i;` |
|        3 | 16055 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 16056 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 16057 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 16058 | `			}` |
|        3 | 16059 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 16060 | `		}` |
|        2 | 16061 | `	}else{` |
|       29 | 16062 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 16063 | `	}` |
|       31 | 16064 | `	ph7_result_bool(pCtx,1);` |
|       31 | 16065 | `	return SXRET_OK;` |
|       22 | 16066 |  |
|        - | 16067 | `/*` |
|        - | 16068 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 16069 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 16070 | ` * Parameters` |
|        - | 16071 | ` *  callback` |
|        - | 16072 | ` *   The autoload function being unregistered.` |
|        - | 16073 | ` * Return` |
|        - | 16074 | ` *  TRUE on success, FALSE on failure.` |
|        - | 16075 | ` */` |
|       32 | 16076 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 16077 |  |
|       37 | 16078 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16079 | `	sxu32 n,nEntry;` |
|       37 | 16080 | `	if( nArg < 1 ){` |
|      ! 0 | 16081 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 16082 | `		return SXRET_OK;` |
|        - | 16083 | `	}` |
|       37 | 16084 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 | 16085 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 | 16086 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 | 16087 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 16088 | `			/* Found — remove by shifting remaining entries down */` |
|       35 | 16089 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 16090 | `			sxu32 i;` |
|       35 | 16091 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 | 16092 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 16093 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 16094 | `			}` |
|        - | 16095 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 | 16096 | `			SySetPop(&pVm->aAutoload);` |
|       35 | 16097 | `			ph7_result_bool(pCtx,1);` |
|       35 | 16098 | `			return SXRET_OK;` |
|        - | 16099 | `		}` |
|        3 | 16100 | `	}` |
|        3 | 16101 | `	ph7_result_bool(pCtx,0);` |
|        3 | 16102 | `	return SXRET_OK;` |
|       21 | 16103 |  |
|        - | 16104 | `/*` |
|        - | 16105 | ` * array spl_autoload_functions(void)` |
|        - | 16106 | ` *  Return all registered __autoload() functions.` |
|        - | 16107 | ` * Return` |
|        - | 16108 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 16109 | ` *  an empty array is returned.` |
|        - | 16110 | ` */` |
|       20 | 16111 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16112 |  |
|       21 | 16113 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 16114 | `	ph7_value *pArray;` |
|        - | 16115 | `	sxu32 n,nEntry;` |
|       10 | 16116 | `	SXUNUSED(nArg);` |
|       10 | 16117 | `	SXUNUSED(apArg);` |
|       21 | 16118 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 16119 | `	if( pArray == 0 ){` |
|      ! 0 | 16120 | `		ph7_result_null(pCtx);` |
|      ! 0 | 16121 | `		return SXRET_OK;` |
|        - | 16122 | `	}` |
|       21 | 16123 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 16124 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 16125 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 16126 | `		if( pEntry ){` |
|       15 | 16127 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 16128 | `		}` |
|        8 | 16129 | `	}` |
|       21 | 16130 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 16131 | `	return SXRET_OK;` |
|       11 | 16132 |  |
|        - | 16133 | `/*` |
|        - | 16134 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 16135 | ` *  Default implementation of __autoload().` |
|        - | 16136 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 16137 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 16138 | ` * Parameters` |
|        - | 16139 | ` *  class` |
|        - | 16140 | ` *   The class name being searched.` |
|        - | 16141 | ` *  file_extensions` |
|        - | 16142 | ` *   Comma-separated list of file extensions to try.` |
|        - | 16143 | ` */` |
|        2 | 16144 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 16145 |  |
|        - | 16146 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 16147 | `	SyBlob sPath;` |
|        - | 16148 | `	int nClass;` |
|        - | 16149 | `	sxi32 rc;` |
|        3 | 16150 | `	if( nArg < 1 ){` |
|      ! 0 | 16151 | `		return SXRET_OK;` |
|        - | 16152 | `	}` |
|        3 | 16153 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 16154 | `	if( nClass < 1 ){` |
|      ! 0 | 16155 | `		return SXRET_OK;` |
|        - | 16156 | `	}` |
|        - | 16157 | `	/* Default extensions */` |
|        3 | 16158 | `	zExt = ".php,.inc";` |
|        3 | 16159 | `	if( nArg >= 2 ){` |
|        - | 16160 | `		int nExt;` |
|      ! 0 | 16161 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 16162 | `		if( nExt < 1 ){` |
|      ! 0 | 16163 | `			zExt = ".php,.inc";` |
|      ! 0 | 16164 | `		}` |
|      ! 0 | 16165 | `	}` |
|        3 | 16166 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 16167 | `	/* Iterate over comma-separated extensions */` |
|        3 | 16168 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 16169 | `	zCur = zExt;` |
|        7 | 16170 | `	while( zCur < zEnd ){` |
|        - | 16171 | `		const char *zComma;` |
|        - | 16172 | `		SyString sFile;` |
|        - | 16173 | `		int i;` |
|        - | 16174 | `		/* Find next comma or end */` |
|        5 | 16175 | `		zComma = zCur;` |
|       21 | 16176 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 16177 | `			zComma++;` |
|        1 | 16178 | `		}` |
|        - | 16179 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 16180 | `		SyBlobReset(&sPath);` |
|       69 | 16181 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 16182 | `			char c = zClass[i];` |
|       65 | 16183 | `			if( c == '\\' ){` |
|      ! 0 | 16184 | `				c = '/';` |
|       65 | 16185 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 16186 | `				c = c + ('a' - 'A');` |
|        6 | 16187 | `			}` |
|       65 | 16188 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 16189 | `		}` |
|        - | 16190 | `		/* Append extension */` |
|        5 | 16191 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 16192 | `		/* Try to include the file */` |
|        5 | 16193 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 16194 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 16195 | `		if( rc == SXRET_OK ){` |
|        - | 16196 | `			/* File included successfully */` |
|      ! 0 | 16197 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 16198 | `			return SXRET_OK;` |
|        - | 16199 | `		}` |
|        - | 16200 | `		/* Move past the comma */` |
|        5 | 16201 | `		zCur = zComma;` |
|        5 | 16202 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 16203 | `			zCur++;` |
|        1 | 16204 | `		}` |
|        1 | 16205 | `	}` |
|        3 | 16206 | `	SyBlobRelease(&sPath);` |
|        3 | 16207 | `	return SXRET_OK;` |
|        2 | 16208 |  |
|        - | 16209 | `/* Table of built-in VM functions. */` |
|        - | 16210 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 16211 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 16212 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 16213 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 16214 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 16215 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 16216 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 16217 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 16218 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 16219 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 16220 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 16221 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 16222 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 16223 | `	    /* Constants management */` |
|        - | 16224 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 16225 | `	{ "define",   vm_builtin_define               },` |
|        - | 16226 | `	{ "constant", vm_builtin_constant             },` |
|        - | 16227 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 16228 | `	   /* Class/Object functions */` |
|        - | 16229 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 16230 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 16231 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 16232 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 16233 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 16234 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 16235 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 16236 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 16237 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 16238 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 16239 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 16240 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 16241 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 16242 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 16243 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 16244 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 16245 | `	   /* SPL Autoloading */` |
|        - | 16246 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 16247 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 16248 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 16249 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 16250 | `	   /* Random numbers/strings generators */` |
|        - | 16251 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 16252 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 16253 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 16254 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 16255 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 16256 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 16257 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 16258 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16259 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 16260 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 16261 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 16262 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16263 | `	   /* Language constructs functions */` |
|        - | 16264 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 16265 | `	{ "print", vm_builtin_print                   },` |
|        - | 16266 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 16267 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 16268 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 16269 | `	  /* Variable handling functions */` |
|        - | 16270 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 16271 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 16272 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 16273 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 16274 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 16275 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 16276 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 16277 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 16278 | `	  /* Ouput control functions */` |
|        - | 16279 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 16280 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 16281 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 16282 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 16283 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 16284 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 16285 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 16286 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 16287 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 16288 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 16289 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 16290 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 16291 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 16292 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 16293 | `	  /* Assertion functions */` |
|        - | 16294 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 16295 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 16296 | `	  /* Error reporting functions */` |
|        - | 16297 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 16298 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 16299 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 16300 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 16301 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 16302 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 16303 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 16304 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 16305 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 16306 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 16307 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 16308 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 16309 | `	  /* Release info */` |
|        - | 16310 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 16311 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 16312 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 16313 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 16314 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 16315 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 16316 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 16317 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 16318 | `	  /* hashmap */` |
|        - | 16319 | `	{"compact",          vm_builtin_compact       },` |
|        - | 16320 | `	{"extract",          vm_builtin_extract       },` |
|        - | 16321 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 16322 | `	  /* URL related function */` |
|        - | 16323 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 16324 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 16325 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 16326 | `	   /* XML processing functions */` |
|        - | 16327 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 16328 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 16329 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 16330 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 16331 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 16332 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 16333 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 16334 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 16335 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 16336 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 16337 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 16338 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 16339 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 16340 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 16341 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 16342 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 16343 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 16344 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 16345 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 16346 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 16347 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 16348 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 16349 | `	   /* UTF-8 encoding/decoding */` |
|        - | 16350 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 16351 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 16352 | `	   /* Command line processing */` |
|        - | 16353 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 16354 | `	   /* JSON encoding/decoding */` |
|        - | 16355 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 16356 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 16357 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 16358 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 16359 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 16360 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 16361 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 16362 | `	   /* Files/URI inclusion facility */` |
|        - | 16363 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 16364 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 16365 | `	{ "include",      vm_builtin_include          },` |
|        - | 16366 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 16367 | `	{ "require",      vm_builtin_require          },` |
|        - | 16368 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 16369 | `};` |
|        - | 16370 | `/*` |
|        - | 16371 | ` * Register the built-in VM functions defined above.` |
|        - | 16372 | ` */` |
|     2834 | 16373 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        5 | 16374 |  |
|        - | 16375 | `	sxi32 rc;` |
|        - | 16376 | `	sxu32 n;` |
|   382595 | 16377 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 16378 | `		/* Note that these special functions have access` |
|        - | 16379 | `		 * to the underlying virtual machine as their` |
|        - | 16380 | `		 * private data.` |
|        - | 16381 | `		 */` |
|   379761 | 16382 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   379761 | 16383 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 16384 | `			return rc;` |
|        - | 16385 | `		}` |
|   189883 | 16386 | `	}` |
|     2839 | 16387 | `	return SXRET_OK;` |
|     1422 | 16388 |  |
|        - | 16389 | `/*` |
|        - | 16390 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 16391 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 16392 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 16393 | ` */` |
|   186234 | 16394 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        5 | 16395 |  |
|   186239 | 16396 | `	if( !iLoadable ){` |
|   184061 | 16397 | `		return pClass;` |
|        - | 16398 | `	}` |
|     2189 | 16399 | `	while(pClass){` |
|     2183 | 16400 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2177 | 16401 | `			return pClass;` |
|        - | 16402 | `		}` |
|        7 | 16403 | `		pClass = pClass->pNextName;` |
|        1 | 16404 | `	}` |
|        7 | 16405 | `	return 0;` |
|    93122 | 16406 |  |
|        - | 16407 | `/*` |
|        - | 16408 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 16409 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 16410 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 16411 | ` * registered in the VM's class table.` |
|        - | 16412 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 16413 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 16414 | ` */` |
|       38 | 16415 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 16416 |  |
|        - | 16417 | `	VmAutoloadCB *pEntry;` |
|        - | 16418 | `	ph7_value sArg,sResult;` |
|        - | 16419 | `	SyHashEntry *pHashEntry;` |
|        - | 16420 | `	ph7_class *pClass;` |
|        - | 16421 | `	sxu32 n,nEntry;` |
|       43 | 16422 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       43 | 16423 | `	if( nEntry < 1 ){` |
|       28 | 16424 | `		return 0;` |
|        - | 16425 | `	}` |
|        - | 16426 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       19 | 16427 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 16428 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 16429 | `	}` |
|        - | 16430 | `	/* Mark this class as being autoloaded */` |
|       17 | 16431 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 16432 | `	/* Prepare the class name argument */` |
|       17 | 16433 | `	PH7_MemObjInit(pVm,&sArg);` |
|       17 | 16434 | `	PH7_MemObjInit(pVm,&sResult);` |
|       17 | 16435 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       17 | 16436 | `	pClass = 0;` |
|       31 | 16437 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 16438 | `		ph7_value *apArg[1];` |
|       27 | 16439 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       27 | 16440 | `		if( pEntry == 0 ){` |
|      ! 0 | 16441 | `			continue;` |
|        - | 16442 | `		}` |
|       27 | 16443 | `		apArg[0] = &sArg;` |
|       27 | 16444 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 16445 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 16446 | `			continue;` |
|        - | 16447 | `		}` |
|        - | 16448 | `		/* Check if the class is now available */` |
|       27 | 16449 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       27 | 16450 | `		if( pHashEntry ){` |
|       12 | 16451 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       12 | 16452 | `			if( pClass ){` |
|       12 | 16453 | `				break;` |
|        - | 16454 | `			}` |
|      ! 0 | 16455 | `		}` |
|       10 | 16456 | `	}` |
|       17 | 16457 | `	PH7_MemObjRelease(&sArg);` |
|       17 | 16458 | `	PH7_MemObjRelease(&sResult);` |
|        - | 16459 | `	/* Remove reentrancy guard */` |
|       17 | 16460 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       17 | 16461 | `	return pClass;` |
|       24 | 16462 |  |
|        - | 16463 | `/*` |
|        - | 16464 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 16465 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 16466 | ` */` |
|       18 | 16467 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        5 | 16468 |  |
|       23 | 16469 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        5 | 16470 |  |
|        - | 16471 | `/*` |
|        - | 16472 | ` * Check if the given name refer to an installed class.` |
|        - | 16473 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 16474 | ` */` |
|   186246 | 16475 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 16476 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 16477 | `	const char *zName,  /* Name of the target class */` |
|        - | 16478 | `	sxu32 nByte,        /* zName length */` |
|        - | 16479 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 16480 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 16481 | `						 */` |
|        - | 16482 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 16483 | `	)` |
|        5 | 16484 |  |
|        - | 16485 | `	SyHashEntry *pEntry;` |
|        - | 16486 | `	ph7_class *pClass;` |
|    93123 | 16487 | `	SXUNUSED(iNest);` |
|        - | 16488 | `	/* Exact class lookup.` |
|        - | 16489 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16490 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   186251 | 16491 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   186251 | 16492 | `	if( pEntry == 0 ){` |
|        - | 16493 | `		/* Class not found in hash table — try autoload before giving up */` |
|       23 | 16494 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16495 | `	}` |
|   186231 | 16496 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   186231 | 16497 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    93128 | 16498 |  |
|        - | 16499 | `/*` |
|        - | 16500 | ` * Reference Table Implementation` |
|        - | 16501 | ` * Status: stable <chm@symisc.net>` |
|        - | 16502 | ` * Intro` |
|        - | 16503 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16504 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16505 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16506 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16507 | ` *  Refer to the official for more information on this powerful` |
|        - | 16508 | ` *  extension.` |
|        - | 16509 | ` */` |
|        - | 16510 | `/*` |
|        - | 16511 | ` * Allocate a new reference entry.` |
|        - | 16512 | ` */` |
|  3216320 | 16513 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 16514 |  |
|        - | 16515 | `	VmRefObj *pRef;` |
|        - | 16516 | `	/* Allocate a new instance */` |
|  3216325 | 16517 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3216325 | 16518 | `	if( pRef == 0 ){` |
|      ! 0 | 16519 | `		return 0;` |
|        - | 16520 | `	}` |
|        - | 16521 | `	/* Zero the structure */` |
|  3216325 | 16522 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16523 | `	/* Initialize fields */` |
|  3216325 | 16524 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3216325 | 16525 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3216325 | 16526 | `	pRef->nIdx = nIdx;` |
|  3216325 | 16527 | `	return pRef;` |
|  1608165 | 16528 |  |
|        - | 16529 | `/*` |
|        - | 16530 | ` * Default hash function used by the reference table` |
|        - | 16531 | ` * for lookup/insertion operations.` |
|        - | 16532 | ` */` |
| 17600067 | 16533 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        5 | 16534 |  |
|        - | 16535 | `	/* Calculate the hash based on the memory object index */` |
| 17600072 | 16536 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        5 | 16537 |  |
|        - | 16538 | `/*` |
|        - | 16539 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16540 | ` * in the reference table.` |
|        - | 16541 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16542 | ` * otherwise.` |
|        - | 16543 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16544 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16545 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16546 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16547 | ` * Refer to the official for more information on this powerful` |
|        - | 16548 | ` * extension.` |
|        - | 16549 | ` */` |
|  9587130 | 16550 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        5 | 16551 |  |
|        - | 16552 | `	VmRefObj *pRef;` |
|        - | 16553 | `	sxu32 nBucket;` |
|        - | 16554 | `	/* Point to the appropriate bucket */` |
|  9587135 | 16555 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16556 | `	/* Perform the lookup */` |
|  9587135 | 16557 | `	pRef = pVm->apRefObj[nBucket];` |
| 21081735 | 16558 | `	for(;;){` |
| 42145035 | 16559 | `		if( pRef == 0 ){` |
|  3322995 | 16560 | `			break;` |
|        - | 16561 | `		}` |
| 38822045 | 16562 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16563 | `			/* Entry found */` |
|  6264145 | 16564 | `			return pRef;` |
|        - | 16565 | `		}` |
|        - | 16566 | `		/* Point to the next entry */` |
| 32557905 | 16567 | `		pRef = pRef->pNextCollide;` |
|        5 | 16568 | `	}` |
|        - | 16569 | `	/* No such entry,return NULL */` |
|  3322995 | 16570 | `	return 0;` |
|  4793570 | 16571 |  |
|        - | 16572 | `/*` |
|        - | 16573 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16574 | ` *` |
|        - | 16575 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16576 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16577 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16578 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16579 | ` * Refer to the official for more information on this powerful` |
|        - | 16580 | ` * extension.` |
|        - | 16581 | ` */` |
|  3216320 | 16582 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 16583 |  |
|        - | 16584 | `	sxu32 nBucket;` |
|  3216325 | 16585 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16586 | `		VmRefObj **apNew;` |
|        - | 16587 | `		sxu32 nNew;` |
|        - | 16588 | `		/* Allocate a larger table */` |
|     4499 | 16589 | `		nNew = pVm->nRefSize << 1;` |
|     4499 | 16590 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4499 | 16591 | `		if( apNew ){` |
|     4499 | 16592 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16593 | `			sxu32 n;` |
|        - | 16594 | `			/* Zero the structure */` |
|     4499 | 16595 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16596 | `			/* Rehash all referenced entries */` |
|  2848345 | 16597 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16598 | `				/* Remove old collision links */` |
|  2843851 | 16599 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16600 | `				/* Point to the appropriate bucket */` |
|  2843851 | 16601 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16602 | `				/* Insert the entry  */` |
|  2843851 | 16603 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843851 | 16604 | `				if( apNew[nBucket] ){` |
|  2301119 | 16605 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16606 | `				}` |
|  2843851 | 16607 | `				apNew[nBucket] = pEntry;` |
|        - | 16608 | `				/* Point to the next entry */` |
|  2843851 | 16609 | `				pEntry = pEntry->pNext;` |
|  1421928 | 16610 | `			}` |
|        - | 16611 | `			/* Release the old table */` |
|     4499 | 16612 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16613 | `			/* Install the new one */` |
|     4499 | 16614 | `			pVm->apRefObj = apNew;` |
|     4499 | 16615 | `			pVm->nRefSize = nNew;` |
|     2247 | 16616 | `		}` |
|     2247 | 16617 | `	}` |
|        - | 16618 | `	/* Point to the appropriate bucket */` |
|  3216325 | 16619 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16620 | `	/* Insert the entry */` |
|  3216325 | 16621 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3216325 | 16622 | `	if( pVm->apRefObj[nBucket] ){` |
|  2622118 | 16623 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1311059 | 16624 | `	}` |
|  3216325 | 16625 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3216325 | 16626 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3216325 | 16627 | `	pVm->nRefUsed++;` |
|  3216325 | 16628 | `	return SXRET_OK;` |
|        5 | 16629 |  |
|        - | 16630 | `/*` |
|        - | 16631 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16632 | ` * the reference table.` |
|        - | 16633 | ` * This function is invoked when the user perform an unset` |
|        - | 16634 | ` * call [i.e: unset($var); ].` |
|        - | 16635 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16636 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16637 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16638 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16639 | ` * Refer to the official for more information on this powerful` |
|        - | 16640 | ` * extension.` |
|        - | 16641 | ` */` |
|  3174126 | 16642 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        5 | 16643 |  |
|        - | 16644 | `	ph7_hashmap_node **apNode;` |
|        - | 16645 | `	SyHashEntry **apEntry;` |
|        - | 16646 | `	sxu32 n;` |
|        - | 16647 | `	/* Point to the reference table */` |
|  3174131 | 16648 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3174131 | 16649 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16650 | `	/* Unlink the entry from the reference table */` |
|  3286805 | 16651 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   112679 | 16652 | `		if( apEntry[n] ){` |
|   112629 | 16653 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    56312 | 16654 | `		}` |
|    56342 | 16655 | `	}` |
|  6235559 | 16656 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3061433 | 16657 | `		if( apNode[n] ){` |
|     7080 | 16658 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3538 | 16659 | `		}` |
|  1530719 | 16660 | `	}` |
|  3174131 | 16661 | `	if( pRef->pPrevCollide ){` |
|  1221360 | 16662 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   611012 | 16663 | `	}else{` |
|  1952776 | 16664 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16665 | `	}` |
|  3174131 | 16666 | `	if( pRef->pNextCollide ){` |
|  1809176 | 16667 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   904580 | 16668 | `	}` |
|  3174131 | 16669 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16670 | `	/* Release the node */` |
|  3174131 | 16671 | `	SySetRelease(&pRef->aReference);` |
|  3174131 | 16672 | `	SySetRelease(&pRef->aArrEntries);` |
|  3174131 | 16673 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3174131 | 16674 | `	pVm->nRefUsed--;` |
|  3174131 | 16675 | `	return SXRET_OK;` |
|        5 | 16676 |  |
|        - | 16677 | `/*` |
|        - | 16678 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16679 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16680 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16681 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16682 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16683 | ` * Refer to the official for more information on this powerful` |
|        - | 16684 | ` * extension.` |
|        - | 16685 | ` */` |
|  3252102 | 16686 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16687 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16688 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16689 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16690 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16691 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16692 | `	)` |
|        5 | 16693 |  |
|  3252107 | 16694 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16695 | `	VmRefObj *pRef;` |
|        - | 16696 | `	/* Check if the referenced object already exists */` |
|  3252107 | 16697 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3252107 | 16698 | `	if( pRef == 0 ){` |
|        - | 16699 | `		/* Create a new entry */` |
|  3216325 | 16700 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3216325 | 16701 | `		if( pRef == 0 ){` |
|      ! 0 | 16702 | `			return SXERR_MEM;` |
|        - | 16703 | `		}` |
|  3216325 | 16704 | `		pRef->iFlags = iFlags;` |
|        - | 16705 | `		/* Install the entry */` |
|  3216325 | 16706 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1608160 | 16707 | `	}` |
|  3252107 | 16708 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3252107 | 16709 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16710 | `		VmSlot sRef;` |
|        - | 16711 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16712 | `		 * be deleted when we leave this frame.` |
|        - | 16713 | `		 */` |
|   106785 | 16714 | `		sRef.nIdx = nIdx;` |
|   106785 | 16715 | `		sRef.pUserData = pEntry;` |
|   106785 | 16716 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16717 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16718 | `		}` |
|    53390 | 16719 | `	}` |
|  3252107 | 16720 | `	if( pEntry ){` |
|        - | 16721 | `		/* Address of the hash-entry */` |
|   142339 | 16722 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    71167 | 16723 | `	}` |
|  3252107 | 16724 | `	if( pMapEntry ){` |
|        - | 16725 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3100891 | 16726 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1550443 | 16727 | `	}` |
|  3252107 | 16728 | `	return SXRET_OK;` |
|  1626056 | 16729 |  |
|        - | 16730 | `/*` |
|        - | 16731 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16732 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16733 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16734 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16735 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16736 | ` * Refer to the official for more information on this powerful` |
|        - | 16737 | ` * extension.` |
|        - | 16738 | ` */` |
|  3161094 | 16739 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16740 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16741 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16742 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16743 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16744 | `	)` |
|        5 | 16745 |  |
|        - | 16746 | `	VmRefObj *pRef;` |
|        - | 16747 | `	sxu32 n;` |
|        - | 16748 | `	/* Check if the referenced object already exists */` |
|  3161099 | 16749 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3161099 | 16750 | `	if( pRef == 0 ){` |
|        - | 16751 | `		/* Not such entry */` |
|   106669 | 16752 | `		return SXERR_NOTFOUND;` |
|        - | 16753 | `	}` |
|        - | 16754 | `	/* Remove the desired entry */` |
|  3054435 | 16755 | `	if( pEntry ){` |
|        - | 16756 | `		SyHashEntry **apEntry;` |
|       77 | 16757 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      267 | 16758 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      195 | 16759 | `			if( apEntry[n] == pEntry ){` |
|        - | 16760 | `				/* Nullify the entry */` |
|       77 | 16761 | `				apEntry[n] = 0;` |
|        - | 16762 | `				/*` |
|        - | 16763 | `				 * NOTE:` |
|        - | 16764 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16765 | `				 * we avoid wasting spaces.` |
|        - | 16766 | `				 */` |
|       36 | 16767 | `			}` |
|      100 | 16768 | `		}` |
|       36 | 16769 | `	}` |
|  3054435 | 16770 | `	if( pMapEntry ){` |
|        - | 16771 | `		ph7_hashmap_node **apNode;` |
|  3054363 | 16772 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6108815 | 16773 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3054457 | 16774 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16775 | `				/* nullify the entry */` |
|  3054363 | 16776 | `				apNode[n] = 0;` |
|  1527179 | 16777 | `			}` |
|  1527231 | 16778 | `		}` |
|  1527179 | 16779 | `	}` |
|  3054435 | 16780 | `	return SXRET_OK;` |
|  1580552 | 16781 |  |
|        - | 16782 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16783 | `/*` |
|        - | 16784 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16785 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16786 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16787 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16788 | ` * For more information on how to register IO stream devices,please` |
|        - | 16789 | ` * refer to the official documentation.` |
|        - | 16790 | ` */` |
|    29488 | 16791 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16792 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16793 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16794 | `	int nByte              /* *pzDevice length*/` |
|        - | 16795 | `	)` |
|        5 | 16796 |  |
|        - | 16797 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16798 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16799 | `	SyString sDev,sCur;` |
|        - | 16800 | `	sxu32 n,nEntry;` |
|        - | 16801 | `	int rc;` |
|        - | 16802 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29493 | 16803 | `	zNext = zCur = zIn = *pzDevice;` |
|    29493 | 16804 | `	zEnd = &zIn[nByte];` |
|  1882742 | 16805 | `	while( zIn < zEnd ){` |
|  1853256 | 16806 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16807 | `			/* Got one */` |
|        3 | 16808 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16809 | `			break;` |
|        - | 16810 | `		}` |
|        - | 16811 | `		/* Advance the cursor */` |
|  1853254 | 16812 | `		zIn++;` |
|        5 | 16813 | `	}` |
|    29493 | 16814 | `	if( zIn >= zEnd ){` |
|        - | 16815 | `		/* No such scheme,return the default stream */` |
|    29491 | 16816 | `		return pVm->pDefStream;` |
|        - | 16817 | `	}` |
|        3 | 16818 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16819 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16820 | `	SyStringFullTrim(&sDev);` |
|        - | 16821 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16822 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16823 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16824 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16825 | `		pStream = apStream[n];` |
|        3 | 16826 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16827 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16828 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16829 | `		if( rc == 0 ){` |
|        - | 16830 | `			/* Stream device found */` |
|        3 | 16831 | `			*pzDevice = zNext;` |
|        3 | 16832 | `			return pStream;` |
|        - | 16833 | `		}` |
|      ! 0 | 16834 | `	}` |
|        - | 16835 | `	/* No such stream,return NULL */` |
|      ! 0 | 16836 | `	return 0;` |
|    14749 | 16837 |  |
|        - | 16838 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16839 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16840 |  |
